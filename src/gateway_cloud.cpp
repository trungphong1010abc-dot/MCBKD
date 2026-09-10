#include "gateway_cloud.h"

#include <Arduino.h>
#include <HTTPClient.h>
#include <WiFi.h>

#include "gateway_logic.h"
#include "project_config.h"

void uploadThingsBoard(const TelemetryPacket &packet, int rssi)
{
    if (!Config::EnableCloudUpload || WiFi.status() != WL_CONNECTED ||
        String(Config::ThingsBoardToken).length() == 0)
    {
        return;
    }

    HTTPClient http;
    http.setTimeout(1500);
    const String url = String("http://") + Config::ThingsBoardHost + ":" +
                       String(Config::ThingsBoardHttpPort) + "/api/v1/" +
                       Config::ThingsBoardToken + "/telemetry";
    http.begin(url);
    http.addHeader("Content-Type", "application/json");
    const String body = String("{\"Node_ID\":") + packet.nodeId +
                        ",\"T_air\":" + String(packet.temperatureC, 1) +
                        ",\"H_air\":" + String(packet.humidityRh, 1) +
                        ",\"H_soil\":" + String(packet.soilMoistureVol, 0) +
                        ",\"RSSI\":" + String(rssi) +
                        ",\"Error_Flag\":" + String(packet.errorFlag) +
                        ",\"Alert\":\"" + alertLevel(packet) + "\"" +
                        ",\"PumpTime\":" + String(computePumpTime(packet)) +
                        ",\"Sleep_Min\":" + String(packet.configSleepMinutes) +
                        ",\"Soil_Threshold\":" + String(packet.configSoilThresholdVol) +
                        ",\"Filter_Mode\":" + String(packet.configFilterMode) +
                        ",\"Config_Pump_Time\":" + String(packet.configPumpSeconds) +
                        ",\"Control_Mode\":" + String(packet.configControlMode) +
                        ",\"Duty_Cycle\":" + String(packet.configDutyCycleMode) + "}";
    const int status = http.POST(body);
    Serial.printf("ThingsBoard HTTP status: %d\n", status);
    http.end();
}
