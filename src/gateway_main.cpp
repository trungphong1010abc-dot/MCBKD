#include <Arduino.h>
#include <HTTPClient.h>
#include <HTTPUpdate.h>
#include <LoRa.h>
#include <SPI.h>
#include <Update.h>
#include <WebServer.h>
#include <WiFi.h>

#include "packet_protocol.h"
#include "project_config.h"

struct StoredTelemetry
{
    TelemetryPacket packet;
    int rssi = 0;
    float snr = 0.0f;
    uint32_t receivedMs = 0;
};

static constexpr size_t kHistorySize = 64;
static StoredTelemetry history[kHistorySize];
static size_t historyCount = 0;
static size_t historyWriteIndex = 0;
static uint32_t lastPacketIdByNode[256] = {};
static AckPacket pendingCommands[256];
static WebServer server(80);

static int parseCommandParameter(CommandType command, String value)
{
    value.trim();
    value.toUpperCase();
    if (command == CommandType::SetFilterMode)
    {
        return value == "MEDIAN" ? 1 : 0;
    }
    if (command == CommandType::SetControlMode)
    {
        return value == "AUTO" ? 1 : 0;
    }
    if (command == CommandType::SetDutyCycle)
    {
        return value == "ADAPTIVE" ? 1 : 0;
    }
    return value.toInt();
}

static bool initLoRa()
{
    SPI.begin(Config::LoraSck, Config::LoraMiso, Config::LoraMosi, Config::LoraSs);
    pinMode(Config::LoraRst, OUTPUT);
    digitalWrite(Config::LoraRst, LOW);
    delay(20);
    digitalWrite(Config::LoraRst, HIGH);
    delay(100);

    LoRa.setPins(Config::LoraSs, Config::LoraRst, Config::LoraDio0);
    if (!LoRa.begin(Config::LoraFrequency))
    {
        return false;
    }
    LoRa.setSpreadingFactor(Config::LoraSpreadingFactor);
    LoRa.setSignalBandwidth(Config::LoraSignalBandwidth);
    LoRa.setCodingRate4(Config::LoraCodingRate);
    LoRa.setTxPower(Config::LoraTxPowerDbm);
    LoRa.enableCrc();
    return true;
}

static void connectWiFi()
{
    if (String(Config::WifiSsid) == "TEN_WIFI_CUA_BAN")
    {
        Serial.println("WiFi disabled: set WifiSsid/WifiPassword in project_config.h");
        return;
    }

    WiFi.mode(WIFI_STA);
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

static void storeTelemetry(const TelemetryPacket &packet, int rssi, float snr)
{
    history[historyWriteIndex].packet = packet;
    history[historyWriteIndex].rssi = rssi;
    history[historyWriteIndex].snr = snr;
    history[historyWriteIndex].receivedMs = millis();
    historyWriteIndex = (historyWriteIndex + 1) % kHistorySize;
    if (historyCount < kHistorySize)
    {
        historyCount++;
    }
}

static int computePumpTime(const TelemetryPacket &packet)
{
    if (packet.errorFlag != 0)
    {
        return 0;
    }

    int pumpTime = 0;
    if (packet.soilMoistureVol > 42.0f)
    {
        pumpTime = 0;
    }
    else if (packet.soilMoistureVol > 33.0f)
    {
        pumpTime = 0;
    }
    else if (packet.soilMoistureVol > 24.0f)
    {
        pumpTime = 3;
    }
    else if (packet.soilMoistureVol > 15.0f)
    {
        pumpTime = 6;
    }
    else
    {
        pumpTime = 10;
    }

    if (packet.temperatureC > 32.0f)
    {
        pumpTime += 2;
    }
    if (packet.humidityRh < 50.0f)
    {
        pumpTime += 3;
    }
    if (packet.humidityRh > 80.0f)
    {
        pumpTime -= 2;
    }
    return constrain(pumpTime, 0, 15);
}

static String alertLevel(const TelemetryPacket &packet)
{
    if (packet.errorFlag != 0)
    {
        return "SENSOR_ERROR";
    }
    if (packet.soilMoistureVol > 42.0f)
    {
        return "OVER_MOISTURE";
    }
    if (packet.soilMoistureVol > 33.0f)
    {
        return "NORMAL";
    }
    if (packet.soilMoistureVol > 24.0f)
    {
        return "LIGHT_DRY";
    }
    if (packet.soilMoistureVol > 15.0f)
    {
        return "NEED_WATERING";
    }
    return "URGENT_WATERING";
}

static void uploadThingsBoard(const TelemetryPacket &packet, int rssi)
{
    if (!Config::EnableCloudUpload || WiFi.status() != WL_CONNECTED ||
        String(Config::ThingsBoardToken).length() == 0)
    {
        return;
    }

    HTTPClient http;
    const String url = String("http://") + Config::ThingsBoardHost + ":" +
                       String(Config::ThingsBoardHttpPort) + "/api/v1/" +
                       Config::ThingsBoardToken + "/telemetry";
    http.begin(url);
    http.addHeader("Content-Type", "application/json");
    const String body = String("{\"Node_ID\":") + packet.nodeId +
                        ",\"T_air\":" + String(packet.temperatureC, 1) +
                        ",\"H_air\":" + String(packet.humidityRh, 1) +
                        ",\"H_soil\":" + String(packet.soilMoistureVol, 0) +
                        ",\"V_bat\":" + String(packet.batteryV, 2) +
                        ",\"RSSI\":" + String(rssi) +
                        ",\"Error_Flag\":" + String(packet.errorFlag) +
                        ",\"Alert\":\"" + alertLevel(packet) + "\"" +
                        ",\"PumpTime\":" + String(computePumpTime(packet)) + "}";
    const int status = http.POST(body);
    Serial.printf("ThingsBoard HTTP status: %d\n", status);
    http.end();
}

static bool waitForOtaStatus(uint8_t nodeId, uint32_t otaId, uint16_t chunkIndex,
                             const String &expectedStatus, OtaStatusPacket &status)
{
    const uint32_t start = millis();
    while (millis() - start < 7000)
    {
        const int packetSize = LoRa.parsePacket();
        if (packetSize <= 0)
        {
            delay(10);
            continue;
        }

        String line;
        while (LoRa.available())
        {
            line += char(LoRa.read());
        }

        if (!decodeOtaStatus(line, status))
        {
            Serial.print("OTA status invalid: ");
            Serial.println(line);
            continue;
        }
        if (status.nodeId != nodeId || status.otaId != otaId)
        {
            Serial.println("OTA status ignored: node/ota mismatch");
            continue;
        }
        if (status.chunkIndex != chunkIndex && expectedStatus != "OTA_SUCCESS")
        {
            Serial.println("OTA status ignored: chunk mismatch");
            continue;
        }
        if (expectedStatus.length() > 0 && status.status != expectedStatus)
        {
            Serial.printf("OTA status unexpected: %s\n", status.status.c_str());
            return false;
        }
        return status.ok;
    }
    return false;
}

static bool sendOtaChunkWithRetry(uint8_t nodeId, uint32_t otaId, uint16_t index,
                                  uint16_t total, const String &data)
{
    OtaChunkPacket chunk;
    chunk.nodeId = nodeId;
    chunk.otaId = otaId;
    chunk.chunkIndex = index;
    chunk.totalChunks = total;
    chunk.payloadData = data;
    chunk.dataCrc = crc16Ccitt(data);

    const String payload = encodeOtaChunk(chunk);
    for (uint8_t retry = 0; retry < Config::MaxRetry; retry++)
    {
        Serial.printf("OTA CHUNK TX idx=%u try=%u: %s\n", index, retry + 1, payload.c_str());
        LoRa.beginPacket();
        LoRa.print(payload);
        LoRa.endPacket(true);
        delay(150);
        LoRa.receive();

        OtaStatusPacket status;
        if (waitForOtaStatus(nodeId, otaId, index, "ACK", status))
        {
            return true;
        }
        delay(300);
    }
    return false;
}

static void runSimulatedLoraOtaSession(uint8_t nodeId, uint32_t otaId)
{
    static const char *chunks[] = {
        "FW_SIM_001_BOOT_HDR",
        "FW_SIM_002_PARTITION",
        "FW_SIM_003_APP_TEXT",
        "FW_SIM_004_APP_DATA",
        "FW_SIM_005_DRIVER",
        "FW_SIM_006_SENSOR",
        "FW_SIM_007_LORA",
        "FW_SIM_008_CONFIG",
        "FW_SIM_009_CHECKSUM",
        "FW_SIM_010_END"};

    Serial.printf("OTA session start node=%u ota_id=%lu\n", nodeId, otaId);
    LoRa.receive();
    OtaStatusPacket ready;
    if (!waitForOtaStatus(nodeId, otaId, 0, "OTA_READY", ready))
    {
        Serial.println("OTA session failed: node did not report OTA_READY");
        return;
    }

    constexpr uint16_t totalChunks = sizeof(chunks) / sizeof(chunks[0]);
    for (uint16_t i = 0; i < totalChunks; i++)
    {
        if (!sendOtaChunkWithRetry(nodeId, otaId, i + 1, totalChunks, chunks[i]))
        {
            Serial.printf("OTA session failed at chunk %u\n", i + 1);
            return;
        }
    }

    OtaStatusPacket done;
    if (waitForOtaStatus(nodeId, otaId, totalChunks, "OTA_SUCCESS", done))
    {
        Serial.printf("OTA session success node=%u ota_id=%lu chunks=%u\n", nodeId, otaId, totalChunks);
    }
    else
    {
        Serial.println("OTA session finished chunks but final OTA_SUCCESS not received");
    }
}

static void sendAckFor(const TelemetryPacket &packet)
{
    AckPacket ack;
    ack.nodeId = packet.nodeId;
    ack.packetId = packet.packetId;
    ack.ok = packet.errorFlag == 0;
    ack.status = ack.ok ? "OK" : "SENSOR_ERROR";

    const CommandType queuedCommand = pendingCommands[packet.nodeId].command;
    if (queuedCommand != CommandType::None)
    {
        ack.command = queuedCommand;
        ack.parameter = pendingCommands[packet.nodeId].parameter;
        pendingCommands[packet.nodeId].command = CommandType::None;
        pendingCommands[packet.nodeId].parameter = 0;
        ack.status = "COMMAND";
    }
    else if (Config::EnablePumpHardware)
    {
        const int pumpTime = computePumpTime(packet);
        if (pumpTime > 0)
        {
            ack.command = CommandType::StartPump;
            ack.parameter = pumpTime;
        }
    }

    const String payload = encodeAck(ack);
    LoRa.beginPacket();
    LoRa.print(payload);
    LoRa.endPacket(true);
    delay(100);
    LoRa.receive();
    Serial.print("ACK TX: ");
    Serial.println(payload);

    if (queuedCommand == CommandType::StartOta)
    {
        delay(500);
        runSimulatedLoraOtaSession(packet.nodeId, uint32_t(ack.parameter));
    }
}

static String csvHistory()
{
    String csv = "time_ms,node_id,packet_id,t_air,h_air,h_soil,v_bat,adc,soil_status,error_flag,rssi,snr\n";
    for (size_t i = 0; i < historyCount; i++)
    {
        const size_t idx = (historyWriteIndex + kHistorySize - historyCount + i) % kHistorySize;
        const StoredTelemetry &item = history[idx];
        csv += String(item.receivedMs) + "," + item.packet.nodeId + "," + item.packet.packetId + ",";
        csv += String(item.packet.temperatureC, 1) + "," + String(item.packet.humidityRh, 1) + ",";
        csv += String(item.packet.soilMoistureVol, 1) + "," + String(item.packet.batteryV, 2) + ",";
        csv += String(item.packet.adcFiltered) + "," + item.packet.soilStatus + ",";
        csv += String(item.packet.errorFlag) + "," + String(item.rssi) + "," + String(item.snr, 1) + "\n";
    }
    return csv;
}

static void handleRoot()
{
    String html = "<!doctype html><html><head><meta charset='utf-8'><meta name='viewport' content='width=device-width,initial-scale=1'>";
    html += "<title>EE4552 Gateway</title><style>body{font-family:Arial;margin:24px}table{border-collapse:collapse;width:100%}td,th{border:1px solid #ccc;padding:6px;text-align:left}</style></head><body>";
    html += "<h2>EE4552 Gateway Dashboard</h2><p><a href='/export.csv'>Export CSV</a></p>";
    html += "<p>Firmware: " + String(Config::GatewayFirmwareVersion) + "</p>";
    html += "<p>Queue command: /command?node=1&cmd=SET_SLEEP_DURATION&param=10 or SET_THRESHOLD, SET_FILTER_MODE, SET_PUMP_TIME, SET_CONTROL_MODE, SET_DUTY_CYCLE, START_OTA</p>";
    html += "<p>Gateway OTA: /self_ota?url=http://server/firmware.bin&md5=optional_md5</p>";
    html += "<table><tr><th>ms</th><th>Node</th><th>PID</th><th>T</th><th>H_air</th><th>H_soil</th><th>Vbat</th><th>Status</th><th>RSSI</th></tr>";
    for (size_t i = 0; i < historyCount; i++)
    {
        const size_t idx = (historyWriteIndex + kHistorySize - historyCount + i) % kHistorySize;
        const StoredTelemetry &item = history[idx];
        html += "<tr><td>" + String(item.receivedMs) + "</td><td>" + String(item.packet.nodeId) + "</td><td>" + String(item.packet.packetId) + "</td>";
        html += "<td>" + String(item.packet.temperatureC, 1) + "</td><td>" + String(item.packet.humidityRh, 1) + "</td><td>" + String(item.packet.soilMoistureVol, 1) + "</td>";
        html += "<td>" + String(item.packet.batteryV, 2) + "</td><td>" + alertLevel(item.packet) + "</td><td>" + String(item.rssi) + "</td></tr>";
    }
    html += "</table></body></html>";
    server.send(200, "text/html", html);
}

static void handleExport()
{
    server.sendHeader("Content-Disposition", "attachment; filename=ee4552_history.csv");
    server.send(200, "text/csv", csvHistory());
}

static void handleCommand()
{
    const uint8_t nodeId = uint8_t(server.arg("node").toInt());
    const CommandType command = commandFromText(server.arg("cmd"));
    int parameter = parseCommandParameter(command, server.arg("param"));

    if (nodeId == 0 || command == CommandType::None)
    {
        server.send(400, "text/plain", "invalid node/cmd");
        return;
    }

    pendingCommands[nodeId].nodeId = nodeId;
    pendingCommands[nodeId].command = command;
    if (command == CommandType::StartOta && parameter == 0)
    {
        parameter = int(millis() % 100000UL) + 1;
    }
    pendingCommands[nodeId].parameter = parameter;
    pendingCommands[nodeId].ok = true;
    pendingCommands[nodeId].status = "QUEUED";

    server.send(200, "text/plain", "queued");
}

static void handleSelfOta()
{
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

static void setupWebServer()
{
    server.on("/", handleRoot);
    server.on("/export.csv", handleExport);
    server.on("/command", handleCommand);
    server.on("/self_ota", handleSelfOta);
    server.begin();
}

static void processLoRa()
{
    const int packetSize = LoRa.parsePacket();
    if (packetSize <= 0)
    {
        return;
    }

    String line;
    while (LoRa.available())
    {
        line += char(LoRa.read());
    }

    TelemetryPacket packet;
    if (!decodeTelemetry(line, packet))
    {
        Serial.print("Invalid RF packet: ");
        Serial.println(line);
        return;
    }

    if (lastPacketIdByNode[packet.nodeId] == packet.packetId)
    {
        Serial.printf("Duplicate packet ignored: node=%u pid=%lu\n", packet.nodeId, packet.packetId);
        sendAckFor(packet);
        return;
    }
    lastPacketIdByNode[packet.nodeId] = packet.packetId;

    const int rssi = LoRa.packetRssi();
    const float snr = LoRa.packetSnr();
    storeTelemetry(packet, rssi, snr);

    Serial.printf("DATA RX node=%u pid=%lu T=%.1f H=%.1f Soil=%.0f ADC=%d V=%.2f RSSI=%d SNR=%.1f Alert=%s PumpTime=%d\n",
                  packet.nodeId, packet.packetId, packet.temperatureC, packet.humidityRh,
                  packet.soilMoistureVol, packet.adcFiltered, packet.batteryV, rssi, snr,
                  alertLevel(packet).c_str(), computePumpTime(packet));

    uploadThingsBoard(packet, rssi);
    sendAckFor(packet);
}

static void processSerialCommand()
{
    if (!Serial.available())
    {
        return;
    }

    String line = Serial.readStringUntil('\n');
    line.trim();
    line.toUpperCase();

    const int firstSpace = line.indexOf(' ');
    const int secondSpace = line.indexOf(' ', firstSpace + 1);
    if (firstSpace < 0)
    {
        Serial.println("Serial command format: NODE_ID CMD PARAM");
        return;
    }

    const uint8_t nodeId = uint8_t(line.substring(0, firstSpace).toInt());
    const String cmdText = secondSpace < 0 ? line.substring(firstSpace + 1)
                                          : line.substring(firstSpace + 1, secondSpace);
    const int parameter = secondSpace < 0 ? 0 : line.substring(secondSpace + 1).toInt();
    const CommandType command = commandFromText(cmdText);
    if (nodeId == 0 || command == CommandType::None)
    {
        Serial.println("Invalid serial command. Example: 1 START_PUMP 5");
        return;
    }

    pendingCommands[nodeId].nodeId = nodeId;
    pendingCommands[nodeId].command = command;
    pendingCommands[nodeId].parameter = parameter;
    pendingCommands[nodeId].ok = true;
    pendingCommands[nodeId].status = "QUEUED";
    Serial.printf("Queued command for node %u: %s %d\n", nodeId, cmdText.c_str(), parameter);
}

void setup()
{
    Serial.begin(Config::SerialBaud);
    delay(1000);
    Serial.println("===== EE4552 GATEWAY FIRMWARE =====");
    Serial.print("Firmware: ");
    Serial.println(Config::GatewayFirmwareVersion);

    if (!initLoRa())
    {
        Serial.println("LoRa init failed");
        while (true)
        {
            delay(1000);
        }
    }
    Serial.println("LoRa init OK");

    connectWiFi();
    setupWebServer();
    Serial.println("Gateway ready");
}

void loop()
{
    processLoRa();
    processSerialCommand();
    server.handleClient();
}
