#include <Wire.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <time.h>
#include "Adafruit_TCS34725.h"
#include "BluetoothSerial.h"
#include <TinyGPS++.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

// GPIO configuration
const int LED_PIN = 4;
const int BUZZER_PIN = 27;

// OLED setup
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET    -1
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

// Wi-Fi credentials
const char* ssid = "hiddennetwork";
const char* password = "hathi112002";

// ThingSpeak API
const char* writeAPIKey = "8DNP5XJGRWMKU8B2";

// NTP for IST
const char* ntpServer = "time.google.com";
const long gmtOffset_sec = 19800;
const int daylightOffset_sec = 0;

// Modules
BluetoothSerial BTSerial;
Adafruit_TCS34725 tcs = Adafruit_TCS34725(TCS34725_INTEGRATIONTIME_600MS, TCS34725_GAIN_60X);
HardwareSerial gpsSerial(2);
TinyGPSPlus gps;

// Dark calibration values
uint16_t darkR = 0, darkG = 0, darkB = 0;

void setup() {
  Serial.begin(115200);
  BTSerial.begin("FluorideMeter");

  pinMode(LED_PIN, OUTPUT);
  pinMode(BUZZER_PIN, OUTPUT);
  digitalWrite(LED_PIN, LOW);
  digitalWrite(BUZZER_PIN, LOW);

  // OLED Init
  if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
    while (1);
  }
  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);
  display.setTextSize(2);
  display.setCursor(15, 5);
  display.println("Fluorimeter");

  display.setTextSize(1);
  display.setCursor(10, 35);
  display.println("A reliable device for");
  display.setCursor(5, 47);
  display.println("fluoride detection");
  display.display();
  delay(3000);
  display.clearDisplay();

  // GPS Init
  gpsSerial.begin(9600, SERIAL_8N1, 16, 17);
  display.setCursor(0, 0);
  display.println("Initializing GPS...");
  display.display();

  unsigned long start = millis();
  while (!gps.location.isValid() && millis() - start < 10000) {
    while (gpsSerial.available()) gps.encode(gpsSerial.read());
    delay(200);
  }

  display.clearDisplay();
  display.setCursor(0, 0);
  if (gps.location.isValid()) display.println("✅ GPS Acquired");
  else display.println("❌ GPS Not Found");
  display.display();
  delay(1000);

  // TCS Sensor Init
  display.clearDisplay();
  display.setCursor(0, 0);
  if (!tcs.begin()) {
    display.println("❌ TCS Sensor Err");
    display.display();
    while (1);
  }
  display.println("✅ TCS Found");
  display.display();
  delay(1000);

  // Wi-Fi Init
  display.clearDisplay();
  display.setCursor(0, 0);
  display.println("Connecting Wi-Fi");
  display.display();

  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(300);
  }

  display.clearDisplay();
  display.setCursor(0, 0);
  display.println("✅ Wi-Fi OK");
  display.display();
  delay(1000);

  // Time Sync
  display.clearDisplay();
  display.setCursor(0, 0);
  display.println("Syncing Time...");
  display.display();

  configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);
  struct tm timeinfo;
  int attempt = 0;
  while (!getLocalTime(&timeinfo) && attempt < 20) {
    delay(500);
    attempt++;
  }

  display.clearDisplay();
  display.setCursor(0, 0);
  if (attempt < 20) display.println("✅ Time Synced");
  else display.println("❌ Time Sync Err");
  display.display();
  delay(1000);

  performDarkCalibration();

  display.clearDisplay();
  display.setCursor(0, 0);
  display.println("System Ready");
  display.println("Place Sample");
  display.display();
}

void loop() {
  digitalWrite(LED_PIN, HIGH);
  BTSerial.println("🔆 LED ON — Place and cover sample now...");
  display.clearDisplay();
  display.setCursor(0, 0);
  display.println("LED ON");
  display.println("Place Sample");
  display.display();
  delay(7000);  // ⏱️ Increased time to place sample

  uint16_t r, g, b, c;
  tcs.getRawData(&r, &g, &b, &c);

  int correctedR = max((int)r - (int)darkR, 0);
  int correctedG = max((int)g - (int)darkG, 0);
  int correctedB = max((int)b - (int)darkB, 0);
  int maxVal = max(max(correctedR, correctedG), correctedB);
  if (maxVal == 0) maxVal = 1;

  uint8_t normR = map(correctedR, 0, maxVal, 0, 255);
  uint8_t normG = map(correctedG, 0, maxVal, 0, 255);
  uint8_t normB = map(correctedB, 0, maxVal, 0, 255);

  struct tm timeinfo;
  getLocalTime(&timeinfo);
  char timestamp[30];
  strftime(timestamp, sizeof(timestamp), "%Y-%m-%d %H:%M:%S", &timeinfo);

  while (gpsSerial.available()) gps.encode(gpsSerial.read());
  double latitude = gps.location.lat();
  double longitude = gps.location.lng();

  String result = "📊 Sample Reading:\n";
  result += "Time: " + String(timestamp) + "\n";
  result += "Norm RGB: R=" + String(normR) + " G=" + String(normG) + " B=" + String(normB) + "\n";
  result += "Lat: " + String(latitude, 6) + " Lon: " + String(longitude, 6);

  Serial.println(result);
  BTSerial.println(result);

  display.clearDisplay();
  display.setCursor(0, 0);
  display.println("Time:");
  display.println(timestamp);
  display.print("R:"); display.print(normR);
  display.print(" G:"); display.print(normG);
  display.print(" B:"); display.println(normB);
  display.print("Lat:"); display.println(latitude, 4);
  display.print("Lon:"); display.println(longitude, 4);
  display.display();

  delay(8000);  // 🖥️ Show result for 8 seconds

  digitalWrite(BUZZER_PIN, HIGH);
  delay(500);
  digitalWrite(BUZZER_PIN, LOW);

  display.clearDisplay();
  display.setCursor(0, 0);
  display.println("Uploading...");
  display.display();

  sendToThingSpeak(normR, normG, normB, latitude, longitude);

  digitalWrite(LED_PIN, LOW);
  display.clearDisplay();
  display.setCursor(0, 0);
  display.println("LED OFF");
  display.display();
  delay(500);     // 🔅 Brief LED OFF display
  delay(15500);   // Total delay = 16s (ThingSpeak limit)
}

void performDarkCalibration() {
  display.clearDisplay();
  display.setCursor(0, 0);
  display.println("Dark Calibrating...");
  display.display();

  digitalWrite(LED_PIN, LOW);
  delay(2000);

  uint16_t r, g, b, c;
  tcs.getRawData(&r, &g, &b, &c);
  darkR = r;
  darkG = g;
  darkB = b;

  display.clearDisplay();
  display.setCursor(0, 0);
  display.println("Dark Cal Done");
  display.print("R:"); display.println(darkR);
  display.print("G:"); display.println(darkG);
  display.print("B:"); display.println(darkB);
  display.display();
  delay(2000);
}

void sendToThingSpeak(int r, int g, int b, double lat, double lon) {
  if (WiFi.status() == WL_CONNECTED) {
    WiFiClient client;
    HTTPClient http;

    String url = "http://api.thingspeak.com/update?api_key=" + String(writeAPIKey) +
                 "&field1=" + String(r) +
                 "&field2=" + String(g) +
                 "&field3=" + String(b) +
                 "&field4=" + String(lat, 6) +
                 "&field5=" + String(lon, 6);

    http.begin(client, url);
    int responseCode = http.GET();

    display.clearDisplay();
    display.setCursor(0, 0);
    if (responseCode > 0) {
      display.println("✅ Upload OK");
    } else {
      display.println("❌ Upload Fail");
    }
    display.display();

    http.end();
  } else {
    display.clearDisplay();
    display.setCursor(0, 0);
    display.println("❌ Wi-Fi Lost");
    display.display();
  }
}
