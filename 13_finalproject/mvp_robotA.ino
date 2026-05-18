#include <esp_now.h>
#include <WiFi.h>

// ============================================================================
// ROBOT A — MAC 58:8C:81:A0:2F:1C — chases Robot B
// Flash this sketch to Robot A only.
// ============================================================================

enum Phase { IDLE, DRIVING };
enum DriveMode { STRAIGHT_MODE, CURVE_LEFT_MODE, CURVE_RIGHT_MODE };

// Per-robot config — DO NOT EDIT (different per sketch):
uint8_t TARGET_MAC[6] = {0x58, 0x8C, 0x81, 0x9F, 0xBA, 0xA8};   // → Robot B
DriveMode INITIAL_CURVE = CURVE_LEFT_MODE;

// ============================================================================
// Motor primitives — 2× ULN2003 / 28BYJ-48 unipolar steppers.
// ============================================================================
const int RIGHT_IN[4] = {D1, D2, D3, D4};
const int LEFT_IN[4]  = {D5, D6, D7, D10};

int leftFwdDir  = -1;
int rightFwdDir = +1;

const uint8_t stepSeq[4][4] = {
  {1, 1, 0, 0},
  {0, 1, 1, 0},
  {0, 0, 1, 1},
  {1, 0, 0, 1}
};
const int SEQ_LEN = 4;

const int STEP_RATE_HZ = 600;
const unsigned long FAST_PERIOD_US = 1000000UL / STEP_RATE_HZ;
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
bool motorsRunning = false;

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
  motorsRunning = false;
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
  motorsRunning = true;
}

void motorsCurveLeft() {
  leftStepDir  = leftFwdDir;
  rightStepDir = rightFwdDir;
  leftStepPeriodUs  = SLOW_PERIOD_US;
  rightStepPeriodUs = FAST_PERIOD_US;
  motorsRunning = true;
}

void motorsCurveRight() {
  leftStepDir  = leftFwdDir;
  rightStepDir = rightFwdDir;
  leftStepPeriodUs  = FAST_PERIOD_US;
  rightStepPeriodUs = SLOW_PERIOD_US;
  motorsRunning = true;
}

void motorsTick() {
  if (!motorsRunning) return;
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

// ============================================================================
// Tunables
// ============================================================================
const unsigned long DECISION_INTERVAL_MS = 1000;
const int RSSI_IMPROVE_THRESHOLD = -2;
const int RSSI_LOST = -85;
const unsigned long PACKET_TIMEOUT_MS = 1000;

// ============================================================================
// Beacon broadcast
// ============================================================================
uint8_t broadcastAddr[6] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
const unsigned long BEACON_INTERVAL_MS = 200;
unsigned long lastBeaconMillis = 0;
uint32_t beaconId = 0;

// ============================================================================
// State
// ============================================================================
typedef struct struct_message { uint32_t packet_id; } struct_message;
struct_message incomingMsg;
struct_message outgoingMsg;
esp_now_peer_info_t peerInfo;

volatile Phase phase = IDLE;
DriveMode driveMode = STRAIGHT_MODE;
DriveMode lastGoodCurve = INITIAL_CURVE;
int consecutiveBadInCurve = 0;
const int FLIP_THRESHOLD = 3;

unsigned long lastDecisionMillis = 0;
float lastDecisionRSSI = -100.0f;

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

void applyMode(DriveMode m) {
  if      (m == STRAIGHT_MODE)     motorsForward();
  else if (m == CURVE_LEFT_MODE)   motorsCurveLeft();
  else                              motorsCurveRight();
}

void enterIDLE() {
  phase = IDLE;
  motorsStop();
}

void enterDRIVING(DriveMode m) {
  phase = DRIVING;
  driveMode = m;
  lastDecisionMillis = millis();
  lastDecisionRSSI   = currentAvgRSSI();
  applyMode(m);
}

void onReceive(const esp_now_recv_info *info, const uint8_t *data, int len) {
  if (len != sizeof(incomingMsg)) return;
  if (memcmp(info->src_addr, TARGET_MAC, 6) != 0) return;

  memcpy(&incomingMsg, data, sizeof(incomingMsg));
  lastPacketMillis = millis();

  int rssi = info->rx_ctrl->rssi;
  rssiBuf[rssiIdx++] = rssi;
  if (rssiIdx >= AVG_WINDOW) { rssiIdx = 0; rssiFilled = true; }
}

void setup() {
  Serial.begin(115200);
  delay(500);
  Serial.println("=== ROBOT A starting (chases B) ===");
  motorsBegin();

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

  enterIDLE();
}

void loop() {
  motorsTick();

  if (millis() - lastBeaconMillis >= BEACON_INTERVAL_MS) {
    beaconId++;
    outgoingMsg.packet_id = beaconId;
    esp_now_send(broadcastAddr, (uint8_t*)&outgoingMsg, sizeof(outgoingMsg));
    lastBeaconMillis = millis();
  }

  if (millis() - lastPacketMillis > PACKET_TIMEOUT_MS) {
    if (phase != IDLE) {
      Serial.println("Watchdog: signal lost, IDLE");
      enterIDLE();
    }
    return;
  }

  float avg = currentAvgRSSI();

  if (phase == IDLE) {
    if (avg > RSSI_LOST) {
      Serial.println("IDLE → DRIVING (signal present)");
      enterDRIVING(STRAIGHT_MODE);
    }
    return;
  }

  if (millis() - lastDecisionMillis < DECISION_INTERVAL_MS) return;

  float delta = avg - lastDecisionRSSI;
  Serial.print("decision: avg=");
  Serial.print(avg, 1);
  Serial.print(" delta=");
  Serial.print(delta, 1);
  Serial.print(" mode=");
  Serial.print(driveMode == STRAIGHT_MODE ? "STRAIGHT" :
               (driveMode == CURVE_LEFT_MODE ? "CURVE_L" : "CURVE_R"));

  DriveMode nextMode = driveMode;
  if (delta > RSSI_IMPROVE_THRESHOLD) {
    if (driveMode != STRAIGHT_MODE) lastGoodCurve = driveMode;
    consecutiveBadInCurve = 0;
    nextMode = STRAIGHT_MODE;
    Serial.print(" → STRAIGHT (progress)");
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

  enterDRIVING(nextMode);
}
