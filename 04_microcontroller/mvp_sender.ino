#include <esp_now.h>
#include <WiFi.h>

uint8_t peerAddress[] = {0x58, 0x8C, 0x81, 0x9F, 0xBA, 0xA8};

typedef struct struct_message {
  uint32_t packet_id;
} struct_message;

struct_message msg;
esp_now_peer_info_t peerInfo;

void setup() {
  Serial.begin(115200);
  delay(500);

  WiFi.mode(WIFI_STA);

  if (esp_now_init() != ESP_OK) {
    Serial.println("ESP-NOW init failed");
    return;
  }

  memset(&peerInfo, 0, sizeof(peerInfo));
  memcpy(peerInfo.peer_addr, peerAddress, 6);
  peerInfo.channel = 0;
  peerInfo.encrypt = false;

  if (esp_now_add_peer(&peerInfo) != ESP_OK) {
    Serial.println("Failed to add peer");
    return;
  }

  msg.packet_id = 0;
}

void loop() {
  msg.packet_id++;
  esp_now_send(peerAddress, (uint8_t *)&msg, sizeof(msg));
  Serial.print("Sent packet ");
  Serial.println(msg.packet_id);
  delay(200);  // 5 Hz
}