#include <esp_now.h>
#include <WiFi.h>

// Enum declarations up here so Arduino's auto-generated function prototypes
// (which it emits at the top of the file) can reference them.
enum Phase { IDLE, DRIVING };
enum DriveMode { STRAIGHT_MODE, CURVE_LEFT_MODE, CURVE_RIGHT_MODE };

// ============================================================================
// TRIANGLE CHASE — TWO constants to update per robot before flashing:
//
//   Robot A (MAC 58:8C:81:A0:2F:1C) chases B:
//     TARGET_MAC = {0x58, 0x8C, 0x81, 0x9F, 0xBA, 0xA8}
//     INITIAL_CURVE = CURVE_LEFT_MODE
//
//   Robot B (MAC 58:8C:81:9F:BA:A8) chases C:
//     TARGET_MAC = {0x58, 0x8C, 0x81, 0x9D, 0x2A, 0xD4}
//     INITIAL_CURVE = CURVE_RIGHT_MODE
//
//   Robot C (MAC 58:8C:81:9D:2A:D4) chases A:
//     TARGET_MAC = {0x58, 0x8C, 0x81, 0xA0, 0x2F, 0x1C}
//     INITIAL_CURVE = CURVE_LEFT_MODE
//
// Alternating L/R/L breaks the symmetric "everyone-curves-right" failure
// that locks the three robots into spreading outward.
// ============================================================================
uint8_t TARGET_MAC[6] = {0x58, 0x8C, 0x81, 0x9F, 0xBA, 0xA8};   // ← UPDATE PER ROBOT
DriveMode INITIAL_CURVE = CURVE_LEFT_MODE;                      // ← UPDATE PER ROBOT

// ============================================================================
// Motor primitives — 2× ULN2003 / 28BYJ-48 unipolar steppers.
// Same wiring as mvp_prey; same step-pulse engine. Three motion modes:
//   motorsForward()  — both motors step forward (drive toward prey)
//   motorsSpin()     — left fwd, right reverse (in-place pivot for scanning)
//   motorsStop()     — all coils off
// ============================================================================
const int RIGHT_IN[4] = {D1, D2, D3, D4};
const int LEFT_IN[4]  = {D5, D6, D7, D10};

// Per-motor forward sequence direction. Differential drive geometry means
// the two motors face opposite ways, so one runs +1 and the other -1 to
// produce chassis-forward motion. If a motor turns the wrong way, flip its
// constant.
int leftFwdDir  = -1;
int rightFwdDir = +1;

// Full-step sequence for 28BYJ-48 (4 phases). Twice the linear speed of
// half-step at the same step rate. Slightly noisier mechanically.
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
// Inner-wheel slowdown during a curve. 2 = half speed (gentle curve radius
// ≈ 3× wheelbase). Bump higher for tighter turns (3 = 2× wheelbase, etc.).
// Higher = tighter curve. 4 is a good balance: inner wheel still moving
// forward (continuous motion looks natural), but the speed differential is
// large enough that one cycle of curving produces a meaningful heading change.
const int CURVE_SLOW_DIVISOR = 4;
const unsigned long SLOW_PERIOD_US = FAST_PERIOD_US * CURVE_SLOW_DIVISOR;

int leftStepIdx = 0;
int rightStepIdx = 0;
int leftStepDir = 0;    // +1, -1, or 0 (idle)
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

// Straight ahead — both wheels at full speed.
void motorsForward() {
  leftStepDir  = leftFwdDir;
  rightStepDir = rightFwdDir;
  leftStepPeriodUs  = FAST_PERIOD_US;
  rightStepPeriodUs = FAST_PERIOD_US;
  motorsRunning = true;
}

// Curving left — both wheels still rolling forward, but left wheel slower
// than right. Robot curves left while advancing.
void motorsCurveLeft() {
  leftStepDir  = leftFwdDir;
  rightStepDir = rightFwdDir;
  leftStepPeriodUs  = SLOW_PERIOD_US;
  rightStepPeriodUs = FAST_PERIOD_US;
  motorsRunning = true;
}

// Curving right — mirror.
void motorsCurveRight() {
  leftStepDir  = leftFwdDir;
  rightStepDir = rightFwdDir;
  leftStepPeriodUs  = FAST_PERIOD_US;
  rightStepPeriodUs = SLOW_PERIOD_US;
  motorsRunning = true;
}

// Non-blocking step generator with per-motor timing so the two wheels can
// run at different speeds simultaneously (= continuous curve while moving).
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
// Tunables — continuous gradient-ascent pursuit (always moving, curves while
// driving so the predator never loses ground to discrete turn-stop pauses).
// ============================================================================
// How often the predator re-evaluates the RSSI gradient. Shorter = more
// reactive but noisier; longer = smoother but less responsive.
const unsigned long DECISION_INTERVAL_MS = 1000;

// Minimum RSSI change (dBm) over a decision interval to count as "progress."
// Negative threshold means small RSSI drops still count as "OK" — critical
// when chasing a moving target (which always slightly drops your RSSI even
// when you're aimed correctly, because the target is moving sideways too).
const int RSSI_IMPROVE_THRESHOLD = -2;

// Below this avg RSSI, signal is too weak to act on — treat as lost.
const int RSSI_LOST = -85;
const unsigned long PACKET_TIMEOUT_MS = 1000;

// ============================================================================
// Beacon broadcast — so the prey can read this robot's RSSI.
// Broadcast addr means we don't track peer MACs; any ESP-NOW listener on the
// channel hears it. Same scheme on the prey side keeps things symmetric.
// ============================================================================
uint8_t broadcastAddr[6] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
const unsigned long BEACON_INTERVAL_MS = 200;   // 5 Hz
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
DriveMode lastGoodCurve = INITIAL_CURVE;      // overrideable per robot (see top of file)
int consecutiveBadInCurve = 0;                // streak of bad decisions in the *same* curve direction
const int FLIP_THRESHOLD = 3;                 // after this many bad decisions in one direction, flip

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

// ============================================================================
// Receive callback — filters by source MAC, then feeds RSSI average.
// Only packets from TARGET_MAC count. Broadcasts from the other (non-target)
// robot get ignored, which is what makes the triangle work.
// ============================================================================
void onReceive(const esp_now_recv_info *info, const uint8_t *data, int len) {
  if (len != sizeof(incomingMsg)) return;
  if (memcmp(info->src_addr, TARGET_MAC, 6) != 0) return;   // not our target

  memcpy(&incomingMsg, data, sizeof(incomingMsg));
  lastPacketMillis = millis();

  int rssi = info->rx_ctrl->rssi;
  rssiBuf[rssiIdx++] = rssi;
  if (rssiIdx >= AVG_WINDOW) { rssiIdx = 0; rssiFilled = true; }
}

// ============================================================================
// Setup / loop
// ============================================================================
void setup() {
  Serial.begin(115200);
  delay(500);
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
  // Step pulse generator — must run every loop iteration when motors active.
  motorsTick();

  // Broadcast beacon at 5 Hz regardless of phase, so the prey always has
  // a fresh RSSI sample even while we're scanning or driving.
  if (millis() - lastBeaconMillis >= BEACON_INTERVAL_MS) {
    beaconId++;
    outgoingMsg.packet_id = beaconId;
    esp_now_send(broadcastAddr, (uint8_t*)&outgoingMsg, sizeof(outgoingMsg));
    lastBeaconMillis = millis();
  }

  // Watchdog: lost contact → idle, regardless of phase
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

  // Phase is DRIVING — we're always moving. Periodically reassess mode.
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
    // Coming out of STRAIGHT into a curve — start with last successful direction.
    nextMode = lastGoodCurve;
    consecutiveBadInCurve = 1;
    Serial.print(" → start lastGoodCurve");
  } else {
    // Already curving and still bad. Commit to this direction for a while
    // before flipping — single-cycle alternation can't accomplish a U-turn.
    consecutiveBadInCurve++;
    if (consecutiveBadInCurve >= FLIP_THRESHOLD) {
      nextMode = (driveMode == CURVE_LEFT_MODE) ? CURVE_RIGHT_MODE : CURVE_LEFT_MODE;
      consecutiveBadInCurve = 1;
      Serial.print(" → FLIP curve direction");
    } else {
      nextMode = driveMode;  // keep curving the same way
      Serial.print(" → continue curve (committed)");
    }
  }
  Serial.println();

  enterDRIVING(nextMode);
}
