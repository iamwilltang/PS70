#include "WiFi.h"

void setup(){
  Serial.begin(115200);
  delay(1000);
  WiFi.mode(WIFI_MODE_STA);
}

void loop(){
  Serial.print("MAC: ");
  Serial.println(WiFi.macAddress());
  delay(1000);
}