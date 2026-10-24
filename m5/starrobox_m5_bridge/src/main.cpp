// M5 bridge between a wireless link and the Nexus UART. One mode per build:
//   MODE_MQTT: commands arrive over WiFi/MQTT from the PC
//   MODE_BLE:  commands arrive over BLE (Nordic UART Service) from a phone
// Replies from the Nexus go back over the same link.
#include <M5Stack.h>
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

static void show(const char* msg) {
	M5.Lcd.fillRect(0, 0, 320, 40, BLACK);
	M5.Lcd.setCursor(4, 4);
	M5.Lcd.print(msg);
}

static void showCmd(const String& cmd) {
	M5.Lcd.fillRect(0, 60, 320, 40, BLACK);
	M5.Lcd.setCursor(4, 60);
	M5.Lcd.print(cmd);
}

// Send one command line to the Nexus.
static void toNexus(const char* s, size_t len) {
	if (len == 0 || len > MAX_CMD) return;
	Serial2.write((const uint8_t*)s, len);
	Serial2.write('\n');
	showCmd(String(s).substring(0, len));
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
		Serial2.write("STOP\n");          // never leave the robot running without a phone
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
	M5.begin();
	M5.Lcd.setTextSize(2);
	Serial2.begin(NEXUS_BAUD, SERIAL_8N1, NEXUS_RX, NEXUS_TX);
	linkSetup();
}

void loop() {
	linkLoop();

	while (Serial2.available()) {
		char c = Serial2.read();
		if (c == '\r') continue;
		if (c == '\n') {
			if (rxLine.length()) toLink(rxLine);
			rxLine = "";
		} else if (rxLine.length() < 64) {
			rxLine += c;
		}
	}
	M5.update();
}
