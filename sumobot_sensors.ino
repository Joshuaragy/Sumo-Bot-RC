/*
 * ╔══════════════════════════════════════════════════════════════╗
 * ║              SUMO BOT — TANK DRIVE (No Sensors)             ║
 * ╠══════════════════════════════════════════════════════════════╣
 * ║  Board   : NodeMCU 32s                                      ║
 * ║  Input   : PS5 DualSense via Bluetooth (direct)             ║
 * ║  Motors  : TB6612FNG (left=A, right=B)                      ║
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
// ═══════════════════════════════════════════════════════════════

// TB6612FNG — Left motor (Motor A)
#define PIN_PWMA   25
#define PIN_AIN1   27
#define PIN_AIN2   26

// TB6612FNG — Right motor (Motor B)
#define PIN_PWMB   33
#define PIN_BIN1   32
#define PIN_BIN2   13

// TB6612FNG — Standby (must be HIGH to enable driver)
#define PIN_STBY   14

// ═══════════════════════════════════════════════════════════════
//  LEDC PWM
// ═══════════════════════════════════════════════════════════════
#define LEDC_CH_A  0
#define LEDC_CH_B  1
#define LEDC_FREQ  1000
#define LEDC_RES   8     // 0–255

// ═══════════════════════════════════════════════════════════════
//  TUNING
//
//  CENTER_RAW: the value your PS5 library reports when the stick
//  is untouched. Use Serial debug below to find yours.
//  If the bot drives forward on its own, this value is wrong.
//
//  JOYSTICK_DEADZONE: raw units around center that are zeroed out.
//  Raise if motors still twitch at rest; lower for sharper response.
// ═══════════════════════════════════════════════════════════════
#define LEFT_CENTER_RAW    7
#define RIGHT_CENTER_RAW   7
#define JOYSTICK_DEADZONE  30

// ═══════════════════════════════════════════════════════════════
//  GLOBALS
// ═══════════════════════════════════════════════════════════════
bool wasConnected = false;

// ═══════════════════════════════════════════════════════════════
//  MOTOR CONTROL
// ═══════════════════════════════════════════════════════════════
void setLeftMotor(int speed) {
  speed = constrain(speed, -255, 255);
  if (speed > 0) {
    digitalWrite(PIN_AIN1, HIGH);
    digitalWrite(PIN_AIN2, LOW);
  } else if (speed < 0) {
    digitalWrite(PIN_AIN1, LOW);
    digitalWrite(PIN_AIN2, HIGH);
    speed = -speed;
  } else {
    digitalWrite(PIN_AIN1, LOW);
    digitalWrite(PIN_AIN2, LOW);
  }
  ledcWrite(LEDC_CH_A, (uint8_t)speed);
}

void setRightMotor(int speed) {
  speed = constrain(speed, -255, 255);
  if (speed > 0) {
    digitalWrite(PIN_BIN1, HIGH);
    digitalWrite(PIN_BIN2, LOW);
  } else if (speed < 0) {
    digitalWrite(PIN_BIN1, LOW);
    digitalWrite(PIN_BIN2, HIGH);
    speed = -speed;
  } else {
    digitalWrite(PIN_BIN1, LOW);
    digitalWrite(PIN_BIN2, LOW);
  }
  ledcWrite(LEDC_CH_B, (uint8_t)speed);
}

void stopMotors() {
  setLeftMotor(0);
  setRightMotor(0);
}

// ═══════════════════════════════════════════════════════════════
//  JOYSTICK NORMALIZATION
//
//  raw    : value from ps5.LStickY() / ps5.RStickY()  (0–255)
//  center : resting value of that stick (yours = 7)
//  invert : true = pushing stick up gives positive output
//
//  Output : -255 … +255  (0 inside deadzone)
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
  Serial.println("\n=== SUMO BOT — TANK DRIVE ===");

  // Motor driver pins
  pinMode(PIN_AIN1, OUTPUT);
  pinMode(PIN_AIN2, OUTPUT);
  pinMode(PIN_BIN1, OUTPUT);
  pinMode(PIN_BIN2, OUTPUT);
  pinMode(PIN_STBY, OUTPUT);

  // LEDC PWM channels
  ledcSetup(LEDC_CH_A, LEDC_FREQ, LEDC_RES);
  ledcAttachPin(PIN_PWMA, LEDC_CH_A);
  ledcSetup(LEDC_CH_B, LEDC_FREQ, LEDC_RES);
  ledcAttachPin(PIN_PWMB, LEDC_CH_B);

  digitalWrite(PIN_STBY, HIGH);  // Enable TB6612FNG
  stopMotors();

  // Replace the MAC below with your controller's Bluetooth address
  ps5.begin("a0:fa:9c:71:73:15");

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

  // Tank drive: each stick drives its motor independently
  int leftSpeed  = normalizeStick(ps5.LStickY(), LEFT_CENTER_RAW,  true);
  int rightSpeed = normalizeStick(ps5.RStickY(), RIGHT_CENTER_RAW, true);

  setLeftMotor(leftSpeed);
  setRightMotor(rightSpeed);

  // ── Serial debug (remove when not needed — costs ~1ms/print) ──
  static unsigned long lastPrint = 0;
  if (millis() - lastPrint > 200) {
    Serial.print("L_raw:");  Serial.print(ps5.LStickY());
    Serial.print("  R_raw:"); Serial.print(ps5.RStickY());
    Serial.print("  L_out:");  Serial.print(leftSpeed);
    Serial.print("  R_out:");  Serial.println(rightSpeed);
    lastPrint = millis();
  }

  delay(10);
}
