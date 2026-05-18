// L9110 wiring + direction smoke test. No ESP-NOW, no RSSI — just cycles
// STOP → FORWARD → STOP → SPIN → STOP so you can confirm each command
// does what it claims before flashing the full follower.
//
// Expected behavior:
//   FORWARD: both wheels turn the same way → robot drives forward
//   SPIN:    wheels turn opposite ways → robot pivots in place (clockwise)
// If FORWARD goes backward → swap leads on BOTH motors at the L9110 outputs.
// If SPIN drives forward/back → swap leads on ONE motor.

const int LEFT_IA  = D2;
const int LEFT_IB  = D3;
const int RIGHT_IA = D4;
const int RIGHT_IB = D5;

const int PWM_FREQ  = 1000;
const int PWM_RES   = 8;
const int SPIN_DUTY  = 110;
const int DRIVE_DUTY = 180;

void motorsBegin() {
  ledcAttach(LEFT_IA,  PWM_FREQ, PWM_RES);
  ledcAttach(LEFT_IB,  PWM_FREQ, PWM_RES);
  ledcAttach(RIGHT_IA, PWM_FREQ, PWM_RES);
  ledcAttach(RIGHT_IB, PWM_FREQ, PWM_RES);
}

void motorsStop() {
  ledcWrite(LEFT_IA,  0);
  ledcWrite(LEFT_IB,  0);
  ledcWrite(RIGHT_IA, 0);
  ledcWrite(RIGHT_IB, 0);
}

void motorsForward() {
  ledcWrite(LEFT_IA,  DRIVE_DUTY);
  ledcWrite(LEFT_IB,  0);
  ledcWrite(RIGHT_IA, DRIVE_DUTY);
  ledcWrite(RIGHT_IB, 0);
}

void motorsSpin() {
  ledcWrite(LEFT_IA,  SPIN_DUTY);
  ledcWrite(LEFT_IB,  0);
  ledcWrite(RIGHT_IA, 0);
  ledcWrite(RIGHT_IB, SPIN_DUTY);
}

void setup() {
  Serial.begin(115200);
  delay(500);
  motorsBegin();
  motorsStop();
}

void loop() {
  Serial.println("STOP (2s)");
  motorsStop();
  delay(2000);

  Serial.println("FORWARD (2s) — both wheels same direction");
  motorsForward();
  delay(2000);

  Serial.println("STOP (1s)");
  motorsStop();
  delay(1000);

  Serial.println("SPIN (3s) — pivot in place");
  motorsSpin();
  delay(3000);
}
