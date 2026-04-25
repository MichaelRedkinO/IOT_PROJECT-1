#include <U8g2lib.h>
#include <math.h>

// -------- OLED SETUP ------------------------------------------
U8G2_SSD1306_128X64_NONAME_F_HW_I2C oled(U8G2_R0, U8X8_PIN_NONE);


// -------- PIN CONFIGURATION -----------------------------------
const int SOUND_PIN  = A0;
const int LIGHT_PIN  = A1;
// const int PIR_PIN = 2;   // PIR disabled for now
const int TEMP_PIN   = A2;
const int BUTTON_PIN = 3;


// -------- BUTTON VARIABLES ------------------------------------
bool lastButtonState = HIGH;
int displayMode = 0;
const int TOTAL_MODES = 5;


// -------- SENSOR VARIABLES ------------------------------------
int soundValue = 0;
int lightValue = 0;
int motionValue = 0;
float temperature = 0.0;
int focusScore = 100;
// ---------------------------------------------------------------


// -------- LIGHT RAPID CHANGE ----------------------------------
int previousLightValue = 0;
unsigned long rapidChangeStartTime = 0;
bool rapidLightDetected = false;

const int LIGHT_CHANGE_THRESHOLD = 150;
const unsigned long RAPID_WINDOW = 5000;
// ---------------------------------------------------------------


// -------- THRESHOLDS ------------------------------------------
const int SOUND_THRESHOLD = 600;
const int LIGHT_LOW = 200;
const int LIGHT_HIGH = 800;
const float TEMP_HIGH = 30.0;
// ---------------------------------------------------------------


void setup() {

  oled.begin();
  pinMode(PIR_PIN, INPUT);
  pinMode(BUTTON_PIN, INPUT_PULLUP);
}


void loop() {

  // ================= BUTTON SWITCH =================
  int currentButtonState = digitalRead(BUTTON_PIN);

  if (currentButtonState == LOW && lastButtonState == HIGH) {
    displayMode++;
    if (displayMode >= TOTAL_MODES) {
      displayMode = 0;
    }
  }

  lastButtonState = currentButtonState;
  // =================================================


  // ================= READ SENSORS ==================
  soundValue = analogRead(SOUND_PIN);
  lightValue = analogRead(LIGHT_PIN);
  motionValue = digitalRead(PIR_PIN);

  int tempRaw = analogRead(TEMP_PIN);
  temperature = (tempRaw * 5.0 / 1023.0) * 100.0;
  // =================================================


  // =========== RAPID LIGHT DETECTION ==============
  int lightDifference = abs(lightValue - previousLightValue);

  if (lightDifference > LIGHT_CHANGE_THRESHOLD) {

    if (rapidChangeStartTime == 0) {
      rapidChangeStartTime = millis();
    }

    if (millis() - rapidChangeStartTime <= RAPID_WINDOW) {
      rapidLightDetected = true;
    }

  } else {
    rapidChangeStartTime = 0;
    rapidLightDetected = false;
  }

  previousLightValue = lightValue;
  // =================================================


  // ================= FOCUS LOGIC ===================
  if (soundValue > SOUND_THRESHOLD) focusScore -= 2;
  if (lightValue < LIGHT_LOW || lightValue > LIGHT_HIGH) focusScore -= 1;
  if (motionValue == HIGH) focusScore -= 2;
  if (temperature > TEMP_HIGH) focusScore -= 1;
  if (rapidLightDetected) focusScore -= 2;

  if (focusScore < 0) focusScore = 0;
  if (focusScore > 100) focusScore = 100;
  // =================================================


  // ================= OLED DISPLAY ==================
  oled.clearBuffer();
  oled.setFont(u8g2_font_ncenB08_tr);

  switch(displayMode) {

    case 0:
      oled.drawStr(0,15,"Light:");
      if (rapidLightDetected) {
        oled.drawStr(0,35,"Rapid Change!");
      } else {
        oled.setCursor(0,35);
        oled.print(lightValue);
      }
      break;

    case 1:
      oled.drawStr(0,15,"Motion:");
      if (motionValue == HIGH)
        oled.drawStr(0,35,"Detected");
      else
        oled.drawStr(0,35,"Clear");
      break;

    case 2:
      oled.drawStr(0,15,"Sound:");
      oled.setCursor(0,35);
      oled.print(soundValue);
      break;

    case 3:
      oled.drawStr(0,15,"Temperature:");
      oled.setCursor(0,35);
      oled.print(temperature);
      oled.print(" C");
      break;

    case 4:
      oled.drawStr(0,15,"Focus Score:");
      oled.setCursor(0,35);
      oled.print(focusScore);
      break;
  }

  oled.sendBuffer();
  // =================================================

  delay(200);
}
