// Copy to config.h (git-ignored) and fill in.
#pragma once

// Nexus UART (both modes)
#define NEXUS_BAUD  19200
#define NEXUS_RX    16   // M5 RX <- Nexus D1 (level shift 5V -> 3.3V)
#define NEXUS_TX    17   // M5 TX -> Nexus D0

// MQTT mode only
#define WIFI_SSID   "your-ssid"
#define WIFI_PASS   "your-password"
#define MQTT_HOST   "192.168.0.10"
#define MQTT_PORT   1883
#define MQTT_CLIENT "starrobox-m5"
#define TOPIC_CMD    "starrobox/robot/cmd"
#define TOPIC_STATUS "starrobox/robot/status"

// BLE mode only
#define BLE_NAME    "StarroBox"
