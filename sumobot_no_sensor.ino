/*
 * ╔══════════════════════════════════════════════════════════════╗
 * ║              SUMO BOT — TANK DRIVE (MX1508)                 ║
 * ╠══════════════════════════════════════════════════════════════╣
 * ║  Board   : NodeMCU 32s                                      ║
 * ║  Input   : PS5 DualSense via Bluetooth (direct)             ║
 * ║  Driver  : MX1508 dual H-bridge                             ║
 * ╠══════════════════════════════════════════════════════════════╣
 * ║  MX1508 WIRING:                                             ║
 * ║    Left  motor : IN1A → 27 | IN2A → 26                     ║
 * ║    Right motor : IN1B → 32 | IN2B → 13                     ║
 * ║    No STBY pin, no separate PWM pin.                        ║
 * ║    PWM is applied directly to the IN pins.                  ║
 * ╠══════════════════════════════════════════════════════════════╣
 * ║  CONTROL:                                                   ║
 * ║    Left  stick Y → left  motor                             ║
 * ║    Right stick Y → right motor                             ║
 * ╠══════════════════════════════════════════════════════════════╣
 * ║  LIBRARY REQUIRED:                                          ║
 * ║    • ps5-esp32  by rodriguezst  (search: "PS5 ESP32")       ║
 * ╚══════════════════════════════════════════════════════════════╝
 */

#include <ps5Controller.h>

// ═══════════════════════════════════════════════════════════════
//  PIN DEFINITIONS
//  MX1508: PWM goes directly on IN pins — no separate PWM pin.
//  Forward:  IN1=PWM,  IN2=LOW
//  Reverse:  IN1=LOW,  IN2=PWM
//  Stop:     IN1=LOW,  IN2=LOW
// ═══════════════════════════════════════════════════════════════

// Left motor
#define PIN_IN1A   27
#define PIN_IN2A   26

// Right motor
#define PIN_IN1B   32
#define PIN_IN2B   13

// ═══════════════════════════════════════════════════════════════
//  LEDC PWM — one channel per motor input pin (4 total)
// ═══════════════════════════════════════════════════════════════
#define LEDC_CH_L_FWD  0   // IN1A
#define LEDC_CH_L_REV  1   // IN2A
#define LEDC_CH_R_FWD  2   // IN1B
#define LEDC_CH_R_REV  3   // IN2B
#define LEDC_FREQ      1000
#define LEDC_RES       8   // 0–255

// ═══════════════════════════════════════════════════════════════
//  TUNING
// ═══════════════════════════════════════════════════════════════
#define LEFT_CENTER_RAW    0
#define RIGHT_CENTER_RAW   0
#define JOYSTICK_DEADZONE  10

// ═══════════════════════════════════════════════════════════════
//  GLOBALS
// ═══════════════════════════════════════════════════════════════
bool wasConnected = false;

// ═══════════════════════════════════════════════════════════════
//  MOTOR CONTROL
//
//  MX1508 truth table:
//    Forward : IN1=PWM, IN2=0
//    Reverse : IN1=0,   IN2=PWM
//    Stop    : IN1=0,   IN2=0
//    (Brake) : IN1=1,   IN2=1  — not used here
// ═══════════════════════════════════════════════════════════════
void setLeftMotor(int speed) {
  speed = constrain(speed, -255, 255);
  if (speed > 0) {
    ledcWrite(LEDC_CH_L_FWD, speed);
    ledcWrite(LEDC_CH_L_REV, 0);
  } else if (speed < 0) {
    ledcWrite(LEDC_CH_L_FWD, 0);
    ledcWrite(LEDC_CH_L_REV, -speed);
  } else {
    ledcWrite(LEDC_CH_L_FWD, 0);
    ledcWrite(LEDC_CH_L_REV, 0);
  }
}

void setRightMotor(int speed) {
  speed = constrain(speed, -255, 255);
  if (speed > 0) {
    ledcWrite(LEDC_CH_R_FWD, speed);
    ledcWrite(LEDC_CH_R_REV, 0);
  } else if (speed < 0) {
    ledcWrite(LEDC_CH_R_FWD, 0);
    ledcWrite(LEDC_CH_R_REV, -speed);
  } else {
    ledcWrite(LEDC_CH_R_FWD, 0);
    ledcWrite(LEDC_CH_R_REV, 0);
  }
}

void stopMotors() {
  setLeftMotor(0);
  setRightMotor(0);
}

// ═══════════════════════════════════════════════════════════════
//  JOYSTICK NORMALIZATION
// ═══════════════════════════════════════════════════════════════
int normalizeStick(int raw, int center, bool invert = false) {
  int centered = raw - center;
  if (abs(centered) < JOYSTICK_DEADZONE) return 0;
  int out = constrain(centered * 2, -255, 255);
  return invert ? -out : out;
}

// ═══════════════════════════════════════════════════════════════
//  SETUP
// ═══════════════════════════════════════════════════════════════
void setup() {
  Serial.begin(115200);
  delay(300);
  Serial.println("\n=== SUMO BOT — MX1508 TANK DRIVE ===");

  // Attach LEDC channels to motor input pins
  ledcSetup(LEDC_CH_L_FWD, LEDC_FREQ, LEDC_RES);
  ledcAttachPin(PIN_IN1A, LEDC_CH_L_FWD);

  ledcSetup(LEDC_CH_L_REV, LEDC_FREQ, LEDC_RES);
  ledcAttachPin(PIN_IN2A, LEDC_CH_L_REV);

  ledcSetup(LEDC_CH_R_FWD, LEDC_FREQ, LEDC_RES);
  ledcAttachPin(PIN_IN1B, LEDC_CH_R_FWD);

  ledcSetup(LEDC_CH_R_REV, LEDC_FREQ, LEDC_RES);
  ledcAttachPin(PIN_IN2B, LEDC_CH_R_REV);

  stopMotors();

  // Replace with your DualSense Bluetooth MAC address
  ps5.begin("44:46:48:52:5a:38");

  Serial.println("Waiting for PS5 DualSense...");
  Serial.println("Hold PS + CREATE to pair.\n");
}

// ═══════════════════════════════════════════════════════════════
//  MAIN LOOP
// ═══════════════════════════════════════════════════════════════
void loop() {
  if (!ps5.isConnected()) {
    wasConnected = false;
    stopMotors();
    delay(50);
    return;
  }

  if (!wasConnected) {
    Serial.println("PS5 connected! Tank drive active.");
    wasConnected = true;
  }

  int leftSpeed  = normalizeStick(ps5.LStickY(), LEFT_CENTER_RAW,  false);
  int rightSpeed = normalizeStick(ps5.RStickY(), RIGHT_CENTER_RAW, false);

  setLeftMotor(leftSpeed);
  setRightMotor(rightSpeed);

  // ── Serial debug ───────────────────────────────────────────
  static unsigned long lastPrint = 0;
  if (millis() - lastPrint > 200) {
    Serial.print("L_raw:"); Serial.print(ps5.LStickY());
    Serial.print("  R_raw:"); Serial.print(ps5.RStickY());
    Serial.print("  L_out:"); Serial.print(leftSpeed);
    Serial.print("  R_out:"); Serial.println(rightSpeed);
    lastPrint = millis();
  }

  delay(10);
}
