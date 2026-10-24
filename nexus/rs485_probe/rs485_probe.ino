// RS485 wiring probe for the Nexus IO Expansion Shield (not the robot program).
// Once a second it sends "HELLO <n> rx=<bytes>" on the RS485 bus, where rx is how many bytes
// it has received so far. Watching it from the M5 tests Nexus -> M5; a growing rx tests M5 -> Nexus.
// Pin 13 selects the direction (HIGH = transmit, LOW = receive), as in the SONAR library.

#define PIN_RS485_DIR 13
#define BAUD 19200

static unsigned long rxBytes = 0;
static unsigned long lastMs = 0;
static unsigned int n = 0;

void setup() {
	Serial.begin(BAUD);
	pinMode(PIN_RS485_DIR, OUTPUT);
	digitalWrite(PIN_RS485_DIR, LOW);
}

void loop() {
	while (Serial.available()) {
		Serial.read();
		rxBytes++;
	}
	if (millis() - lastMs >= 1000) {
		lastMs = millis();
		digitalWrite(PIN_RS485_DIR, HIGH);
		Serial.print(F("HELLO "));
		Serial.print(n++);
		Serial.print(F(" rx="));
		Serial.println(rxBytes);
		Serial.flush();
		digitalWrite(PIN_RS485_DIR, LOW);
	}
}
