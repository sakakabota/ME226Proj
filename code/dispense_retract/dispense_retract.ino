// Pins
const int potPin = 26; 
const int pwm1 = 27;   
const int pwm2 = 33;   
const int button = 15; // Change this to your actual button pin


const int VAL_RETRACTED = 150; 
const int VAL_MAX_PUSH = 3800; 

// State Variables
enum SystemState { IDLE, DISPENSING, RETRACTING };
SystemState currentState = IDLE;

unsigned long lastPressTime = 0;
int pressCount = 0;

void setup() {
  Serial.begin(115200);
  pinMode(pwm1, OUTPUT);
  pinMode(pwm2, OUTPUT);
  pinMode(button, INPUT_PULLUP);
  stopActuator();
}

void loop() {
  handleButton();
  runStateLogic();
}

void handleButton() {
  bool buttonDown = (digitalRead(button) == LOW);
  static bool lastButtonState = false;

  // Simple click detection
  if (buttonDown && !lastButtonState) {
    unsigned long now = millis();
    
    // Check for double click (within 400ms)
    if (now - lastPressTime < 400) {
      pressCount++;
    } else {
      pressCount = 1;
    }
    lastPressTime = now;
    
    // Logic for Single vs Double
    delay(50); // Small debounce
  }
  
  lastButtonState = buttonDown;

  // Process the clicks after a short window
  if (pressCount > 0 && (millis() - lastPressTime > 400)) {
    if (pressCount >= 2) {
      Serial.println("Double Click: Returning to Zero...");
      currentState = RETRACTING;
    } else {
      if (currentState == DISPENSING) {
        Serial.println("Single Click: Stopping.");
        currentState = IDLE;
      } else {
        Serial.println("Single Click: Dispensing...");
        currentState = DISPENSING;
      }
    }
    pressCount = 0; // Reset counter
  }
}

void runStateLogic() {
  int currentPos = analogRead(potPin);

  switch (currentState) {
    case DISPENSING:
      if (currentPos < VAL_MAX_PUSH) {
        extend();
      } else {
        Serial.println("Auto-Stop: Max Extension Reached.");
        currentState = IDLE;
      }
      break;

    case RETRACTING:
      if (currentPos > VAL_RETRACTED) {
        retract();
      } else {
        Serial.println("Home: Fully Retracted.");
        currentState = IDLE;
      }
      break;

    case IDLE:
      stopActuator();
      break;
  }
}

void extend() {
  digitalWrite(pwm1, HIGH);
  digitalWrite(pwm2, LOW);
}

void retract() {
  digitalWrite(pwm1, LOW);
  digitalWrite(pwm2, HIGH);
}

void stopActuator() {
  digitalWrite(pwm1, LOW);
  digitalWrite(pwm2, LOW);
}
