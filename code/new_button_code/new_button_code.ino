// Pins
const int potPin = 32;     // Potentiometer speed control
const int pwm1 = 27;       // Motor driver input 1
const int pwm2 = 33;       // Motor driver input 2
const int button = 15;     // Button pin

// PWM setup for ESP32
const int pwmFreq = 5000;
const int pwmResolution = 8; // 8-bit: 0–255

// State Variables
enum SystemState { IDLE, DISPENSING, RETRACTING };
SystemState currentState = IDLE;

void setup() {
  Serial.begin(115200);

  pinMode(button, INPUT_PULLUP);

  // Attach PWM channels to motor driver pins
  ledcAttach(pwm1, pwmFreq, pwmResolution);
  ledcAttach(pwm2, pwmFreq, pwmResolution);

  stopActuator();
}

void loop() {
  handleButton();
  runStateLogic();
}

void handleButton() {
  bool buttonDown = (digitalRead(button) == LOW);
  static bool lastButtonState = false;

  static unsigned long lastReleaseTime = 0;
  static bool waitingForSecondClick = false;

  unsigned long now = millis();

  // Detect button press
  if (buttonDown && !lastButtonState) {
    // If second press within 400 ms → double click
    if (waitingForSecondClick && (now - lastReleaseTime < 400)) {
      Serial.println("Double Click: Retracting...");
      currentState = RETRACTING;
      waitingForSecondClick = false;
    }
  }

  // While holding → DISPENSING
  if (buttonDown) {
    if (currentState != RETRACTING) {
      currentState = DISPENSING;
    }
  }

  // Detect release
  if (!buttonDown && lastButtonState) {
    if (currentState == DISPENSING) {
      Serial.println("Released: Stopping.");
      currentState = IDLE;
    }

    lastReleaseTime = now;
    waitingForSecondClick = true;
  }

  // Timeout double-click window
  if (waitingForSecondClick && (now - lastReleaseTime > 400)) {
    waitingForSecondClick = false;
  }

  lastButtonState = buttonDown;
}

void runStateLogic() {
  int speedPWM = getSpeedPWM();

  switch (currentState) {
    case DISPENSING:
      extend(speedPWM);
      Serial.print("Dispensing | Speed PWM: ");
      Serial.println(speedPWM);
      break;

    case RETRACTING:
      retract(speedPWM);
      Serial.print("Retracting | Speed PWM: ");
      Serial.println(speedPWM);
      break;

    case IDLE:
      stopActuator();
      break;
  }
}

int getSpeedPWM() {
  int potValue = analogRead(potPin); // ESP32 reads 0–4095

  int speedPWM = map(potValue, 0, 4095, 0, 255);

  // Prevent weak buzzing at very low speed
  if (speedPWM < 40) {
    speedPWM = 0;
  }

  return speedPWM;
}

void extend(int speedPWM) {
  ledcWrite(pwm1, speedPWM);
  ledcWrite(pwm2, 0);
}

void retract(int speedPWM) {
  ledcWrite(pwm1, 0);
  ledcWrite(pwm2, speedPWM);
}

void stopActuator() {
  ledcWrite(pwm1, 0);
  ledcWrite(pwm2, 0);
}