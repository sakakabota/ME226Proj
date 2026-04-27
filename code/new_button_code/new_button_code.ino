#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

// --- PINS ---
const int speedPotPin = 32;
const int positionPotPin = 26;
const int pwm1 = 27;
const int pwm2 = 33;
const int button = 15;

// --- SETTINGS ---
const int pwmFreq = 5000;
const int pwmResolution = 8;
const int POS_MIN = 150;
const int POS_MAX = 3800;
const int AVG_SAMPLES = 15;
const int MOTOR_MIN_PWM = 85; 
const int CLICK_WINDOW = 600;

// --- GLOBALS ---
Adafruit_SSD1306 display(128, 32, &Wire, -1);
enum SystemState { IDLE, DISPENSING, RETRACTING, BOOT_HOME };
SystemState currentState = BOOT_HOME;

int speedHistory[AVG_SAMPLES];
int posHistory[AVG_SAMPLES];
int readIndex = 0;

SystemState lastState = BOOT_HOME;
int lastLoggedPos = 0;
int lastLoggedSpeed = -1;

void setup() {
  Serial.begin(115200);
  pinMode(button, INPUT_PULLUP);

  ledcAttach(pwm1, pwmFreq, pwmResolution);
  ledcAttach(pwm2, pwmFreq, pwmResolution);

  if(!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
    for(;;); 
  }

  int initialRead = analogRead(positionPotPin);
  for(int i = 0; i < AVG_SAMPLES; i++) {
    posHistory[i] = initialRead;
    speedHistory[i] = analogRead(speedPotPin);
  }

  showDebug("STARTUP", "HOMING");
  
  while (getMovingAverage(positionPotPin, posHistory) > POS_MIN) {
    retract(200);
    delay(10);
  }
  
  stopActuator();
  currentState = IDLE;
  showDebug("HOMED", "READY");
}

void loop() {
  int currentPos = getMovingAverage(positionPotPin, posHistory);
  int rawSpeed = getMovingAverage(speedPotPin, speedHistory);
  int speedPercent = map(rawSpeed, 0, 4095, 0, 100);
  
  int speedPWM = 0;
  if (speedPercent > 5) {
    speedPWM = map(speedPercent, 5, 100, MOTOR_MIN_PWM, 255);
  }

  handleButton();
  runStateLogic(speedPWM, currentPos);
  
  bool stateChanged = (currentState != lastState);
  bool posMoved = (abs(currentPos - lastLoggedPos) > 40);
  bool speedChanged = (abs(speedPercent - lastLoggedSpeed) > 5);

  if (stateChanged || posMoved || speedChanged) {
    String l1 = (currentState == DISPENSING) ? "DISPENSING" : (currentState == RETRACTING ? "RETRACTING" : "READY");
    String l2 = String(speedPercent) + "% " + String(currentPos);
    showDebug(l1, l2);
    
    lastState = currentState;
    lastLoggedPos = currentPos;
    lastLoggedSpeed = speedPercent;
  }
}

// --- BUTTON & MOTOR MODULES ---

void handleButton() {
  bool buttonDown = (digitalRead(button) == LOW); 
  static bool lastBtnState = false;
  static unsigned long firstClickTime = 0;
  static int clickCount = 0;
  unsigned long now = millis();

  if (buttonDown && !lastBtnState) {
    if (clickCount == 0 || (now - firstClickTime > CLICK_WINDOW)) {
      firstClickTime = now;
      clickCount = 1;
    } else {
      clickCount++;
    }

    if (clickCount == 3) {
      currentState = RETRACTING;
      clickCount = 0;
    }
  }

  if (buttonDown && currentState != RETRACTING) {
    currentState = DISPENSING;
  }

  if (!buttonDown && lastBtnState) {
    if (currentState == DISPENSING) currentState = IDLE;
  }

  lastBtnState = buttonDown;
}

void runStateLogic(int speed, int pos) {
  switch (currentState) {
    case DISPENSING:
      if (pos < POS_MAX) extend(speed);
      else { stopActuator(); currentState = IDLE; }
      break;
    case RETRACTING:
      if (pos > POS_MIN) retract(200);
      else currentState = IDLE;
      break;
    case IDLE:
      stopActuator();
      break;
  }
}

// --- HELPERS ---

int getMovingAverage(int pin, int* history) {
  long sum = 0;
  history[readIndex % AVG_SAMPLES] = analogRead(pin);
  for (int i = 0; i < AVG_SAMPLES; i++) sum += history[i];
  readIndex++;
  return sum / AVG_SAMPLES;
}

void extend(int speed)  { ledcWrite(pwm1, speed); ledcWrite(pwm2, 0); }
void retract(int speed) { ledcWrite(pwm1, 0); ledcWrite(pwm2, speed); }
void stopActuator()     { ledcWrite(pwm1, 0); ledcWrite(pwm2, 0); }

void showDebug(String line1, String line2) {
  Serial.println(line1 + " | " + line2);
  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);
  display.setTextSize(1);
  display.setCursor(0,0);
  display.print(line1);
  display.setTextSize(2);
  display.setCursor(0,14);
  display.print(line2);
  display.display();
}
