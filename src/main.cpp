#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_ILI9341.h>
#include <DHT.h>
#include <Adafruit_NeoPixel.h>
#include <WiFi.h>
#include <WebServer.h>

// TFT pins
#define TFT_CS 5
#define TFT_RST 4
#define TFT_DC 2
#define TFT_MOSI 23
#define TFT_CLK 18
#define TFT_LED 33

// DHT22 on digital pin 21
#define DHTPIN 21
#define DHTTYPE DHT22

// Sensor pins
#define TDS_PIN 35  // TDS sensor analog pin
#define PH_PIN 34   // pH sensor analog pin
#define TRIG_PIN 14 // Water level sensor TRIG
#define ECHO_PIN 15 // Water level sensor ECHO
#define WATER_TEMP_PIN 32 // Water Temp sensor pH 
// NeoPixel LED strip settings
#define LED_PIN 19
#define NUM_LEDS 90
#define BRIGHTNESS 150
Adafruit_NeoPixel strip(NUM_LEDS, LED_PIN, NEO_GRB + NEO_KHZ800);

// Water Pump Pin
#define WATER_PUMP_PIN 27  // GPIO pin to control water pump

// WiFi Credentials
const char* ssid = "Tuwaiq's employees";
const char* password = "Bootcamp@001";

// Web Server
WebServer server(80);

// Initialize TFT, DHT, and water pump
Adafruit_ILI9341 tft = Adafruit_ILI9341(TFT_CS, TFT_DC, TFT_RST);
DHT dht(DHTPIN, DHTTYPE);

float readWaterTemperature() {
  int sensorValue = analogRead(WATER_TEMP_PIN);  // Read analog value from temperature sensor
  float voltage = sensorValue * (3.3 / 4095.0);  // Convert ADC value to voltage

  // Calculate the temperature using the voltage
  // Assuming 0V corresponds to 0°C and 3.3V corresponds to 100°C
  float temperatureC = (voltage / 3.3) * 100.0;  // Linear scaling (adjust according to your sensor's range)

  return temperatureC;
}


// Function to read water level
long readWaterLevelPercentage() {
    digitalWrite(TRIG_PIN, LOW);
    delayMicroseconds(2);
    digitalWrite(TRIG_PIN, HIGH);           
    delayMicroseconds(10);                  
    digitalWrite(TRIG_PIN, LOW);            

    long duration = pulseIn(ECHO_PIN, HIGH, 30000); 
    if (duration == 0) {
        return 0;  // Return 0 if no signal is received instead of error
    }

    long distance = (duration / 2) * 0.0343;  // Convert to cm

    // Allow distance to be larger (adjust based on your setup)
    if (distance <= 2 || distance >= 400) {
        distance = 0;  // Set to 0 if out of range instead of error
    }

    // Define min and max distances (adjust as per your tank size)
    long minDistance = 10;   // Minimum distance (full tank, adjust accordingly)
    long maxDistance = 30;   // Maximum distance (empty tank, adjust accordingly)

    // If the distance is out of bounds, return the closest value
    if (distance < minDistance) {
        distance = minDistance;
    }
    if (distance > maxDistance) {
        distance = maxDistance;
    }

    // Calculate water level percentage
    float percentage = ((float)(distance - minDistance) / (maxDistance - minDistance)) * 100;

    // Ensure the percentage is within the range of 0 to 100
    if (percentage > 100) percentage = 100;
    if (percentage < 0) percentage = 0;

    return (long)percentage;
}


// Function to read TDS sensor value
float readTDS() {
  const int samples = 10;
  long sum = 0;
  for (int i = 0; i < samples; i++) {
    sum += analogRead(TDS_PIN);  // Read TDS sensor value
    delay(10);                   // Delay between samples
  }
  float avgADC = sum / (float)samples;  // Average ADC value
  float voltage = (avgADC / 4095.0) * 3.3;  // Convert ADC to voltage
  return voltage * 150.0;  // Multiply by a factor to get TDS in ppm
}

// Function to read pH sensor value
float readPH() {
  int sensorValue = analogRead(PH_PIN);
  float voltage = sensorValue * (3.3 / 4095.0); // Convert ADC to voltage
  float pH = 7 + ((voltage - 2.5) / 0.18);     // Adjust this based on calibration
  return pH;
}

void setup() {
  Serial.begin(115200);
  tft.begin();
  tft.setRotation(3);
  tft.fillScreen(ILI9341_WHITE);
  tft.setTextColor(ILI9341_BLACK);
  tft.setTextSize(2);
  dht.begin();

  pinMode(TFT_LED, OUTPUT);
  digitalWrite(TFT_LED, HIGH);
  pinMode(TRIG_PIN, OUTPUT);
  pinMode(ECHO_PIN, INPUT);
  pinMode(WATER_PUMP_PIN, OUTPUT);  // Initialize water pump pin
  digitalWrite(WATER_PUMP_PIN, LOW); // Initially turn off the pump

  // Initialize the NeoPixel strip
  strip.begin();
  strip.setBrightness(BRIGHTNESS);
  strip.show();  // Initialize all pixels to 'off'

  // WiFi Connection
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("WiFi connected");
  Serial.print("IP Address: ");
  Serial.println(WiFi.localIP());

  // Web Server Routes
  server.on("/", HTTP_GET, []() {
    server.send(200, "text/html", "<h1>Water Pump and LED Control</h1><a href='/pump/on'>Turn On Water Pump</a><br><a href='/pump/off'>Turn Off Water Pump</a><br><a href='/led/on'>Turn On LED</a><br><a href='/led/off'>Turn Off LED</a>");
  });

  server.on("/pump/on", HTTP_GET, []() {
    digitalWrite(WATER_PUMP_PIN, HIGH);  // Turn on the water pump
    server.send(200, "text/html", "<h1>Water Pump is ON</h1><a href='/pump/off'>Turn Off Water Pump</a><br><a href='/led/on'>Turn On LED</a><br><a href='/led/off'>Turn Off LED</a>");
  });

  server.on("/pump/off", HTTP_GET, []() {
    digitalWrite(WATER_PUMP_PIN, LOW);  // Turn off the water pump
    server.send(200, "text/html", "<h1>Water Pump is OFF</h1><a href='/pump/on'>Turn On Water Pump</a><br><a href='/led/on'>Turn On LED</a><br><a href='/led/off'>Turn Off LED</a>");
  });

  server.on("/led/on", HTTP_GET, []() {
    for (int i = 0; i < NUM_LEDS; i++) {
      strip.setPixelColor(i, strip.Color(255, 242, 0));  // Turn LED strip on with a color
    }
    strip.show();
    server.send(200, "text/html", "<h1>LED is ON</h1><a href='/pump/on'>Turn On Water Pump</a><br><a href='/pump/off'>Turn Off Water Pump</a><br><a href='/led/off'>Turn Off LED</a>");
  });

  server.on("/led/off", HTTP_GET, []() {
    for (int i = 0; i < NUM_LEDS; i++) {
      strip.setPixelColor(i, strip.Color(0, 0, 0));  // Turn LED strip off
    }
    strip.show();
    server.send(200, "text/html", "<h1>LED is OFF</h1><a href='/pump/on'>Turn On Water Pump</a><br><a href='/pump/off'>Turn Off Water Pump</a><br><a href='/led/on'>Turn On LED</a>");
  });

  server.begin();
}

void loop() {
  // Handle web server clients
  server.handleClient();

  // Optionally display sensor data on TFT screen
  float humidity = dht.readHumidity();
  float airTemp = dht.readTemperature();
  float tds = readTDS();
  long waterLevel = readWaterLevelPercentage();
  float pH = readPH();
  float waterTemp = readWaterTemperature();  // Read water temperature

  // Clear screen and display updated values
  tft.fillScreen(ILI9341_WHITE);
  tft.setTextColor(ILI9341_BLACK);
  tft.setTextSize(2);
  tft.setCursor(10, 20);
  tft.print("Air Temp: ");
  tft.print(airTemp);
  tft.print(" C");

  tft.setCursor(10, 50);
  tft.print("Humidity: ");
  tft.print(humidity);
  tft.print(" %");

  tft.setCursor(10, 80);
  tft.print("TDS: ");
  tft.print(tds);
  tft.print(" ppm");

  tft.setCursor(10, 110);
  if (waterLevel == -1) {
    tft.print("Water Level: ERROR");
  } else {
    tft.print("Water Level: ");
    tft.print(waterLevel);
    tft.print(" %");  // Display as percentage
  }

  tft.setCursor(10, 140);
  tft.print("pH: ");
  tft.print(pH);

  tft.setCursor(10, 170);
  tft.print("Water Temp: ");
  tft.print(waterTemp);
  tft.print(" C");  // Display water temperature in Celsius
  
  delay(2000);  // Delay before next reading
}
