// Copy to config.h (git-ignored) and fill in.
#pragma once

// Nexus link over RS485 (both modes): AtomS3 + Atomic RS485 Base (auto-direction),
// A/B wired to the A/B terminals of the Nexus IO Expansion Shield.
#define NEXUS_BAUD  19200
#define NEXUS_RX    5    // AtomS3 RX <- RS485 base
#define NEXUS_TX    6    // AtomS3 TX -> RS485 base

// MQTT mode only
#define WIFI_SSID   "your-ssid"
#define WIFI_PASS   "your-password"
#define MQTT_HOST   "192.168.0.10"
#define MQTT_PORT   1883
#define MQTT_CLIENT "starrobox-m5"
#define TOPIC_CMD    "starrobox/robot/cmd"
#define TOPIC_STATUS "starrobox/robot/status"

// BLE mode only
#define BLE_NAME    "Starrobox"
