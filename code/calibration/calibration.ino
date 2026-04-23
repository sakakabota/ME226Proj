const int potPin = 26;    // A0 
const int pwm1 = 27;      // A1 
const int pwm2 = 33;      // A2

void setup() {
  Serial.begin(115200);
  pinMode(pwm1, OUTPUT);
  pinMode(pwm2, OUTPUT);
  
  Serial.println("--- CALIBRATION MODE ---");
  Serial.println("Use Maker Drive buttons to move actuator.");
  Serial.println("Record the values for Full Retraction and Full Extension.");
}

void loop() {
  int rawValue = analogRead(potPin);
  Serial.print("Current Potentiometer Value: ");
  Serial.println(rawValue);
  delay(100);
}
