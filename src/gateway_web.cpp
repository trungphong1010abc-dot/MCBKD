#include "gateway_web.h"

#include <Arduino.h>
#include <HTTPClient.h>
#include <Update.h>
#include <WebServer.h>
#include <WiFi.h>

#include "gateway_logic.h"
#include "gateway_state.h"
#include "packet_protocol.h"
#include "project_config.h"

static WebServer server(80);
static WiFiServer debugServer(8080);
static TaskHandle_t webTaskHandle = nullptr;

static void handleDebugServer();

void connectWiFi()
{
    if (String(Config::WifiSsid) == "TEN_WIFI_CUA_BAN")
    {
        Serial.println("WiFi disabled: set WifiSsid/WifiPassword in project_config.h");
        return;
    }

    WiFi.mode(WIFI_STA);
    WiFi.setSleep(false);
    WiFi.begin(Config::WifiSsid, Config::WifiPassword);
    Serial.print("Connecting WiFi");
    const uint32_t start = millis();
    while (WiFi.status() != WL_CONNECTED && millis() - start < 15000)
    {
        Serial.print(".");
        delay(500);
    }
    Serial.println();
    if (WiFi.status() == WL_CONNECTED)
    {
        Serial.print("Gateway IP: ");
        Serial.println(WiFi.localIP());
    }
}

static String csvHistory()
{
    String csv = "time_ms,node_id,packet_id,t_air,h_air,h_soil,v_bat,adc,soil_status,error_flag,rssi,snr,sleep_min,soil_threshold,filter_mode,pump_time,control_mode,duty_cycle\n";
    for (size_t i = 0; i < historyCount; i++)
    {
        const size_t idx = (historyWriteIndex + kGatewayHistorySize - historyCount + i) % kGatewayHistorySize;
        const StoredTelemetry &item = history[idx];
        csv += String(item.receivedMs) + "," + item.packet.nodeId + "," + item.packet.packetId + ",";
        csv += String(item.packet.temperatureC, 1) + "," + String(item.packet.humidityRh, 1) + ",";
        csv += String(item.packet.soilMoistureVol, 1) + "," + String(item.packet.batteryV, 2) + ",";
        csv += String(item.packet.adcFiltered) + "," + item.packet.soilStatus + ",";
        csv += String(item.packet.errorFlag) + "," + String(item.rssi) + "," + String(item.snr, 1) + ",";
        csv += String(item.packet.configSleepMinutes) + "," + String(item.packet.configSoilThresholdVol) + ",";
        csv += String(item.packet.configFilterMode) + "," + String(item.packet.configPumpSeconds) + ",";
        csv += String(item.packet.configControlMode) + "," + String(item.packet.configDutyCycleMode) + "\n";
    }
    return csv;
}

static void handleRoot()
{
    if (Config::EnableGatewayHttpDebug)
    {
        Serial.println("HTTP GET /");
    }
    String html = "<!doctype html><html><head><meta charset='utf-8'><meta name='viewport' content='width=device-width,initial-scale=1'>";
    html += "<title>EE4552 Gateway</title><style>body{font-family:Arial;margin:24px}table{border-collapse:collapse;width:100%}td,th{border:1px solid #ccc;padding:6px;text-align:left}</style></head><body>";
    html += "<h2>EE4552 Gateway Dashboard</h2><p><a href='/export.csv'>Export CSV</a></p>";
    html += "<p>Firmware: " + String(Config::GatewayFirmwareVersion) + "</p>";
    html += "<p>Queue command: /command?node=1&amp;cmd=SET_THRESHOLD&amp;param=20 or SET_FILTER_MODE, SET_PUMP_TIME, SET_CONTROL_MODE, START_OTA</p>";
    html += "<p>Gateway OTA: /self_ota?url=http://server/firmware.bin&amp;md5=optional_md5</p>";
    html += "<table><tr><th>ms</th><th>Node</th><th>PID</th><th>T</th><th>H_air</th><th>H_soil</th><th>Vbat</th><th>Status</th><th>RSSI</th><th>Sleep</th><th>Threshold</th><th>Pump</th><th>Control</th><th>Duty</th></tr>";
    for (size_t i = 0; i < historyCount; i++)
    {
        const size_t idx = (historyWriteIndex + kGatewayHistorySize - historyCount + i) % kGatewayHistorySize;
        const StoredTelemetry &item = history[idx];
        html += "<tr><td>" + String(item.receivedMs) + "</td><td>" + String(item.packet.nodeId) + "</td><td>" + String(item.packet.packetId) + "</td>";
        html += "<td>" + String(item.packet.temperatureC, 1) + "</td><td>" + String(item.packet.humidityRh, 1) + "</td><td>" + String(item.packet.soilMoistureVol, 1) + "</td>";
        html += "<td>" + String(item.packet.batteryV, 2) + "</td><td>" + alertLevel(item.packet) + "</td><td>" + String(item.rssi) + "</td>";
        html += "<td>" + String(item.packet.configSleepMinutes) + "</td><td>" + String(item.packet.configSoilThresholdVol) + "</td>";
        html += "<td>" + String(item.packet.configPumpSeconds) + "</td><td>" + String(item.packet.configControlMode) + "</td>";
        html += "<td>" + String(item.packet.configDutyCycleMode) + "</td></tr>";
    }
    html += "</table></body></html>";
    server.send(200, "text/html", html);
}

static void handleExport()
{
    if (Config::EnableGatewayHttpDebug)
    {
        Serial.println("HTTP GET /export.csv");
    }
    server.sendHeader("Content-Disposition", "attachment; filename=ee4552_history.csv");
    server.send(200, "text/csv", csvHistory());
}

static void handleCommand()
{
    if (Config::EnableGatewayHttpDebug)
    {
        Serial.println("HTTP GET /command");
    }
    const uint8_t nodeId = uint8_t(server.arg("node").toInt());
    const CommandType command = commandFromText(server.arg("cmd"));
    int parameter = parseCommandParameter(command, server.arg("param"));

    if (nodeId == 0 || command == CommandType::None)
    {
        server.send(400, "text/plain", "invalid node/cmd");
        return;
    }

    if (command == CommandType::StartOta && parameter == 0)
    {
        parameter = int(millis() % 100000UL) + 1;
    }
    queueGatewayCommand(nodeId, command, parameter);

    Serial.printf("Queued HTTP command node=%u cmd=%s param=%d\n",
                  nodeId, commandToText(command).c_str(), parameter);

    server.send(200, "text/plain", "queued");
}

static void handleSelfOta()
{
    if (Config::EnableGatewayHttpDebug)
    {
        Serial.println("HTTP GET /self_ota");
    }
    if (WiFi.status() != WL_CONNECTED)
    {
        server.send(503, "text/plain", "wifi not connected");
        return;
    }

    const String url = server.arg("url");
    const String md5 = server.arg("md5");
    if (!url.startsWith("http://") && !url.startsWith("https://"))
    {
        server.send(400, "text/plain", "missing url");
        return;
    }

    server.send(200, "text/plain", "OTA started; gateway will reboot on success");
    delay(250);

    WiFiClient client;
    HTTPClient http;
    http.setTimeout(4000);
    http.begin(client, url);
    const int status = http.GET();
    if (status != HTTP_CODE_OK)
    {
        Serial.printf("OTA_FAILED http_status=%d\n", status);
        http.end();
        return;
    }

    const int contentLength = http.getSize();
    if (contentLength <= 0 || !Update.begin(contentLength))
    {
        Serial.printf("OTA_FAILED begin_error=%s size=%d\n", Update.errorString(), contentLength);
        http.end();
        return;
    }
    if (md5.length() > 0)
    {
        Update.setMD5(md5.c_str());
    }

    const size_t written = Update.writeStream(http.getStream());
    if (written != size_t(contentLength))
    {
        Serial.printf("OTA_FAILED written=%u expected=%d\n", unsigned(written), contentLength);
        Update.abort();
        http.end();
        return;
    }
    if (!Update.end(true))
    {
        Serial.printf("OTA_FAILED end_error=%s\n", Update.errorString());
        http.end();
        return;
    }

    Serial.printf("OTA_SUCCESS bytes=%u md5=%s\n", unsigned(written), md5.length() > 0 ? md5.c_str() : "not_provided");
    http.end();
    delay(250);
    ESP.restart();
}

static void handlePing()
{
    if (Config::EnableGatewayHttpDebug)
    {
        Serial.println("HTTP GET /ping");
    }
    server.send(200, "text/plain", "pong");
}

static void handleNotFound()
{
    if (Config::EnableGatewayHttpDebug)
    {
        Serial.print("HTTP 404 ");
        Serial.println(server.uri());
    }
    server.send(404, "text/plain", "not found");
}

void setupWebServer()
{
    server.on("/", handleRoot);
    server.on("/ping", handlePing);
    server.on("/export.csv", handleExport);
    server.on("/command", handleCommand);
    server.on("/self_ota", handleSelfOta);
    server.onNotFound(handleNotFound);
    server.begin();
    Serial.println("HTTP server listening on port 80");
    debugServer.begin();
    Serial.println("Debug HTTP server listening on port 8080");
}

static void webServerTask(void *)
{
    while (true)
    {
        handleDebugServer();
        server.handleClient();
        delay(2);
    }
}

void startWebServerTask()
{
    if (webTaskHandle != nullptr)
    {
        return;
    }

    xTaskCreatePinnedToCore(
        webServerTask,
        "web_server",
        8192,
        nullptr,
        1,
        &webTaskHandle,
        0);
}

static void handleDebugServer()
{
    WiFiClient client = debugServer.available();
    if (!client)
    {
        return;
    }

    const uint32_t start = millis();
    while (client.connected() && !client.available() && millis() - start < 1000)
    {
        delay(1);
    }

    String requestLine;
    if (client.available())
    {
        requestLine = client.readStringUntil('\n');
        requestLine.trim();
    }

    if (Config::EnableGatewayHttpDebug)
    {
        Serial.print("DEBUG HTTP request: ");
        Serial.println(requestLine.length() > 0 ? requestLine : "(empty)");
    }

    client.print("HTTP/1.1 200 OK\r\n");
    client.print("Content-Type: text/plain\r\n");
    client.print("Connection: close\r\n");
    client.print("Content-Length: 4\r\n\r\n");
    client.print("pong");
    client.stop();
}
