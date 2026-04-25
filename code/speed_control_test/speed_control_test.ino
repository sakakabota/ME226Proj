// Pins
const int potPosPin = 26;    // Actuator Position (Purple Wire)
const int potSpeedPin = 32;  // Flow Rate Knob (External Pot)
const int buttonPin = 13;    // Dispense Button
const int pwm1 = 27;         // Motor Drive IN1
const int pwm2 = 33;         // Motor Drive IN2

// Calibration
const int MIN_POS = 150;     // Fully Retracted (Home)
const int MAX_POS = 3800;    // Fully Extended

// Setup
unsigned long lastPressTime = 0;
bool isRetracting = false;
int lastSpeedReported = -1;

void setup() {
  Serial.begin(115200);
  pinMode(buttonPin, INPUT_PULLUP);
  
  // ESP32 PWM Setup
  ledcAttach(pwm1, 5000, 8); // 5kHz, 8-bit
  ledcAttach(pwm2, 5000, 8);
  
  Serial.println("READY: Hold to Dispense, Double-Click to Home.");
}

void loop() {
  int currentPos = analogRead(potPosPin);
  int knobValue = analogRead(potSpeedPin);
  
  // Calculate flow metrics
  int motorDuty = map(knobValue, 0, 4095, 0, 255);
  int speedPercent = map(knobValue, 0, 4095, 0, 100);

  // Only show speed change if knob moves by more than 2%
  if (abs(speedPercent - lastSpeedReported) > 2) {
    Serial.print("FLOW RATE: ");
    Serial.print(speedPercent);
    Serial.println("%");
    lastSpeedReported = speedPercent;
  }

  bool buttonPressed = (digitalRead(buttonPin) == LOW);

  // Double click button = retract back to start
  static bool lastButtonState = false;
  if (buttonPressed && !lastButtonState) {
    unsigned long now = millis();
    if (now - lastPressTime < 400) {
      isRetracting = true;
      Serial.println("Double click: back to start");
    }
    lastPressTime = now;
  }
  lastButtonState = buttonPressed;

  
  if (isRetracting) {
    // AUTOMATIC RETRACT
    if (currentPos > MIN_POS) {
      moveActuator(motorDuty, false); // Retract at knob speed
    } else {
      stopMotor();
      isRetracting = false;
      Serial.println("Min position.");
    }
  } 
  else if (buttonPressed) {
    // Press and Hold to Dispense, stop to release
    if (currentPos < MAX_POS) {
      moveActuator(motorDuty, true);
      Serial.print("Dispensing at ");
      Serial.print(speedPercent);
      Serial.print("% Pos: ");
      Serial.println(currentPos);
    } else {
      stopMotor();
      Serial.println("Max Position.");
    }
  } 
  else {

    stopMotor();
  }

  delay(10); 
}

void moveActuator(int duty, bool forward) {
  if (forward) {
    ledcWrite(pwm1, duty);
    ledcWrite(pwm2, 0);
  } else {
    ledcWrite(pwm1, 0);
    ledcWrite(pwm2, duty);
  }
}

void stopMotor() {
  ledcWrite(pwm1, 0);
  ledcWrite(pwm2, 0);
}
