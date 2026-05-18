#include <esp_now.h>
#include <WiFi.h>

// Enum declaration up here so Arduino's auto-generated function prototypes
// (which it emits at the top of the file) can reference DriveMode.
enum DriveMode { STRAIGHT_MODE, CURVE_LEFT_MODE, CURVE_RIGHT_MODE };

// ============================================================================
// PREY (Robot B): receives the predator's beacon, computes RSSI moving avg,
// flees forward when the predator is close. Drives 2× A4988 stepper drivers,
// one per wheel, using non-blocking step-pulse generation so ESP-NOW packets
// keep flowing during motor activity.
//
// !!! IMPORTANT: update LEFT_STEP / LEFT_DIR / RIGHT_STEP / RIGHT_DIR below
// !!! to match your existing stepper-robot wiring. Defaults are guesses.
// ============================================================================

// --- ULN2003 / 28BYJ-48 wiring -------------------------------------------
// Pin order in each array is IN1, IN2, IN3, IN4.
const int RIGHT_IN[4] = {D1, D2, D3, D4};
const int LEFT_IN[4]  = {D5, D6, D7, D10};

// Per-motor "forward" sequence direction. Differential-drive geometry means
// the two motors face opposite ways, so one normally runs +1 sequence dir
// and the other -1 to make the chassis drive forward. If a motor turns the
// wrong way, flip its constant.
int leftFwdDir  = -1;
int rightFwdDir = +1;

// Full-step sequence for 28BYJ-48 (4 phases). Half the resolution of half-step
// but twice the linear speed per step pulse. Slightly noisier mechanically.
const uint8_t stepSeq[4][4] = {
  {1, 1, 0, 0},
  {0, 1, 1, 0},
  {0, 0, 1, 1},
  {1, 0, 0, 1}
};
const int SEQ_LEN = 4;

// Full-speed step rate. 600 Hz × 65 mm wheels = ~6 cm/s.
const int STEP_RATE_HZ = 600;
const unsigned long FAST_PERIOD_US = 1000000UL / STEP_RATE_HZ;
// Higher = tighter curve. 4 keeps inner wheel rolling but at 1/4 speed.
const int CURVE_SLOW_DIVISOR = 4;
const unsigned long SLOW_PERIOD_US = FAST_PERIOD_US * CURVE_SLOW_DIVISOR;

int leftStepIdx = 0;
int rightStepIdx = 0;
int leftStepDir = 0;
int rightStepDir = 0;
unsigned long leftStepPeriodUs  = FAST_PERIOD_US;
unsigned long rightStepPeriodUs = FAST_PERIOD_US;
unsigned long lastLeftStepMicros  = 0;
unsigned long lastRightStepMicros = 0;
bool fleeing = false;

void writePins(const int pins[4], int seqIdx) {
  digitalWrite(pins[0], stepSeq[seqIdx][0]);
  digitalWrite(pins[1], stepSeq[seqIdx][1]);
  digitalWrite(pins[2], stepSeq[seqIdx][2]);
  digitalWrite(pins[3], stepSeq[seqIdx][3]);
}

void motorsBegin() {
  for (int i = 0; i < 4; i++) {
    pinMode(LEFT_IN[i],  OUTPUT);
    pinMode(RIGHT_IN[i], OUTPUT);
    digitalWrite(LEFT_IN[i],  LOW);
    digitalWrite(RIGHT_IN[i], LOW);
  }
}

void motorsStop() {
  fleeing = false;
  leftStepDir  = 0;
  rightStepDir = 0;
  for (int i = 0; i < 4; i++) {
    digitalWrite(LEFT_IN[i],  LOW);
    digitalWrite(RIGHT_IN[i], LOW);
  }
}

void motorsForward() {
  leftStepDir  = leftFwdDir;
  rightStepDir = rightFwdDir;
  leftStepPeriodUs  = FAST_PERIOD_US;
  rightStepPeriodUs = FAST_PERIOD_US;
  fleeing = true;
}

// Slow-side curve: inner wheel still rolling forward at 1/CURVE_SLOW_DIVISOR
// of outer-wheel speed. Continuous motion, looks natural.
void motorsCurveLeft() {
  leftStepDir  = leftFwdDir;
  rightStepDir = rightFwdDir;
  leftStepPeriodUs  = SLOW_PERIOD_US;
  rightStepPeriodUs = FAST_PERIOD_US;
  fleeing = true;
}

void motorsCurveRight() {
  leftStepDir  = leftFwdDir;
  rightStepDir = rightFwdDir;
  leftStepPeriodUs  = FAST_PERIOD_US;
  rightStepPeriodUs = SLOW_PERIOD_US;
  fleeing = true;
}

void motorsTick() {
  if (leftStepDir == 0 && rightStepDir == 0) return;
  unsigned long now = micros();

  if (leftStepDir != 0 && now - lastLeftStepMicros >= leftStepPeriodUs) {
    leftStepIdx = (leftStepIdx + leftStepDir + SEQ_LEN) % SEQ_LEN;
    writePins(LEFT_IN, leftStepIdx);
    lastLeftStepMicros = now;
  }
  if (rightStepDir != 0 && now - lastRightStepMicros >= rightStepPeriodUs) {
    rightStepIdx = (rightStepIdx + rightStepDir + SEQ_LEN) % SEQ_LEN;
    writePins(RIGHT_IN, rightStepIdx);
    lastRightStepMicros = now;
  }
}

// --- Flee-mode state ------------------------------------------------------
DriveMode driveMode = STRAIGHT_MODE;
DriveMode lastGoodCurve = CURVE_RIGHT_MODE;
int consecutiveBadInCurve = 0;
const int FLIP_THRESHOLD = 3;
const unsigned long DECISION_INTERVAL_MS = 1000;
const int RSSI_DROP_THRESHOLD = 0;   // require avg to drop more than this to count as "escaping"
unsigned long lastDecisionMillis = 0;
float lastDecisionRSSI = -100.0f;

void applyMode(DriveMode m) {
  if      (m == STRAIGHT_MODE)     motorsForward();
  else if (m == CURVE_LEFT_MODE)   motorsCurveLeft();
  else                              motorsCurveRight();
}

// --- ESP-NOW + RSSI -------------------------------------------------------
typedef struct struct_message { uint32_t packet_id; } struct_message;
struct_message incomingMsg;
struct_message outgoingMsg;
esp_now_peer_info_t peerInfo;

uint8_t broadcastAddr[6] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
const unsigned long BEACON_INTERVAL_MS = 200;   // 5 Hz
unsigned long lastBeaconMillis = 0;
uint32_t beaconId = 0;

// Hysteresis: flee when predator close (strong RSSI), idle when far (weak).
// Calibrated indoors (-28 dBm at touching, -50 dBm at 2.5 m).
const int RSSI_FLEE_ON  = -38;
const int RSSI_FLEE_OFF = -48;
const int RSSI_LOST     = -85;
const unsigned long PACKET_TIMEOUT_MS = 1000;

const int AVG_WINDOW = 10;
int rssiBuf[AVG_WINDOW];
int rssiIdx = 0;
bool rssiFilled = false;
volatile unsigned long lastPacketMillis = 0;

float currentAvgRSSI() {
  int n = rssiFilled ? AVG_WINDOW : rssiIdx;
  if (n == 0) return -100.0f;
  long s = 0;
  for (int i = 0; i < n; i++) s += rssiBuf[i];
  return (float)s / n;
}

void onReceive(const esp_now_recv_info *info, const uint8_t *data, int len) {
  if (len != sizeof(incomingMsg)) return;
  memcpy(&incomingMsg, data, sizeof(incomingMsg));
  lastPacketMillis = millis();

  int rssi = info->rx_ctrl->rssi;
  rssiBuf[rssiIdx++] = rssi;
  if (rssiIdx >= AVG_WINDOW) { rssiIdx = 0; rssiFilled = true; }
}

void setup() {
  Serial.begin(115200);
  delay(500);
  Serial.println("=== mvp_prey starting ===");
  motorsBegin();
  Serial.println("motorsBegin done");

  WiFi.mode(WIFI_STA);
  if (esp_now_init() != ESP_OK) {
    Serial.println("ESP-NOW init failed");
    return;
  }
  esp_now_register_recv_cb(onReceive);

  memset(&peerInfo, 0, sizeof(peerInfo));
  memcpy(peerInfo.peer_addr, broadcastAddr, 6);
  peerInfo.channel = 0;
  peerInfo.encrypt = false;
  if (esp_now_add_peer(&peerInfo) != ESP_OK) {
    Serial.println("Failed to add broadcast peer");
  }
}

void loop() {
  // 1. Step pulse generator — must run every loop iteration when fleeing.
  motorsTick();

  // 2. Broadcast our own beacon at 5 Hz.
  if (millis() - lastBeaconMillis >= BEACON_INTERVAL_MS) {
    beaconId++;
    outgoingMsg.packet_id = beaconId;
    esp_now_send(broadcastAddr, (uint8_t*)&outgoingMsg, sizeof(outgoingMsg));
    lastBeaconMillis = millis();
  }

  // 3. Watchdog — predator silent → idle.
  if (millis() - lastPacketMillis > PACKET_TIMEOUT_MS) {
    if (fleeing) {
      Serial.println("Watchdog: predator silent, IDLE");
      motorsStop();
    }
    return;
  }

  // 4. State transitions: hysteresis enters/exits FLEEING.
  float avg = currentAvgRSSI();
  if (!fleeing && avg > RSSI_FLEE_ON) {
    Serial.print("FLEE start  avg=");
    Serial.println(avg, 1);
    driveMode = STRAIGHT_MODE;
    lastDecisionMillis = millis();
    lastDecisionRSSI = avg;
    applyMode(STRAIGHT_MODE);
    return;
  }
  if (fleeing && avg < RSSI_FLEE_OFF) {
    Serial.print("IDLE  avg=");
    Serial.println(avg, 1);
    motorsStop();
    return;
  }

  // 5. While fleeing: gradient DESCENT — pick the direction where RSSI drops
  //    fastest (predator getting farther). Same algorithm as predator's
  //    ascent, with the sign flipped.
  if (!fleeing) return;
  if (millis() - lastDecisionMillis < DECISION_INTERVAL_MS) return;

  float delta = avg - lastDecisionRSSI;
  Serial.print("flee decision: avg=");
  Serial.print(avg, 1);
  Serial.print(" delta=");
  Serial.print(delta, 1);
  Serial.print(" mode=");
  Serial.print(driveMode == STRAIGHT_MODE ? "STRAIGHT" :
               (driveMode == CURVE_LEFT_MODE ? "CURVE_L" : "CURVE_R"));

  // For the prey, "good" = RSSI dropped (we're escaping).
  bool escaping = (delta < -RSSI_DROP_THRESHOLD);

  DriveMode nextMode = driveMode;
  if (escaping) {
    if (driveMode != STRAIGHT_MODE) lastGoodCurve = driveMode;
    consecutiveBadInCurve = 0;
    nextMode = STRAIGHT_MODE;
    Serial.print(" → STRAIGHT (escaping)");
  } else if (driveMode == STRAIGHT_MODE) {
    nextMode = lastGoodCurve;
    consecutiveBadInCurve = 1;
    Serial.print(" → start lastGoodCurve");
  } else {
    consecutiveBadInCurve++;
    if (consecutiveBadInCurve >= FLIP_THRESHOLD) {
      nextMode = (driveMode == CURVE_LEFT_MODE) ? CURVE_RIGHT_MODE : CURVE_LEFT_MODE;
      consecutiveBadInCurve = 1;
      Serial.print(" → FLIP curve direction");
    } else {
      nextMode = driveMode;
      Serial.print(" → continue curve (committed)");
    }
  }
  Serial.println();

  driveMode = nextMode;
  lastDecisionMillis = millis();
  lastDecisionRSSI = avg;
  applyMode(nextMode);
}
