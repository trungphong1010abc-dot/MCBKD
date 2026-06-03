#include <Arduino.h>
#include <WiFi.h>

#include "gateway_lora.h"
#include "gateway_state.h"
#include "gateway_web.h"
#include "project_config.h"

void setup()
{
    Serial.begin(Config::SerialBaud);
    delay(1000);
    Serial.println("===== EE4552 GATEWAY FIRMWARE =====");
    Serial.print("Firmware: ");
    Serial.println(Config::GatewayFirmwareVersion);

    loraReady = initLoRa();
    if (!loraReady)
    {
        Serial.println("LoRa init failed; WiFi/web will continue");
    }
    else
    {
        Serial.println("LoRa init OK");
    }

    connectWiFi();
    setupWebServer();
    startWebServerTask();
    Serial.println("Gateway ready");
}

void loop()
{
    if (otaReadyWaitActive && int32_t(millis() - otaReadyWaitDeadlineMs) > 0)
    {
        otaReadyWaitActive = false;
        Serial.println("OTA session failed: node did not report OTA_READY");
    }

    if (Config::EnableGatewayHeartbeatLog && millis() - lastHeartbeatMs >= 2000)
    {
        lastHeartbeatMs = millis();
        Serial.printf("Loop alive wifi=%d ip=%s\n", int(WiFi.status()), WiFi.localIP().toString().c_str());
    }

    processLoRa();
    processSerialCommand();
    delay(1);
}
