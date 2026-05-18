const int motorPin = 3; 

// Set your speed here! 
// 0 = Fastest. 255 = Stopped.
// Try 20 as a starting point for a slow crawl.
int motorSpeed = 30; 

void setup() {
  pinMode(motorPin, OUTPUT);
}

void loop() {
  analogWrite(motorPin, motorSpeed); 
}