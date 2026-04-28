#include <U8g2lib.h>
#include <math.h>
#include <WiFiS3.h>
#include <ArduinoHttpClient.h>
#include "arduino_secrets.h"

// -------- OLED SETUP ------------------------------------------
// i think this is correct for my screen, didnt change it
U8G2_SSD1306_128X64_NONAME_F_HW_I2C oled(U8G2_R0, U8X8_PIN_NONE);

// -------- PIN CONFIGURATION -----------------------------------
const int SOUND_PIN  = A0;
const int LIGHT_PIN  = A1;
// const int PIR_PIN = 2;  // PIR disabled for now (sensor broken i think)
const int TEMP_PIN   = A2;
const int BUTTON_PIN = 3;

// -------- BUTTON VARIABLES ------------------------------------
bool lastButtonState = HIGH;
int displayMode = 0;
const int TOTAL_MODES = 5;

// -------- SENSOR VARIABLES ------------------------------------
int soundValue = 0;
int lightValue = 0;
float temperature = 0.0;
int focusScore = 100;

// -------- LIGHT RAPID CHANGE ----------------------------------
int previousLightValue = 0;
unsigned long rapidChangeStartTime = 0;
bool rapidLightDetected = false;

const int LIGHT_CHANGE_THRESHOLD = 150;
const unsigned long RAPID_WINDOW = 5000;

// -------- THRESHOLDS ------------------------------------------
const int SOUND_THRESHOLD = 600;
const int LIGHT_LOW = 200;
const int LIGHT_HIGH = 800;
const float TEMP_HIGH = 30.0;

// =============================================================
// SETUP
// =============================================================
void setup() {
  oled.begin();
  Serial.begin(9600);

  pinMode(BUTTON_PIN, INPUT_PULLUP);

  connectWiFi(); // hope wifi connects fast lol
}

// =============================================================
// LOOP
// =============================================================
void loop() {

  handleButton();
  readSensors();
  detectRapidLight();
  calculateFocus();
  displayOLED();

  // sendToServer(); // old pushingbox, not using now
  sendToFirebase(); // sending data to firebase db

  delay(5000); // not too fast so server doesnt hate me
}

// =============================================================
// WIFI
// =============================================================
void connectWiFi() {
  Serial.print("Connecting to WiFi: ");
  Serial.println(SECRET_SSID);

  int status = WiFi.begin(SECRET_SSID, SECRET_PASS);

  while (status != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
    status = WiFi.status();
  }

  Serial.println("\nConnected!");
  Serial.println(WiFi.localIP());
}

// =============================================================
// BUTTON
// =============================================================
void handleButton() {
  int currentButtonState = digitalRead(BUTTON_PIN);

  if (currentButtonState == LOW && lastButtonState == HIGH) {
    displayMode++;
    if (displayMode >= TOTAL_MODES) displayMode = 0;
  }

  lastButtonState = currentButtonState;
}

// =============================================================
// SENSOR READ
// =============================================================
void readSensors() {
  soundValue = analogRead(SOUND_PIN);
  lightValue = analogRead(LIGHT_PIN);

  // temp sometimes was crazy like 200C lol, fixing that
  temperature = readTemperature();
}

// =============================================================
// TEMPERATURE (FIXED)
// =============================================================
float readTemperature() {
  int tempRaw = analogRead(TEMP_PIN);

  if (tempRaw == 0) return 0; // avoid crash (happened once)

  float resistance = (1023.0 / tempRaw - 1.0) * 10000.0;
  float tempC = 1.0 / (log(resistance / 10000.0) / 3975.0 + 1 / 298.15) - 273.15;

  return tempC;
}

// =============================================================
// LIGHT CHANGE DETECTION
// =============================================================
void detectRapidLight() {

  int diff = abs(lightValue - previousLightValue);

  if (diff > LIGHT_CHANGE_THRESHOLD) {

    if (rapidChangeStartTime == 0) {
      rapidChangeStartTime = millis(); // start timer
    }

    if (millis() - rapidChangeStartTime <= RAPID_WINDOW) {
      rapidLightDetected = true;
    }

  } else {
    rapidChangeStartTime = 0;
    rapidLightDetected = false;
  }

  previousLightValue = lightValue;
}

// =============================================================
// FOCUS SCORE
// =============================================================
void calculateFocus() {

  focusScore = 100;

  if (soundValue > SOUND_THRESHOLD) focusScore -= 25;

  if (lightValue < LIGHT_LOW || lightValue > LIGHT_HIGH || rapidLightDetected)
    focusScore -= 25;

  if (temperature > TEMP_HIGH) focusScore -= 25;

  if (focusScore < 0) focusScore = 0;
}

// =============================================================
// DISPLAY
// =============================================================
void displayOLED() {

  oled.clearBuffer();
  oled.setFont(u8g2_font_ncenB08_tr);

  switch(displayMode) {

    case 0:
      oled.drawStr(0,15,"Light:");
      if (rapidLightDetected)
        oled.drawStr(0,35,"Rapid Change!");
      else {
        oled.setCursor(0,35);
        oled.print(lightValue);
      }
      break;

    case 1:
      oled.drawStr(0,15,"Motion:");
      oled.drawStr(0,35,"Disabled"); // pir not working :(
      break;

    case 2:
      oled.drawStr(0,15,"Sound:");
      oled.setCursor(0,35);
      oled.print(soundValue);
      break;

    case 3:
      oled.drawStr(0,15,"Temp:");
      oled.setCursor(0,35);
      oled.print(temperature);
      oled.print(" C");
      break;

    case 4:
      oled.drawStr(0,15,"Focus:");
      oled.setCursor(0,35);
      oled.print(focusScore);
      break;
  }

  oled.sendBuffer();
}

// =============================================================
// FIREBASE SEND
// =============================================================
void sendToFirebase() {

  if (WiFi.status() != WL_CONNECTED) {
    connectWiFi();
  }

  // fixed host (no https!!)
  String firebaseHost = "asec-iot-default-rtdb.europe-west1.firebasedatabase.app";

  WiFiClient client;

  if (client.connect(firebaseHost.c_str(), 80)) {

    String url = "/asec.json";

    String jsonData = "{";
    jsonData += "\"temperature\":" + String(temperature) + ",";
    jsonData += "\"light\":" + String(lightValue) + ",";
    jsonData += "\"sound\":" + String(soundValue) + ",";
    jsonData += "\"focus\":" + String(focusScore);
    jsonData += "}";

    client.println("PUT " + url + " HTTP/1.1");
    client.println("Host: " + firebaseHost);
    client.println("Content-Type: application/json");
    client.print("Content-Length: ");
    client.println(jsonData.length());
    client.println();
    client.println(jsonData);

    Serial.println("sent to firebase:");
    Serial.println(jsonData);

  } else {
    Serial.println("firebase connection failed...");
  }

  client.stop();
}
