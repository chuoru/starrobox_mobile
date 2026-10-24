// StarroBox Nexus 4WD omni robot - serial command receiver.
// Commands arrive from the M5 over RS485 (IO Expansion Shield) on the hardware UART
// at 19200 8N1. RS485 is half-duplex: pin 13 selects receive (LOW) or transmit (HIGH).
// Fit the shield's RS485 jumper to run; remove it to upload or to use the USB cable.
// See README.md for the protocol.

#include <PinChangeInt.h>
#include <PinChangeIntConfig.h>
#include <EEPROM.h>
#include <fuzzy_table.h>
#include <PID_Beta6.h>
#include <MotorWheel.h>
#include <Omni4WD.h>

#define BAUD            19200
#define PIN_RS485_DIR   13      // HIGH = transmit, LOW = receive (same as the SONAR library)
#define RS485_TURNAROUND_MS 30  // default pause before replying so the M5's RS485 base has switched to receive (TURN <ms> changes it)
#define LINE_MAX        48
#define FAILSAFE_MS     1000UL
#define MAX_SPEED_MMPS  600

irqISR(irq1,isr1);
MotorWheel wheel1(3,2,4,5,&irq1);

irqISR(irq2,isr2);
MotorWheel wheel2(11,12,14,15,&irq2);

irqISR(irq3,isr3);
MotorWheel wheel3(9,8,16,17,&irq3);

irqISR(irq4,isr4);
MotorWheel wheel4(10,7,18,19,&irq4);

Omni4WD Omni(&wheel1,&wheel2,&wheel3,&wheel4);

static char line[LINE_MAX];
static uint8_t lineLen = 0;
static bool overflow = false;
static unsigned long lastCmdMs = 0;
static bool moving = false;
static unsigned int turnaroundMs = RS485_TURNAROUND_MS;
#ifdef RS485_DEBUG
static unsigned long dbgRxBytes = 0, dbgLines = 0, dbgLastMs = 0;
#endif

void setup() {
	TCCR1B=TCCR1B&0xf8|0x01;    // Pin9,Pin10 PWM 31250Hz
	TCCR2B=TCCR2B&0xf8|0x01;    // Pin3,Pin11 PWM 31250Hz

	Omni.PIDEnable(0.31,0.01,0,10);
	Serial.begin(BAUD);
	pinMode(PIN_RS485_DIR, OUTPUT);
	digitalWrite(PIN_RS485_DIR, LOW);
	lastCmdMs = millis();
}

// Send one reply line: wait for the M5's auto-direction RS485 unit to release the bus,
// drive the bus, wait until the last bit is out, release it.
static void reply(const __FlashStringHelper* msg) {
	delay(turnaroundMs);
	digitalWrite(PIN_RS485_DIR, HIGH);
	Serial.println(msg);
	Serial.flush();
	digitalWrite(PIN_RS485_DIR, LOW);
}

static int clampSpeed(long v) {
	if (v < 0) v = 0;
	if (v > MAX_SPEED_MMPS) v = MAX_SPEED_MMPS;
	return (int)v;
}

static bool needsSpeed(const char* c) {
	return !strcmp(c, "ADV") || !strcmp(c, "BACK") || !strcmp(c, "LEFT") ||
	       !strcmp(c, "RIGHT") || !strcmp(c, "ROTL") || !strcmp(c, "ROTR");
}

static void handleLine(char* s) {
	char* cmd = strtok(s, " ");
	if (!cmd) return;
	char* a1 = strtok(NULL, " ");
	char* a2 = strtok(NULL, " ");
	char* a3 = strtok(NULL, " ");
	int speed = a1 ? clampSpeed(atol(a1)) : 0;

	if (!strcmp(cmd, "PING")) { reply(F("PONG")); return; }
	if (!strcmp(cmd, "TURN")) {          // TURN <ms>: reply pause, 0-200
		if (!a1) { reply(F("ERR args")); return; }
		long ms = atol(a1);
		turnaroundMs = ms < 0 ? 0 : (ms > 200 ? 200 : (unsigned int)ms);
		reply(F("OK"));
		return;
	}
	if (!strcmp(cmd, "STOP")) { Omni.setCarStop(); moving = false; }
	else if (!strcmp(cmd, "MOVE")) {
		if (!a1 || !a2) { reply(F("ERR args")); return; }
		float rad = atof(a2) * PI / 180.0;
		float omega = a3 ? atof(a3) : 0;
		Omni.setCarMove(speed, rad, omega);
		moving = true;
	}
	else if (needsSpeed(cmd) && !a1) { reply(F("ERR args")); return; }
	else if (!strcmp(cmd, "ADV"))   { Omni.setCarAdvance(speed);     moving = true; }
	else if (!strcmp(cmd, "BACK"))  { Omni.setCarBackoff(speed);     moving = true; }
	else if (!strcmp(cmd, "LEFT"))  { Omni.setCarLeft(speed);        moving = true; }
	else if (!strcmp(cmd, "RIGHT")) { Omni.setCarRight(speed);       moving = true; }
	else if (!strcmp(cmd, "ROTL"))  { Omni.setCarRotateLeft(speed);  moving = true; }
	else if (!strcmp(cmd, "ROTR"))  { Omni.setCarRotateRight(speed); moving = true; }
	else { reply(F("ERR cmd")); return; }
	reply(F("OK"));
}

static void readSerial() {
	while (Serial.available()) {
		char c = Serial.read();
#ifdef RS485_DEBUG
		dbgRxBytes++;
		if (c == '\n') dbgLines++;
#endif
		if (c == '\r') continue;
		if (c == '\n') {
			if (overflow) reply(F("ERR long"));
			else if (lineLen) {
				line[lineLen] = 0;
				lastCmdMs = millis();
				handleLine(line);
			}
			lineLen = 0;
			overflow = false;
		} else if (lineLen < LINE_MAX - 1) {
			line[lineLen++] = c;
		} else {
			overflow = true;
		}
	}
}

#ifdef RS485_DEBUG
// Wiring debug (build with -DRS485_DEBUG): once a second report bytes received and lines parsed.
static void debugBeat() {
	if (millis() - dbgLastMs < 1000) return;
	dbgLastMs = millis();
	digitalWrite(PIN_RS485_DIR, HIGH);
	Serial.print(F("BEAT rx="));
	Serial.print(dbgRxBytes);
	Serial.print(F(" lines="));
	Serial.println(dbgLines);
	Serial.flush();
	digitalWrite(PIN_RS485_DIR, LOW);
}
#endif

void loop() {
#ifdef RS485_DEBUG
	debugBeat();
#endif
	readSerial();
	if (moving && millis() - lastCmdMs > FAILSAFE_MS) {
		Omni.setCarStop();
		moving = false;
	}
	Omni.PIDRegulate();
}
