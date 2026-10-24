// M5 bridge between a wireless link and the Nexus board (over RS485). One mode per build:
//   MODE_MQTT: commands arrive over WiFi/MQTT from the PC
//   MODE_BLE:  commands arrive over BLE (Nordic UART Service) from a phone
// Replies from the Nexus go back over the same link.
#include <M5Unified.h>
#include "config.h"

#if defined(MODE_MQTT) == defined(MODE_BLE)
#error "Define exactly one of MODE_MQTT or MODE_BLE (use pio run -e mqtt / -e ble)"
#endif

#ifdef MODE_MQTT
#include <WiFi.h>
#include <PubSubClient.h>
#else
#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLE2902.h>
#endif

#define MAX_CMD 46

static String rxLine;

// Atomic RS485 Base on the AtomS3: RX = G5, TX = G6 (same as the reference project).
static HardwareSerial nexusUart(1);

// AtomS3 display is 128x128; status on the first line, last command on the third.
static void show(const char* msg) {
	M5.Display.fillRect(0, 0, 128, 16, BLACK);
	M5.Display.setCursor(2, 2);
	M5.Display.print(msg);
}

static void showCmd(const String& cmd) {
	M5.Display.fillRect(0, 40, 128, 16, BLACK);
	M5.Display.setCursor(2, 40);
	M5.Display.print(cmd);
}

// The Nexus is on a half-duplex RS485 bus (nexusUart -> Atomic RS485 Base, auto-direction).
// Keep writes apart so a command does not collide with a Nexus reply on the bus.
#define BUS_GAP_MS 10
static unsigned long lastBusMs = 0;

// Send one command line to the Nexus.
static void toNexus(const char* s, size_t len) {
	if (len == 0 || len > MAX_CMD) return;
	while (millis() - lastBusMs < BUS_GAP_MS) delay(1);
	nexusUart.write((const uint8_t*)s, len);
	nexusUart.write('\n');
	lastBusMs = millis();
	showCmd(String(s).substring(0, len));
}

// The RS485 unit hears its own transmissions, so only Nexus replies are forwarded.
static bool isNexusReply(const String& l) {
	return l == "PONG" || l == "OK" || l.startsWith("ERR");
}

#ifdef MODE_MQTT

static WiFiClient net;
static PubSubClient mqtt(net);
static unsigned long lastMqttTry = 0;
static unsigned long retryMs = 1000;

static void onMessage(char* topic, byte* payload, unsigned int len) {
	toNexus((const char*)payload, len);
}

static void connectWifi() {
	if (WiFi.status() == WL_CONNECTED) return;
	show("WiFi...");
	WiFi.begin(WIFI_SSID, WIFI_PASS);
	for (int i = 0; i < 40 && WiFi.status() != WL_CONNECTED; i++) delay(250);
}

static void connectMqtt() {
	if (mqtt.connected() || WiFi.status() != WL_CONNECTED) return;
	if (millis() - lastMqttTry < retryMs) return;
	lastMqttTry = millis();
	show("MQTT...");
	if (mqtt.connect(MQTT_CLIENT, TOPIC_STATUS, 0, true, "offline")) {
		mqtt.subscribe(TOPIC_CMD);
		mqtt.publish(TOPIC_STATUS, "online", true);
		retryMs = 1000;
		show("MQTT connected");
	} else {
		retryMs = min(retryMs * 2, 15000UL);
	}
}

static void linkSetup() {
	mqtt.setServer(MQTT_HOST, MQTT_PORT);
	mqtt.setCallback(onMessage);
}

static void linkLoop() {
	connectWifi();
	connectMqtt();
	mqtt.loop();
}

static void toLink(const String& reply) {
	if (mqtt.connected()) mqtt.publish(TOPIC_STATUS, reply.c_str());
}

#else  // MODE_BLE

// Nordic UART Service: the phone writes to RX, the M5 notifies on TX.
#define NUS_SERVICE "6E400001-B5A3-F393-E0A9-E50E24DCCA9E"
#define NUS_RX      "6E400002-B5A3-F393-E0A9-E50E24DCCA9E"
#define NUS_TX      "6E400003-B5A3-F393-E0A9-E50E24DCCA9E"

static BLEServer* server;
static BLECharacteristic* txChar;
static volatile bool connected = false;

class ServerCb : public BLEServerCallbacks {
	void onConnect(BLEServer*) override { connected = true; }
	void onDisconnect(BLEServer*) override {
		connected = false;
		toNexus("STOP", 4);               // never leave the robot running without a phone
		BLEDevice::startAdvertising();
	}
};

// A write holds one or more commands. A trailing command without '\n' counts too,
// so generic terminal apps work whether or not they append a newline.
class RxCb : public BLECharacteristicCallbacks {
	void onWrite(BLECharacteristic* c) override {
		const char* d = (const char*)c->getData();
		size_t n = c->getLength();
		size_t start = 0;
		for (size_t i = 0; i <= n; i++) {
			if (i == n || d[i] == '\n' || d[i] == '\r') {
				if (i > start) {
					char buf[MAX_CMD + 1];
					size_t len = min(i - start, (size_t)MAX_CMD);
					memcpy(buf, d + start, len);
					buf[len] = 0;
					toNexus(buf, len);
				}
				start = i + 1;
			}
		}
	}
};

static void linkSetup() {
	BLEDevice::init(BLE_NAME);
	server = BLEDevice::createServer();
	server->setCallbacks(new ServerCb());
	BLEService* svc = server->createService(NUS_SERVICE);
	txChar = svc->createCharacteristic(NUS_TX, BLECharacteristic::PROPERTY_NOTIFY);
	txChar->addDescriptor(new BLE2902());
	BLECharacteristic* rx = svc->createCharacteristic(
		NUS_RX, BLECharacteristic::PROPERTY_WRITE | BLECharacteristic::PROPERTY_WRITE_NR);
	rx->setCallbacks(new RxCb());
	svc->start();
	BLEAdvertising* adv = BLEDevice::getAdvertising();
	adv->addServiceUUID(NUS_SERVICE);
	adv->setScanResponse(true);
	BLEDevice::startAdvertising();
	show("BLE: " BLE_NAME);
}

static void linkLoop() {
	static bool shown = false;
	if (connected != shown) {
		shown = connected;
		show(connected ? "BLE connected" : "BLE: " BLE_NAME);
	}
}

static void toLink(const String& reply) {
	if (!connected) return;
	String out = reply + "\n";
	txChar->setValue((uint8_t*)out.c_str(), out.length());
	txChar->notify();
}

#endif

void setup() {
	auto cfg = M5.config();
	M5.begin(cfg);
	M5.Display.setTextSize(1);
#if defined(DEBUG_BUS) && defined(MODE_MQTT)
	// Pin probe: send PING in each RX/TX order and look for PONG from the Nexus.
	// Results are shown on the display; the order that answers (else the first) is used.
	int pins[2][2] = {{NEXUS_RX, NEXUS_TX}, {NEXUS_TX, NEXUS_RX}};
	int use = 0;
	bool found = false;
	for (int i = 0; i < 2; i++) {
		nexusUart.begin(NEXUS_BAUD, SERIAL_8N1, pins[i][0], pins[i][1]);
		delay(50);
		while (nexusUart.available()) nexusUart.read();
		String got;
		for (int tries = 0; tries < 3 && got.indexOf("PONG") < 0; tries++) {
			nexusUart.write("PING\n");
			nexusUart.flush();
			unsigned long t = millis();
			while (millis() - t < 300) {
				while (nexusUart.available()) got += (char)nexusUart.read();
			}
		}
		bool ok = got.indexOf("PONG") >= 0;
		M5.Display.setCursor(2, 90 + i * 12);
		M5.Display.printf("RX%d TX%d %s b%d", pins[i][0], pins[i][1], ok ? "PONG" : "none", (int)got.length());
		if (ok && !found) { use = i; found = true; }
		nexusUart.end();
	}
	nexusUart.begin(NEXUS_BAUD, SERIAL_8N1, pins[use][0], pins[use][1]);
#else
	nexusUart.begin(NEXUS_BAUD, SERIAL_8N1, NEXUS_RX, NEXUS_TX);
#endif
	linkSetup();
}

#if defined(DEBUG_BUS) && defined(MODE_MQTT)
// Debug build: publish every raw byte received from the RS485 bus, as hex, to <status>/rx.
static String dbgHex;
static unsigned long dbgLastMs = 0;
static unsigned long rxTotal = 0;
static void debugBus(uint8_t b) {
	char h[4];
	snprintf(h, sizeof h, "%02X ", b);
	dbgHex += h;
	rxTotal++;
}
static void debugFlush() {
	static unsigned long lastPing = 0;
	if (millis() - lastPing > 2000) {   // keep the bus busy so the Nexus probe can count bytes
		lastPing = millis();
		nexusUart.write("PING\n");
	}
	if (dbgHex.length() && millis() - dbgLastMs > 200 && mqtt.connected()) {
		mqtt.publish(TOPIC_STATUS "/rx", dbgHex.c_str());
		dbgHex = "";
		dbgLastMs = millis();
		M5.Display.fillRect(0, 70, 128, 16, BLACK);
		M5.Display.setCursor(2, 70);
		M5.Display.printf("rx bytes %lu", rxTotal);
	}
}
#else
#define debugBus(b)
#define debugFlush()
#endif

void loop() {
	linkLoop();
	debugFlush();

	while (nexusUart.available()) {
		char c = nexusUart.read();
		debugBus((uint8_t)c);
		if (c == '\r') continue;
		if (c == '\n') {
			if (isNexusReply(rxLine)) toLink(rxLine);
			rxLine = "";
		} else if (rxLine.length() < 64) {
			rxLine += c;
		}
	}
	M5.update();
}
