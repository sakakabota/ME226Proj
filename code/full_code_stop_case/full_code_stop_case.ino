#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

// --- PINS ---
const int speedPotPin = 2;      
const int positionPotPin = 3;   
const int pwm1 = 4;             
const int pwm2 = 5;             
const int button = 21; 
const int SAFE_MAX_POS = 2400;  // stop before cover jams         

// --- SETTINGS ---
const int pwmFreq = 5000;
const int pwmResolution = 8;
const int POS_MIN = 250;
const int POS_MAX = 3800;
const int AVG_SAMPLES = 5; 
const int MOTOR_MIN_PWM = 85; 
const int CLICK_WINDOW = 600;
const unsigned long IDLE_TIMEOUT = 15000;

// --- JAM DETECTION SETTINGS ---
const int JAM_THRESHOLD = 5;    // Min change in ADC to not be "stalled"
const int JAM_TIMEOUT = 500;     // ms to wait before E-Stop
unsigned long lastMoveTime = 0;
int lastPosCheck = 0;
bool isJammed = false;

// --- GLOBALS ---
Adafruit_SSD1306 display(128, 32, &Wire, -1);
enum SystemState { IDLE, DISPENSING, RETRACTING, BOOT_HOME, MAX_REACHED, JAMMED };
SystemState currentState = BOOT_HOME;

int speedHistory[AVG_SAMPLES];
int posHistory[AVG_SAMPLES];
int posIndex = 0;
int speedIndex = 0;
int lastSmoothSpeed = 0;
unsigned long lastActivityTime = 0;

void setup() {
  pinMode(button, INPUT_PULLUP);
  ledcAttach(pwm1, pwmFreq, pwmResolution);
  ledcAttach(pwm2, pwmFreq, pwmResolution);

  if(!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) for(;;); 

  int initialReadPos = analogRead(positionPotPin);
  int initialReadSpeed = analogRead(speedPotPin);
  for(int i = 0; i < AVG_SAMPLES; i++) {
    posHistory[i] = initialReadPos;
    speedHistory[i] = initialReadSpeed;
  }

  // --- STARTUP SCANNING ANIMATION ---
  while (getPosAverage() > POS_MIN) {
    retract(200);
    drawScanningAnim("HOMING..."); 
  }
  
  stopActuator();
  drawSuccess();
  currentState = IDLE;
  lastActivityTime = millis();
}

void loop() {
  int currentPos = getPosAverage();
  int smoothRawSpeed = getSpeedAverage();
  int targetPercent = map(smoothRawSpeed, 0, 4095, 0, 100);
  
  if (abs(targetPercent - lastSmoothSpeed) > 1) {
    lastSmoothSpeed = targetPercent;
    lastActivityTime = millis(); 
  }

  int speedPWM = 0;
  if (lastSmoothSpeed > 5) {
    speedPWM = map(lastSmoothSpeed, 5, 100, MOTOR_MIN_PWM, 255);
  }

  handleButton();
  runStateLogic(speedPWM, currentPos);
  
  // --- UI STATE LOGIC ---
  if (currentState == BOOT_HOME || currentState == RETRACTING) {
    drawScanningAnim("RETRACTING..."); 
  } else if (millis() - lastActivityTime > IDLE_TIMEOUT && currentState == IDLE) {
    drawIdleAnim(); 
  } else {
    updateDisplay(lastSmoothSpeed, currentPos); 
  }
}

// --- MODULES ---

void handleButton() {
  bool buttonDown = (digitalRead(button) == LOW); 
  static bool lastBtnState = false;
  static unsigned long firstClickTime = 0;
  static int clickCount = 0;
  unsigned long now = millis();

  if (buttonDown && !lastBtnState) {
    lastActivityTime = now; 
    if (clickCount == 0 || (now - firstClickTime > CLICK_WINDOW)) {
      firstClickTime = now;
      clickCount = 1;
    } else {
      clickCount++;
    }
    if (clickCount == 3) {
      currentState = RETRACTING;
      isJammed = false; // Reset jam on full retract
      clickCount = 0;
    }
  }
  
  if (buttonDown && currentState != RETRACTING) {
    if (currentState != MAX_REACHED && currentState != JAMMED) {
        currentState = DISPENSING;
    }
  }

  if (!buttonDown && lastBtnState) {
    if (currentState == DISPENSING || currentState == MAX_REACHED || currentState == JAMMED) {
        currentState = IDLE;
        isJammed = false; // Reset jam flag when user lets go
    }
  }
  lastBtnState = buttonDown;
}

void runStateLogic(int speed, int pos) {
  switch (currentState) {
    case DISPENSING:
      // JAM DETECTION: Check if position is changing
      if (abs(pos - lastPosCheck) > JAM_THRESHOLD) {
          lastPosCheck = pos;
          lastMoveTime = millis(); 
      }

      if (millis() - lastMoveTime > JAM_TIMEOUT) {
          stopActuator();
          retract(200); // Auto-retract 1 sec for pressure relief
          delay(1000);
          stopActuator();
          currentState = JAMMED;
          return;
      }

      if (pos < SAFE_MAX_POS) {
        extend(speed);
      } else { 
        stopActuator(); 
        currentState = MAX_REACHED; 
      }
      break;

    case RETRACTING:
      if (pos > POS_MIN) {
        retract(200);
      } else { 
        stopActuator(); 
        currentState = IDLE; 
      }
      break;

    case IDLE:
    case MAX_REACHED:
    case JAMMED:
      stopActuator();
      lastMoveTime = millis(); // Reset timers while idle
      lastPosCheck = pos;
      break;
  }
}

int getPosAverage() {
  long sum = 0;
  posHistory[posIndex] = analogRead(positionPotPin);
  posIndex = (posIndex + 1) % AVG_SAMPLES;
  for (int i = 0; i < AVG_SAMPLES; i++) { sum += posHistory[i]; }
  return sum / AVG_SAMPLES;
}

int getSpeedAverage() {
  long sum = 0;
  speedHistory[speedIndex] = analogRead(speedPotPin);
  speedIndex = (speedIndex + 1) % AVG_SAMPLES;
  for (int i = 0; i < AVG_SAMPLES; i++) { sum += speedHistory[i]; }
  return sum / AVG_SAMPLES;
}

void extend(int speed)  { ledcWrite(pwm1, speed); ledcWrite(pwm2, 0); }
void retract(int speed) { ledcWrite(pwm1, 0); ledcWrite(pwm2, speed); }
void stopActuator()     { ledcWrite(pwm1, 0); ledcWrite(pwm2, 0); }

// --- UI & ANIMATIONS ---

void updateDisplay(int speed, int pos) {
  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);
  display.setTextSize(1);
  display.setCursor(0,0);
  
  if (currentState == DISPENSING) display.print(">> DISPENSING");
  else if (currentState == MAX_REACHED) display.print("!! MAX POSITION !!");
  else if (currentState == JAMMED) display.print("!! JAM DETECTED !!");
  else display.print("READY");

  display.drawRect(0, 10, 128, 5, SSD1306_WHITE);
  display.fillRect(2, 11, map(speed, 0, 100, 0, 124), 3, SSD1306_WHITE);

  display.setTextSize(2);
  display.setCursor(0, 18); 
  display.print(speed); display.print("%");
  
  display.setTextSize(1);
  display.setCursor(85, 22);
  display.print("P:"); display.print(pos/10); 
  display.display();
}

void drawScanningAnim(String msg) {
  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);
  display.setTextSize(1);
  display.setCursor(35, 12);
  display.print(msg);
  int linePos = (millis() / 5) % 128;
  display.drawFastVLine(linePos, 22, 8, SSD1306_WHITE);
  display.display();
}

void drawSuccess() {
  display.clearDisplay();
  display.setTextSize(2);
  display.setCursor(35, 10);
  display.print("DONE!");
  display.display();
  delay(800);
}

void drawIdleAnim() {
  display.clearDisplay();
  int offset = (millis() / 1000) % 2; 
  display.setTextSize(1);
  display.setCursor(45, 10 + offset);
  display.print("z  Z  z");
  display.drawRoundRect(40, 22, 48, 8, 4, SSD1306_WHITE); 
  display.display();
}