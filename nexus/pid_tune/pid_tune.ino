// Wheel PID tuning harness for the Nexus 4WD robot. Talks over USB at 115200 (RS485 jumper removed).
// Commands (one line each):
//   GAIN <kc> <taui> <taud> <sampleMs>   set the PID tunings of all four wheels
//   CIR <mm>                              wheel circumference (58 mm wheel = 182)
//   FIX <bits>                            1 = regulate each wheel separately, 2 = zero a stalled speed reading,
//                                         4 = clear the PID accumulator on STOP, 8 = hold PWM 0 after STOP until RUN,
//                                         16 = never let the PID reverse a wheel
//   PWMF <0|1>                            0 = default 490 Hz PWM, 1 = 31 kHz
//   RUN <mm/s>                            drive forward and stream telemetry
//   STOP                                  stop; telemetry continues until QUIET
//   QUIET                                 stop telemetry
// Telemetry: D <ms> <rpm x4> <pwm x4> <pulses x4>   (wheel order UL LL LR UR)
#include <PinChangeInt.h>
#include <PinChangeIntConfig.h>
#include <EEPROM.h>
#include <fuzzy_table.h>
#include <PID_Beta6.h>
#define private public      // reach Motor::speed2DutyCycle to clear it on stop
#include <MotorWheel.h>
#include <Omni4WD.h>
#undef private

irqISR(irq1,isr1);
MotorWheel wheel1(3,2,4,5,&irq1);

irqISR(irq2,isr2);
MotorWheel wheel2(11,12,14,15,&irq2);

irqISR(irq3,isr3);
MotorWheel wheel3(9,8,16,17,&irq3);

irqISR(irq4,isr4);
MotorWheel wheel4(10,7,18,19,&irq4);

Omni4WD Omni(&wheel1,&wheel2,&wheel3,&wheel4);
static MotorWheel* const wheels[4] = {&wheel1,&wheel2,&wheel3,&wheel4};

#define STALL_US 60000UL
static uint8_t fix = 0;
static bool telemetry = false;
static bool halted = false;
static unsigned long lastTelMs = 0, t0 = 0;
static char line[64];
static uint8_t lineLen = 0;

static void regulate() {
	if (fix & 2) {          // a wheel that stopped sends no pulses, so its last speed would stay forever
		for (uint8_t i = 0; i < 4; i++) {
			noInterrupts();
			unsigned long last = wheels[i]->isr->pulseEndMicros;
			interrupts();
			if (micros() - last > STALL_US) wheels[i]->isr->speedPPS = 0;
		}
	}
	if (halted) return;
	if (fix & 1) {
		for (uint8_t i = 0; i < 4; i++) {
			wheels[i]->PIDRegulate();
			if ((fix & 16) && wheels[i]->speed2DutyCycle < 0) {
				wheels[i]->speed2DutyCycle = 0;
				wheels[i]->runPWM(0, wheels[i]->getDesiredDir(), false);
			}
		}
	} else {
		Omni.PIDRegulate();
	}
}

static void stopCar() {
	Omni.setCarStop();
	if (fix & 4) {
		for (uint8_t i = 0; i < 4; i++) {
			wheels[i]->speed2DutyCycle = 0;
			wheels[i]->runPWM(0, DIR_ADVANCE);
		}
	}
	if (fix & 8) halted = true;
}

static void setGains(float kc, float ti, float td, unsigned int ms) {
	for (uint8_t i = 0; i < 4; i++) wheels[i]->PIDSetup(kc, ti, td, ms);
}

static void handle(char* s) {
	char* cmd = strtok(s, " ");
	char* a = strtok(NULL, " ");
	char* b = strtok(NULL, " ");
	char* c = strtok(NULL, " ");
	char* d = strtok(NULL, " ");
	if (!cmd) return;
	if (!strcmp(cmd, "GAIN") && a && b && c && d) { setGains(atof(a), atof(b), atof(c), atoi(d)); }
	else if (!strcmp(cmd, "CIR") && a) { for (uint8_t i = 0; i < 4; i++) wheels[i]->setCirMM(atoi(a)); }
	else if (!strcmp(cmd, "FIX") && a) { fix = atoi(a); }
	else if (!strcmp(cmd, "PWMF") && a) {
		if (atoi(a)) { TCCR1B=TCCR1B&0xf8|0x01; TCCR2B=TCCR2B&0xf8|0x01; }
		else { TCCR1B=TCCR1B&0xf8|0x03; TCCR2B=TCCR2B&0xf8|0x04; }
	}
	else if (!strcmp(cmd, "RUN") && a) {
		if (halted) { halted = false; for (uint8_t i = 0; i < 4; i++) wheels[i]->speed2DutyCycle = 0; }
		Omni.setCarAdvance(atoi(a)); telemetry = true; t0 = millis(); }
	else if (!strcmp(cmd, "STOP")) { stopCar(); }
	else if (!strcmp(cmd, "QUIET")) { telemetry = false; }
	else { Serial.println(F("ERR")); return; }
	Serial.println(F("OK"));
}

void setup() {
	TCCR1B=TCCR1B&0xf8|0x01;    // Pin9,Pin10 PWM 31250Hz
	TCCR2B=TCCR2B&0xf8|0x01;    // Pin3,Pin11 PWM 31250Hz
	Omni.PIDEnable(0.31,0.01,0,10);
	Serial.begin(115200);
	Serial.println(F("READY"));
}

void loop() {
	while (Serial.available()) {
		char ch = Serial.read();
		if (ch == '\r') continue;
		if (ch == '\n') { line[lineLen] = 0; handle(line); lineLen = 0; }
		else if (lineLen < sizeof(line) - 1) line[lineLen++] = ch;
	}
	regulate();
	if (telemetry && millis() - lastTelMs >= 50) {
		lastTelMs = millis();
		Serial.print('D'); Serial.print(' '); Serial.print(lastTelMs - t0);
		for (uint8_t i = 0; i < 4; i++) { Serial.print(' '); Serial.print(wheels[i]->getSpeedRPM()); }
		for (uint8_t i = 0; i < 4; i++) { Serial.print(' '); Serial.print(wheels[i]->getPWM()); }
		for (uint8_t i = 0; i < 4; i++) { Serial.print(' '); Serial.print(wheels[i]->getCurrPulse()); }
		Serial.println();
	}
}
