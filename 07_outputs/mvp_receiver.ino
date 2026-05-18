#include <esp_now.h>
#include <WiFi.h>

const int motorPin = D1;   // gate-control pin for the P-MOSFET (needs external pull-up to VCC)

typedef struct struct_message {
  uint32_t packet_id;
} struct_message;

struct_message incomingMsg;

// RSSI smoothing
const int WINDOW = 10;
int rssiBuffer[WINDOW];
int rssiIndex = 0;
bool bufferFilled = false;

// Follow behavior: motor ON when leader is far (weak signal), OFF when caught up.
// Hysteresis: start chasing once below FAR_ON, stop once back above FAR_OFF.
// Calibrated for indoor short-range demo: ~-28 dBm at touching, ~-50 dBm at 2.5 m.
const int RSSI_FAR_ON  = -45;   // ~1.5 m away → start chasing
const int RSSI_FAR_OFF = -35;   // ~30–50 cm away → caught up, stop

// Watchdog: if no packet for this long, force motor OFF.
const unsigned long PACKET_TIMEOUT_MS = 1000;

volatile bool motorOn = false;
volatile unsigned long lastPacketMillis = 0;

float avgRSSI() {
  int count = bufferFilled ? WINDOW : rssiIndex;
  if (count == 0) return -100.0;
  long sum = 0;
  for (int i = 0; i < count; i++) sum += rssiBuffer[i];
  return (float)sum / count;
}

const char* classify(float rssi) {
  if (rssi > -55) return "NEAR";
  if (rssi > -70) return "MID";
  return "FAR";
}

void setMotor(bool on) {
  if (on == motorOn) return;
  motorOn = on;
  if (on) {
    pinMode(motorPin, OUTPUT);
    digitalWrite(motorPin, LOW);   // P-MOSFET ON
  } else {
    pinMode(motorPin, INPUT);      // pull-up holds gate high = OFF
  }
}

void onReceive(const esp_now_recv_info *info, const uint8_t *incomingData, int len) {
  if (len != sizeof(incomingMsg)) {
    Serial.println("Unexpected packet size");
    return;
  }

  memcpy(&incomingMsg, incomingData, sizeof(incomingMsg));
  lastPacketMillis = millis();

  int rssi = info->rx_ctrl->rssi;
  rssiBuffer[rssiIndex++] = rssi;
  if (rssiIndex >= WINDOW) {
    rssiIndex = 0;
    bufferFilled = true;
  }

  float avg = avgRSSI();

  if (!motorOn && avg < RSSI_FAR_ON)        setMotor(true);
  else if (motorOn && avg > RSSI_FAR_OFF)   setMotor(false);

  Serial.print("id=");
  Serial.print(incomingMsg.packet_id);
  Serial.print(" raw=");
  Serial.print(rssi);
  Serial.print(" avg=");
  Serial.print(avg, 1);
  Serial.print(" class=");
  Serial.print(classify(avg));
  Serial.print(" motor=");
  Serial.println(motorOn ? "ON" : "OFF");
}

void setup() {
  Serial.begin(115200);
  delay(500);

  pinMode(motorPin, INPUT);   // boot-safe: pull-up keeps MOSFET off

  WiFi.mode(WIFI_STA);

  if (esp_now_init() != ESP_OK) {
    Serial.println("ESP-NOW init failed");
    return;
  }

  esp_now_register_recv_cb(onReceive);
}

void loop() {
  if (motorOn && (millis() - lastPacketMillis > PACKET_TIMEOUT_MS)) {
    setMotor(false);
    Serial.println("Watchdog: no packets, motor OFF");
  }
  delay(50);
}