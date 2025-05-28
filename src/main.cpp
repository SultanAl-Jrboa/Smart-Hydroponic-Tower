#include <OneWire.h>
#include <DallasTemperature.h>
#include <DHT.h>
#include <Adafruit_NeoPixel.h>
#include <Adafruit_GFX.h>
#include <Adafruit_ILI9341.h>
#include <SPI.h>
#include <math.h>
#include <DFRobot_EC.h>
#include <WiFi.h>
#include <WebServer.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <time.h>

// --- TFT Display
#define TFT_CS     5
#define TFT_RST    2
#define TFT_DC     4
#define TFT_SCLK   18
#define TFT_MOSI   23
#define TFT_LED    33

// --- Water Pump Relay
#define PUMP_RELAY_PIN 26

Adafruit_ILI9341 tft = Adafruit_ILI9341(TFT_CS, TFT_DC, TFT_RST);
WebServer server(80);

// Colors (RGB565 format)
#define BACKGROUND_COLOR      0xFFFF // White
#define HEADER_COLOR          0x1E69 // Dark green
#define CARD_BG_COLOR         0xEF7D // Light gray
#define PRIMARY_COLOR         0x2D03 // Dark green
#define SECONDARY_COLOR       0x0472 // Light green
#define HIGHLIGHT_COLOR       0x6652 // Green highlight
#define ALERT_COLOR           0xF800 // Red
#define BLACK_COLOR           0x0000 // Black
#define DARK_GRAY             0x632C // Dark gray
#define LIGHT_GRAY            0xCE59 // Light gray
#define OFF_WHITE             0xF79E // Off-white
#define ON_COLOR              0x07E0 // Green
#define OFF_COLOR             0xC618 // Gray
#define WARNING_COLOR         0xFD20 // Orange
#define TEXT_DARK             0x0000 // Black text
#define TEXT_LIGHT            0xFFFF // White text
#define TEXT_GREEN            0x0720 // Dark green text

// --- DS18B20 (Water Temp)
#define ONE_WIRE_BUS 19
OneWire oneWire(ONE_WIRE_BUS);
DallasTemperature waterTempSensor(&oneWire);

// --- DHT22 
#define DHTPIN 22
#define DHTTYPE DHT22
DHT dht(DHTPIN, DHTTYPE);

// --- TDS Sensor
#define TDS_PIN 36

// --- pH Sensor
#define PH_PIN 34

// --- EC Sensor  
#define EC_PIN 34
DFRobot_EC ec;

// --- LED Strip
#define LED_PIN    27
#define NUM_LEDS   140
#define BRIGHTNESS 200
Adafruit_NeoPixel strip(NUM_LEDS, LED_PIN, NEO_GRB + NEO_KHZ800);

// --- Global Variables
float waterTemp = 25.0;
float airTemp = 0.0;
float humidity = 0.0;
float tds_value = 0.0;
float phValue = 7.0;
float ecValue = 0.0;
float ecVoltage = 0.0;
bool ecCalibrated = false;
float kValue = 1.0;  // K value for EC calculation
bool manualCalibration = false;
long waterLevel = 78; // Fixed water level percentage
bool ledStatus = false;
bool pumpStatus = false;
int ledMode = 0;

// Pump control variables
bool autoPumpEnabled = true;  // Auto mode enabled by default
bool manualPumpOverride = false;  // Manual override flag
unsigned long lastPumpCycle = 0;  // Last time pump ran automatically
unsigned long pumpStartTime = 0;  // When current pump cycle started
bool pumpRunning = false;  // Current pump state
bool pumpManualControl = false;  // Manual control flag
const unsigned long PUMP_CYCLE_INTERVAL = 3600000;  // 1 hour in milliseconds
const unsigned long PUMP_RUN_DURATION = 600000;     // 10 minutes in milliseconds

// WiFi credentials
const char* ssid = "Traders Hotel";
const char* password = "";

// Firebase configuration
const char* firebaseHost = "https://hydrobrain-1f3c2-default-rtdb.firebaseio.com";

// Time configuration
const char* ntpServer = "pool.ntp.org";
const long gmtOffset_sec = 10800; // GMT+3 (adjust for your timezone)
const int daylightOffset_sec = 0;

float calculateECManual(float voltage, float temperature) {
  // Updated calibration based on actual readings
  // You measured 1.229V in 1413 µS/cm solution
  
  // Assuming distilled water gives ~0.0V (you should measure this)
  float voltage_zero = 0.0;  // Voltage in distilled water
  float voltage_1413 = 1.229; // Your actual measured voltage in 1413 µS/cm
  
  if (voltage <= voltage_zero) {
    return 0.0; // Below minimum, assume pure water
  }
  
  // Linear interpolation
  float slope = 1413.0 / (voltage_1413 - voltage_zero);
  float rawEC = slope * (voltage - voltage_zero);
  
  // Temperature compensation (2% per degree from 25°C)
  float tempCoeff = 1.0 + 0.02 * (temperature - 25.0);
  float compensatedEC = rawEC * tempCoeff;
  
  return compensatedEC;
}

void controlPump(bool state) {
  if (state) {
    digitalWrite(PUMP_RELAY_PIN, LOW);  // Assuming active LOW relay
    pumpStatus = true;
    pumpRunning = true;
    Serial.println("Pump ON");
  } else {
    digitalWrite(PUMP_RELAY_PIN, HIGH); // Assuming active LOW relay
    pumpStatus = false;
    pumpRunning = false;
    Serial.println("Pump OFF");
  }
}

void handlePumpControl() {
  unsigned long currentTime = millis();
  
  // Handle manual override
  if (manualPumpOverride) {
    return; // Don't run auto control when manual override is active
  }
  
  // Auto pump control
  if (autoPumpEnabled) {
    // Check if it's time to start a new pump cycle
    if (!pumpRunning && (currentTime - lastPumpCycle >= PUMP_CYCLE_INTERVAL)) {
      controlPump(true);
      pumpStartTime = currentTime;
      lastPumpCycle = currentTime;
      Serial.println("Auto pump cycle started");
    }
    
    // Check if current pump cycle should end
    if (pumpRunning && (currentTime - pumpStartTime >= PUMP_RUN_DURATION)) {
      controlPump(false);
      Serial.println("Auto pump cycle ended");
    }
  }
}

// Web API endpoints
void handleGetStatus() {
  Serial.println("=== Status Request Received ===");
  
  server.sendHeader("Access-Control-Allow-Origin", "*");
  server.sendHeader("Access-Control-Allow-Methods", "*");
  server.sendHeader("Access-Control-Allow-Headers", "*");
  server.sendHeader("Access-Control-Max-Age", "86400");
  
  JsonDocument doc;
  doc["waterTemp"] = waterTemp;
  doc["airTemp"] = airTemp;
  doc["humidity"] = humidity;
  doc["tds"] = tds_value;
  doc["ph"] = phValue;
  doc["ec"] = ecValue;
  doc["waterLevel"] = waterLevel;
  doc["ledStatus"] = ledStatus;
  doc["ledMode"] = ledMode;
  doc["pumpStatus"] = pumpStatus;
  doc["pumpRunning"] = pumpRunning;
  doc["autoPumpEnabled"] = autoPumpEnabled;
  doc["manualPumpOverride"] = manualPumpOverride;
  doc["deviceId"] = "HydroBrain-ESP32";
  doc["uptime"] = millis() / 1000;
  doc["freeHeap"] = ESP.getFreeHeap();
  doc["wifiRSSI"] = WiFi.RSSI();
  
  // Add timing info
  unsigned long currentTime = millis();
  unsigned long timeToNextCycle = 0;
  unsigned long timeRemaining = 0;
  
  if (pumpRunning) {
    timeRemaining = PUMP_RUN_DURATION - (currentTime - pumpStartTime);
    doc["pumpTimeRemaining"] = timeRemaining / 1000; // seconds
  } else {
    timeToNextCycle = PUMP_CYCLE_INTERVAL - (currentTime - lastPumpCycle);
    doc["timeToNextPumpCycle"] = timeToNextCycle / 1000; // seconds
  }
  
  String jsonString;
  serializeJson(doc, jsonString);
  
  Serial.print("Sending status response: ");
  Serial.println(jsonString.length());
  Serial.println("bytes");
  
  server.send(200, "application/json", jsonString);
  Serial.println("Status response sent successfully");
}

void handlePumpOn() {
  server.sendHeader("Access-Control-Allow-Origin", "*");
  server.sendHeader("Access-Control-Allow-Methods", "*");
  server.sendHeader("Access-Control-Allow-Headers", "*");
  server.sendHeader("Access-Control-Max-Age", "86400");
  
  manualPumpOverride = true;
  autoPumpEnabled = false;
  controlPump(true);
  
  JsonDocument doc;
  doc["status"] = "success";
  doc["message"] = "Pump turned ON manually";
  doc["pumpStatus"] = pumpStatus;
  doc["manualMode"] = true;
  
  String jsonString;
  serializeJson(doc, jsonString);
  server.send(200, "application/json", jsonString);
}

void handlePumpOff() {
  server.sendHeader("Access-Control-Allow-Origin", "*");
  server.sendHeader("Access-Control-Allow-Methods", "*");
  server.sendHeader("Access-Control-Allow-Headers", "*");
  server.sendHeader("Access-Control-Max-Age", "86400");
  
  manualPumpOverride = true;
  autoPumpEnabled = false;
  controlPump(false);
  
  JsonDocument doc;
  doc["status"] = "success";
  doc["message"] = "Pump turned OFF manually";
  doc["pumpStatus"] = pumpStatus;
  doc["manualMode"] = true;
  
  String jsonString;
  serializeJson(doc, jsonString);
  server.send(200, "application/json", jsonString);
}

void handlePumpAuto() {
  server.sendHeader("Access-Control-Allow-Origin", "*");
  server.sendHeader("Access-Control-Allow-Methods", "*");
  server.sendHeader("Access-Control-Allow-Headers", "*");
  server.sendHeader("Access-Control-Max-Age", "86400");
  
  manualPumpOverride = false;
  autoPumpEnabled = true;
  
  JsonDocument doc;
  doc["status"] = "success";
  doc["message"] = "Pump set to AUTO mode";
  doc["autoMode"] = true;
  
  String jsonString;
  serializeJson(doc, jsonString);
  server.send(200, "application/json", jsonString);
}

void handleLedGrowth() {
  Serial.println("=== LED Growth Mode Request Received ===");
  
  server.sendHeader("Access-Control-Allow-Origin", "*");
  server.sendHeader("Access-Control-Allow-Methods", "*");
  server.sendHeader("Access-Control-Allow-Headers", "*");
  server.sendHeader("Access-Control-Max-Age", "86400");
  
  // Growth Light - Warm white/yellow like sun (high red, medium green, low blue)
  Serial.println("Setting LEDs to Growth Mode (Warm Sunlight)");
  for (int i = 0; i < NUM_LEDS; i++) {
    strip.setPixelColor(i, strip.Color(255, 180, 80)); // Warm sunlight color
  }
  strip.show();
  ledStatus = true;
  ledMode = 1;
  
  Serial.println("LEDs updated successfully");
  
  JsonDocument doc;
  doc["status"] = "success";
  doc["message"] = "LED set to Growth Mode";
  doc["ledStatus"] = ledStatus;
  doc["ledMode"] = "growth";
  
  String jsonString;
  serializeJson(doc, jsonString);
  
  Serial.print("Sending response: ");
  Serial.println(jsonString);
  
  server.send(200, "application/json", jsonString);
  Serial.println("Response sent");
}

void handleLedRelax() {
  Serial.println("=== LED Relax Mode Request Received ===");
  
  server.sendHeader("Access-Control-Allow-Origin", "*");
  server.sendHeader("Access-Control-Allow-Methods", "*");
  server.sendHeader("Access-Control-Allow-Headers", "*");
  server.sendHeader("Access-Control-Max-Age", "86400");
  
  // Relaxing Light - Calm blue
  Serial.println("Setting LEDs to Relax Mode (Calm Blue)");
  for (int i = 0; i < NUM_LEDS; i++) {
    strip.setPixelColor(i, strip.Color(0, 100, 255)); // Calm blue
  }
  strip.show();
  ledStatus = true;
  ledMode = 2;
  
  Serial.println("LEDs updated successfully");
  
  JsonDocument doc;
  doc["status"] = "success";
  doc["message"] = "LED set to Relaxing Mode";
  doc["ledStatus"] = ledStatus;
  doc["ledMode"] = "relax";
  
  String jsonString;
  serializeJson(doc, jsonString);
  
  Serial.print("Sending response: ");
  Serial.println(jsonString);
  
  server.send(200, "application/json", jsonString);
  Serial.println("Response sent");
}

void handleLedSleep() {
  Serial.println("=== LED Sleep Mode Request Received ===");
  
  server.sendHeader("Access-Control-Allow-Origin", "*");
  server.sendHeader("Access-Control-Allow-Methods", "*");
  server.sendHeader("Access-Control-Allow-Headers", "*");
  server.sendHeader("Access-Control-Max-Age", "86400");
  
  // Sleeping Light - Soft red
  Serial.println("Setting LEDs to Sleep Mode (Soft Red)");
  for (int i = 0; i < NUM_LEDS; i++) {
    strip.setPixelColor(i, strip.Color(255, 50, 0)); // Soft red
  }
  strip.show();
  ledStatus = true;
  ledMode = 3;
  
  Serial.println("LEDs updated successfully");
  
  JsonDocument doc;
  doc["status"] = "success";
  doc["message"] = "LED set to Sleep Mode";
  doc["ledStatus"] = ledStatus;
  doc["ledMode"] = "sleep";
  
  String jsonString;
  serializeJson(doc, jsonString);
  
  Serial.print("Sending response: ");
  Serial.println(jsonString);
  
  server.send(200, "application/json", jsonString);
  Serial.println("Response sent");
}

void handleLedOff() {
  Serial.println("=== LED OFF Request Received ===");
  
  server.sendHeader("Access-Control-Allow-Origin", "*");
  server.sendHeader("Access-Control-Allow-Methods", "*");
  server.sendHeader("Access-Control-Allow-Headers", "*");
  server.sendHeader("Access-Control-Max-Age", "86400");
  
  // Turn off all LEDs
  Serial.println("Turning off all LEDs");
  for (int i = 0; i < NUM_LEDS; i++) {
    strip.setPixelColor(i, strip.Color(0, 0, 0));
  }
  strip.show();
  ledStatus = false;
  ledMode = 0;
  
  Serial.println("LEDs turned off successfully");
  
  JsonDocument doc;
  doc["status"] = "success";
  doc["message"] = "LED turned OFF";
  doc["ledStatus"] = ledStatus;
  
  String jsonString;
  serializeJson(doc, jsonString);
  
  Serial.print("Sending response: ");
  Serial.println(jsonString);
  
  server.send(200, "application/json", jsonString);
  Serial.println("Response sent");
}

void handleOptions() {
  server.sendHeader("Access-Control-Allow-Origin", "*");
  server.sendHeader("Access-Control-Allow-Methods", "*");
  server.sendHeader("Access-Control-Allow-Headers", "*");
  server.sendHeader("Access-Control-Max-Age", "86400");
  server.send(200, "text/plain", "");
}

void handleNotFound() {
  server.send(404, "text/plain", "Not Found");
}

void showIPAddressOnLED() {
  // Show IP address using LED colors
  // Get IP address
  IPAddress ip = WiFi.localIP();
  
  // Turn off all LEDs first
  for (int i = 0; i < NUM_LEDS; i++) {
    strip.setPixelColor(i, strip.Color(0, 0, 0));
  }
  strip.show();
  delay(1000);
  
  // Show each octet of IP address
  for (int octet = 0; octet < 4; octet++) {
    int value = ip[octet];
    
    // Show octet number with white flash
    for (int flash = 0; flash <= octet; flash++) {
      for (int i = 0; i < 10; i++) {
        strip.setPixelColor(i, strip.Color(255, 255, 255)); // White
      }
      strip.show();
      delay(300);
      
      for (int i = 0; i < 10; i++) {
        strip.setPixelColor(i, strip.Color(0, 0, 0)); // Off
      }
      strip.show();
      delay(300);
    }
    
    delay(1000);
    
    // Show value using green LEDs (number of LEDs = value/10)
    int numLeds = value / 10;
    if (numLeds > NUM_LEDS) numLeds = NUM_LEDS;
    
    for (int i = 0; i < numLeds; i++) {
      strip.setPixelColor(i, strip.Color(0, 255, 0)); // Green
    }
    strip.show();
    delay(2000);
    
    // Clear
    for (int i = 0; i < NUM_LEDS; i++) {
      strip.setPixelColor(i, strip.Color(0, 0, 0));
    }
    strip.show();
    delay(1000);
  }
}

void setup() {
  Serial.begin(115200);
  Serial.println("=== HYDROBRAIN STARTING ===");
  
  // Wait for power to stabilize - IMPORTANT FOR EXTERNAL POWER
  delay(5000);

  // *** FIXED: INITIALIZE TFT DISPLAY FIRST ***
  Serial.println("=== TFT INITIALIZATION START ===");
  
  // 1. TFT Backlight FIRST
  pinMode(TFT_LED, OUTPUT);
  digitalWrite(TFT_LED, HIGH);
  Serial.println("TFT Backlight ON");
  
  // 2. Initialize SPI BEFORE tft.begin() - THIS WAS MISSING!
  Serial.println("Initializing SPI for TFT...");
  SPI.begin(TFT_SCLK, -1, TFT_MOSI, TFT_CS);
  SPI.setFrequency(4000000); // 4MHz - good balance of speed and stability
  
  // 3. Initialize TFT display
  Serial.println("Initializing TFT display...");
  tft.begin();
  tft.setRotation(1); // Landscape mode
  tft.fillScreen(BACKGROUND_COLOR);
  
  // 4. Show startup message on TFT
  tft.setTextSize(2);
  tft.setTextColor(TEXT_DARK);
  tft.setCursor(50, 100);
  tft.println("HYDROBRAIN");
  tft.setCursor(85, 120);
  tft.println("STARTING...");
  
  Serial.println("TFT Display initialized successfully!");
  Serial.println("=== TFT INITIALIZATION COMPLETE ===");

  // Initialize pump relay pin
  pinMode(PUMP_RELAY_PIN, OUTPUT);
  digitalWrite(PUMP_RELAY_PIN, HIGH); // Start with pump OFF (assuming active LOW)
  Serial.println("Pump relay initialized");

  // Connect to WiFi with enhanced retry logic
  Serial.println("=== WiFi Connection Start ===");
  Serial.print("Connecting to: ");
  Serial.println(ssid);
  
  // Update TFT with WiFi status
  tft.fillScreen(BACKGROUND_COLOR);
  tft.setTextSize(2);
  tft.setTextColor(TEXT_DARK);
  tft.setCursor(50, 80);
  tft.println("HYDROBRAIN");
  tft.setTextSize(1);
  tft.setCursor(10, 120);
  tft.println("Connecting to WiFi...");
  
  // Try multiple connection attempts
  int connectionAttempts = 0;
  bool wifiConnected = false;
  
  while (!wifiConnected && connectionAttempts < 5) {
    connectionAttempts++;
    Serial.print("Connection attempt #");
    Serial.println(connectionAttempts);
    
    // Update TFT with attempt number
    tft.setCursor(10, 140);
    tft.print("Attempt: ");
    tft.println(connectionAttempts);
    
    WiFi.disconnect();
    delay(1000);
    WiFi.mode(WIFI_OFF);
    delay(1000);
    WiFi.mode(WIFI_STA);
    delay(1000);
    
    WiFi.begin(ssid, password);
    
    // Wait for connection with timeout
    int wifi_attempts = 0;
    while (WiFi.status() != WL_CONNECTED && wifi_attempts < 30) {
      Serial.print(".");
      delay(500);
      wifi_attempts++;
    }
    
    if (WiFi.status() == WL_CONNECTED) {
      wifiConnected = true;
      Serial.println();
      Serial.println("=== WiFi CONNECTED! ===");
      Serial.print("IP Address: ");
      Serial.println(WiFi.localIP());
      Serial.print("Signal Strength (RSSI): ");
      Serial.println(WiFi.RSSI());
      
      // Update TFT with success
      tft.fillScreen(BACKGROUND_COLOR);
      tft.setTextSize(2);
      tft.setTextColor(PRIMARY_COLOR);
      tft.setCursor(10, 60);
      tft.println("WiFi Connected!");
      
      tft.setTextSize(1);
      tft.setTextColor(TEXT_DARK);
      tft.setCursor(10, 90);
      tft.print("IP: ");
      tft.println(WiFi.localIP());
      tft.setCursor(10, 110);
      tft.print("RSSI: ");
      tft.print(WiFi.RSSI());
      tft.println(" dBm");
      
      break;
    } else {
      Serial.println();
      Serial.print("Attempt ");
      Serial.print(connectionAttempts);
      Serial.println(" failed. Retrying...");
      delay(2000);
    }
  }
  
  if (wifiConnected) {
    // Initialize time
    configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);
    Serial.println("Initializing NTP time sync...");
    
    // Setup web server routes
    server.on("/api/status", HTTP_GET, handleGetStatus);
    server.on("/api/pump/on", HTTP_POST, handlePumpOn);
    server.on("/api/pump/off", HTTP_POST, handlePumpOff);
    server.on("/api/pump/auto", HTTP_POST, handlePumpAuto);
    server.on("/api/led/growth", HTTP_POST, handleLedGrowth);
    server.on("/api/led/relax", HTTP_POST, handleLedRelax);
    server.on("/api/led/sleep", HTTP_POST, handleLedSleep);
    server.on("/api/led/off", HTTP_POST, handleLedOff);
    server.onNotFound(handleNotFound);
    
    // Handle CORS preflight requests
    server.on("/api/status", HTTP_OPTIONS, handleOptions);
    server.on("/api/pump/on", HTTP_OPTIONS, handleOptions);
    server.on("/api/pump/off", HTTP_OPTIONS, handleOptions);
    server.on("/api/pump/auto", HTTP_OPTIONS, handleOptions);
    server.on("/api/led/growth", HTTP_OPTIONS, handleOptions);
    server.on("/api/led/relax", HTTP_OPTIONS, handleOptions);
    server.on("/api/led/sleep", HTTP_OPTIONS, handleOptions);
    server.on("/api/led/off", HTTP_OPTIONS, handleOptions);
    
    server.begin();
    Serial.println("=== WEB SERVER STARTED ===");
    Serial.println("Local IP Address: " + WiFi.localIP().toString());
    Serial.println("=== API ENDPOINTS AVAILABLE ===");
    Serial.println("GET  /api/status       - Get all sensor data and status");
    Serial.println("POST /api/pump/on      - Turn pump ON manually");
    Serial.println("POST /api/pump/off     - Turn pump OFF manually");
    Serial.println("POST /api/pump/auto    - Set pump to AUTO mode");
    Serial.println("POST /api/led/growth   - Set LED to Growth mode");
    Serial.println("POST /api/led/relax    - Set LED to Relaxing mode");
    Serial.println("POST /api/led/sleep    - Set LED to Sleep mode");  
    Serial.println("POST /api/led/off      - Turn LED OFF");
    Serial.println("=========================");
  } else {
    Serial.println("=== WiFi CONNECTION FAILED AFTER ALL ATTEMPTS! ===");
    Serial.println("Continuing without WiFi...");
    
    // Update TFT with failure
    tft.fillScreen(BACKGROUND_COLOR);
    tft.setTextSize(2);
    tft.setTextColor(ALERT_COLOR);
    tft.setCursor(10, 90);
    tft.println("WiFi Failed!");
    tft.setTextSize(1);
    tft.setCursor(10, 120);
    tft.println("Check credentials/power");
  }

  // Init Sensors
  waterTempSensor.begin();
  dht.begin();
  ec.begin();

  // Init LED Strip for Plant Growth
  strip.begin();
  strip.setBrightness(BRIGHTNESS);
  
  if (wifiConnected) {
    // Show IP address using LED blink pattern
    showIPAddressOnLED();
    delay(3000);
  }
  
  // Set LED strip to optimal plant growth spectrum (Warm sunlight) - ALWAYS ON
  Serial.println("Turning on LED strip - Growth Mode");
  for (int i = 0; i < NUM_LEDS; i++) {
    strip.setPixelColor(i, strip.Color(255, 180, 80)); // Warm sunlight color
  }
  strip.show();
  ledStatus = true;
  ledMode = 1; // Growth mode
  
  Serial.println("LED Strip initialized and ON - Plant Growth Mode ACTIVE");

  // ADC setup
  analogReadResolution(12);
  analogSetAttenuation(ADC_11db);

  // Initialize pump timing
  lastPumpCycle = millis();
  
  // Show sensor initialization on TFT
  delay(3000); // Show WiFi status for 3 seconds
  tft.fillScreen(BACKGROUND_COLOR);
  tft.setTextSize(2);
  tft.setTextColor(TEXT_DARK);
  tft.setCursor(50, 80);
  tft.println("HYDROBRAIN");
  tft.setTextSize(1);
  tft.setCursor(10, 120);
  tft.println("Initializing sensors...");
  
  delay(2000); // Show initialization message
  
  // Initial display update
  updateTFTDisplay();
  Serial.println("Initial TFT Display updated");
  Serial.println("=== HYDROBRAIN STARTUP COMPLETE ===");
}

void loop() {
  // Handle web server requests
  server.handleClient();
  
  // Handle pump control
  handlePumpControl();
  
  // --- Water Temp
  waterTempSensor.requestTemperatures();
  waterTemp = waterTempSensor.getTempCByIndex(0);
  if (waterTemp == DEVICE_DISCONNECTED_C) {
    Serial.println("Failed to read water temp");
    waterTemp = 25.0;
  } else {
    Serial.print("Water Temp: ");
    Serial.print(waterTemp, 1);
    Serial.println(" °C");
  }

  // --- DHT22 (Air Temperature & Humidity)
  airTemp = dht.readTemperature();
  humidity = dht.readHumidity();
  
  if (isnan(airTemp) || isnan(humidity)) {
    Serial.println("Failed to read from DHT22 sensor!");
  } else {
    Serial.print("Air Temp: ");
    Serial.print(airTemp, 1);
    Serial.println(" °C");
    Serial.print("Humidity: ");
    Serial.print(humidity, 1);
    Serial.println(" %");
  }

  // --- EC Sensor
  int ec_raw = 0;
  for (int i = 0; i < 10; i++) {
    ec_raw += analogRead(EC_PIN);
    delay(10);
  }
  ec_raw /= 10;
  ecVoltage = ec_raw * (3.3 / 4095.0);

  // Try library method first
  if (!ecCalibrated && ecVoltage > 0.5 && ecVoltage < 2.5 && waterTemp > 5 && waterTemp < 45) {
    ec.calibration(ecVoltage, waterTemp);
    Serial.println("EC sensor calibrated");
    ecCalibrated = true;
  }

  float libraryEC = ec.readEC(ecVoltage, waterTemp);
  float manualEC = calculateECManual(ecVoltage, waterTemp);

  // Use manual calculation if library gives unrealistic reading
  if (libraryEC < 10.0 && ecVoltage > 0.1) {
    ecValue = manualEC;
  } else {
    ecValue = libraryEC;
  }

  Serial.print("EC: ");
  Serial.print(ecValue, 2);
  Serial.println(" µS/cm");

  // --- TDS Sensor
  int adc_raw = 0;
  for (int i = 0; i < 10; i++) {
    adc_raw += analogRead(TDS_PIN);
    delay(10);
  }
  adc_raw /= 10;
  float adc_voltage = adc_raw * (3.3 / 4095.0);
  float compensation_coefficient = 1.0 + 0.02 * (waterTemp - 25.0);
  float compensated_voltage = adc_voltage / compensation_coefficient;
  tds_value = (133.42 * pow(compensated_voltage, 3)) 
            - (255.86 * pow(compensated_voltage, 2)) 
            + (857.39 * compensated_voltage);
  tds_value *= 124.0 / 165.0;

  Serial.print("TDS: ");
  Serial.print(tds_value, 1);
  Serial.println(" ppm");

  // --- pH Sensor with detailed diagnostics
  int ph_raw = 0;
  for (int i = 0; i < 10; i++) {
    ph_raw += analogRead(PH_PIN);
    delay(10);
  }
  ph_raw /= 10;
  float ph_voltage = ph_raw * (3.3 / 4095.0);
  
  // Check sensor status
  if (ph_raw == 0) {
    Serial.print("ERROR: No signal - check wiring/power! | ");
    phValue = 0.0;
  } else if (ph_raw >= 4090) {
    Serial.print("ERROR: Sensor saturated (3.3V max)! | ");
    phValue = 0.0;
  } else if (ph_voltage < 0.1) {
    Serial.print("WARNING: Very low voltage! | ");
    phValue = 0.0;
  } else {
    // Calibration based on your actual readings
    // 1.810V = pH 4.0 (your measurement)
    // Assuming typical pH sensor: ~0.059V per pH unit (theoretical)
    // and neutral point around 2.5V = pH 7.0
    
    float voltage_ph4 = 1.810;  // Your measured voltage at pH 4.0
    float voltage_ph7 = 1.326;    // Estimated voltage at pH 7.0 (typical)
    
    // Calculate slope: (pH2 - pH1) / (V2 - V1)
    float slope = (7.0 - 4.0) / (voltage_ph7 - voltage_ph4);
    
    // Calculate pH: pH4 + slope * (current_voltage - voltage_at_pH4)
    phValue = 4.0 + slope * (ph_voltage - voltage_ph4);
  }

  Serial.print("pH: ");
  Serial.println(phValue, 2);

  // Print pump status
  Serial.print("Pump Status: ");
  Serial.print(pumpRunning ? "RUNNING" : "STOPPED");
  if (autoPumpEnabled && !manualPumpOverride) {
    Serial.print(" (AUTO MODE)");
    if (pumpRunning) {
      unsigned long remaining = PUMP_RUN_DURATION - (millis() - pumpStartTime);
      Serial.print(" - ");
      Serial.print(remaining / 60000);
      Serial.print("min remaining");
    } else {
      unsigned long nextCycle = PUMP_CYCLE_INTERVAL - (millis() - lastPumpCycle);
      Serial.print(" - Next cycle in ");
      Serial.print(nextCycle / 60000);
      Serial.print("min");
    }
  } else if (manualPumpOverride) {
    Serial.print(" (MANUAL MODE)");
  }
  Serial.println();

  Serial.println("------------------------");
  
  // Send data to Firebase every 2 minutes
  static unsigned long lastFirebaseUpdate = 0;
  if (millis() - lastFirebaseUpdate > 120000) { // 120000ms = 2 minutes
    sendToFirebase();
    lastFirebaseUpdate = millis();
  }
  
  // Update TFT Display every 5 minutes (after first immediate update)
  static unsigned long lastTFTUpdate = 0;
  static bool firstUpdate = true;
  
  if (firstUpdate || millis() - lastTFTUpdate > 300000) { // First update immediately, then every 5 minutes
    updateTFTDisplay();
    lastTFTUpdate = millis();
    firstUpdate = false;
    Serial.println("TFT Display updated");
  }
  
  delay(2000);
}

void updateTFTDisplay() {
  // Clear screen
  tft.fillScreen(BACKGROUND_COLOR);
  
  // Draw header
  drawHeader();
  
  // Draw sensor cards in a grid layout
  drawSensorCard(5, 45, 100, 50, "AIR TEMP", String(airTemp, 1) + "C");
  drawSensorCard(110, 45, 100, 50, "HUMIDITY", String(humidity, 1) + "%");
  drawSensorCard(215, 45, 100, 50, "TDS", String(tds_value, 0) + "ppm");
  
  drawSensorCard(5, 100, 100, 50, "EC", String(ecValue, 0) + "uS");
  drawSensorCard(110, 100, 100, 50, "pH", String(phValue, 1));
  drawSensorCard(215, 100, 100, 50, "H2O TEMP", String(waterTemp, 1) + "C");
  
  // Draw water level bar
  drawWaterLevelBar();
  
  // Draw system status
  drawSystemStatus();
  
  // Draw footer with time
  drawFooter();
}

void drawHeader() {
  // Header background
  tft.fillRect(0, 0, 320, 40, HEADER_COLOR);
  
  // Title
  tft.setTextColor(TEXT_LIGHT);
  tft.setTextSize(2);
  tft.setCursor(85, 12);
  tft.print("HYDROBRAIN");
  
  // Status indicator
  tft.fillCircle(25, 20, 6, ON_COLOR);
  tft.setTextSize(1);
  tft.setCursor(35, 16);
  tft.print("ONLINE");
}

void drawSensorCard(int x, int y, int w, int h, String title, String value) {
  // Card background
  tft.fillRoundRect(x, y, w, h, 4, CARD_BG_COLOR);
  tft.drawRoundRect(x, y, w, h, 4, PRIMARY_COLOR);
  
  // Title
  tft.setTextColor(DARK_GRAY);
  tft.setTextSize(1);
  tft.setCursor(x + 8, y + 8);
  tft.print(title);
  
  // Value
  tft.setTextColor(TEXT_DARK);
  tft.setTextSize(1);
  
  // Center the value
  int valueWidth = value.length() * 6;
  int valueX = x + (w - valueWidth) / 2;
  tft.setCursor(valueX, y + 28);
  tft.print(value);
}

void drawWaterLevelBar() {
  int barX = 10;
  int barY = 160;
  int barWidth = 300;
  int barHeight = 20;
  
  // Background
  tft.fillRoundRect(barX, barY, barWidth, barHeight, 3, CARD_BG_COLOR);
  tft.drawRoundRect(barX, barY, barWidth, barHeight, 3, PRIMARY_COLOR);
  
  // Fill based on water level
  int fillWidth = (waterLevel * (barWidth - 4)) / 100;
  uint16_t fillColor = HIGHLIGHT_COLOR; // Always use the same green color
  
  tft.fillRect(barX + 2, barY + 2, fillWidth, barHeight - 4, fillColor);
  
  // Label
  tft.setTextColor(TEXT_DARK);
  tft.setTextSize(1);
  tft.setCursor(barX + 5, barY + 6);
  tft.print("WATER LEVEL: ");
  tft.print(waterLevel);
  tft.print("%");
}

void drawSystemStatus() {
  // LED Status
  int ledX = 10;
  int ledY = 190;
  drawStatusIndicator(ledX, ledY, 70, 25, "LED", ledStatus, getLedModeText());
  
  // Pump Status
  int pumpX = 90;
  int pumpY = 190;
  String pumpText = pumpRunning ? "ACTIVE" : "IDLE";
  if (autoPumpEnabled && !manualPumpOverride) {
    pumpText += " AUTO";
  } else if (manualPumpOverride) {
    pumpText += " MAN";
  }
  drawStatusIndicator(pumpX, pumpY, 70, 25, "PUMP", pumpRunning, pumpText);
  
  // Plant info
  tft.setTextColor(TEXT_DARK);
  tft.setTextSize(1);
  tft.setCursor(170, 195);
  tft.print("GROWTH MODE");
  tft.setCursor(170, 205);
  tft.print("SPECTRUM ACTIVE");
}

void drawStatusIndicator(int x, int y, int w, int h, String label, bool status, String statusText) {
  // Background
  uint16_t bgColor = status ? ON_COLOR : OFF_COLOR;
  tft.fillRoundRect(x, y, w, h, 3, bgColor);
  tft.drawRoundRect(x, y, w, h, 3, PRIMARY_COLOR);
  
  // Label
  tft.setTextColor(TEXT_DARK);
  tft.setTextSize(1);
  tft.setCursor(x + 3, y + 3);
  tft.print(label);
  
  // Status
  tft.setCursor(x + 3, y + 13);
  tft.print(statusText);
}

void drawFooter() {
  // Footer background
  tft.fillRect(0, 220, 320, 20, DARK_GRAY);
  
  // System info
  tft.setTextColor(TEXT_LIGHT);
  tft.setTextSize(1);
  tft.setCursor(5, 226);
  tft.print("UPTIME: ");
  tft.print(millis() / 60000);
  tft.print("min");
  
  // Version
  tft.setCursor(200, 226);
  tft.print("v2.0.0");
}

String getLedModeText() {
  switch (ledMode) {
    case 1: return "GROWTH";
    case 2: return "RELAX";
    case 3: return "SLEEP";
    default: return "OFF";
  }
}

void sendToFirebase() {
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("WiFi not connected, skipping Firebase upload");
    return;
  }

  HTTPClient http;
  String url = String(firebaseHost) + "/sensor_data.json";
  
  http.begin(url);
  http.addHeader("Content-Type", "application/json");
  
  // Create JSON payload (using JsonDocument instead of deprecated DynamicJsonDocument)
  JsonDocument doc;
  doc["ec"] = ecValue;
  doc["humidity"] = humidity;
  doc["pH"] = phValue;
  doc["tds"] = tds_value;
  doc["airtemp"] = airTemp;
  doc["waterLevel"] = waterLevel;
  doc["waterTemp"] = waterTemp;
  doc["pumpStatus"] = pumpRunning;
  doc["pumpMode"] = autoPumpEnabled ? "AUTO" : "MANUAL";
  doc["ledStatus"] = ledStatus;
  
  // Add timestamp
  struct tm timeinfo;
  if (getLocalTime(&timeinfo)) {
    char timestamp[30];
    strftime(timestamp, sizeof(timestamp), "%Y-%m-%d %H:%M:%S", &timeinfo);
    doc["timestamp"] = timestamp;
  } else {
    doc["timestamp"] = "Time sync failed";
  }
  
  String jsonString;
  serializeJson(doc, jsonString);
  
  Serial.println("Sending to Firebase:");
  Serial.println(jsonString);
  
  int httpResponseCode = http.POST(jsonString);
  
  if (httpResponseCode > 0) {
    String response = http.getString();
    Serial.println("Firebase Response Code: " + String(httpResponseCode));
    Serial.println("Firebase Response: " + response);
  } else {
    Serial.println("Firebase Error: " + String(httpResponseCode));
  }
  
  http.end();
}