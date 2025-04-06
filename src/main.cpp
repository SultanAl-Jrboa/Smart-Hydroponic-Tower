#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_ILI9341.h>
#include <DHT.h>
#include <Adafruit_NeoPixel.h>
#include <WiFi.h>
#include <WebServer.h>
#include <SPIFFS.h>
#include <ArduinoJson.h>

// إعلان مسبق للدالة
void updateTFTDisplay();

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
#define TDS_PIN 35
#define PH_PIN 34
#define TRIG_PIN 14
#define ECHO_PIN 15
#define WATER_TEMP_PIN 32

// NeoPixel LED strip settings
#define LED_PIN 19
#define NUM_LEDS 90
#define BRIGHTNESS 150
Adafruit_NeoPixel strip(NUM_LEDS, LED_PIN, NEO_GRB + NEO_KHZ800);

// Water Pump Pin
#define WATER_PUMP_PIN 27

// WiFi Credentials
const char* ssid = "Battal 4G_EXTnew";
const char* password = "0505169538";

// Web Server
WebServer server(80);

// Initialize TFT, DHT, and water pump
Adafruit_ILI9341 tft = Adafruit_ILI9341(TFT_CS, TFT_DC, TFT_RST);
DHT dht(DHTPIN, DHTTYPE);

// Global variables to store sensor data
float humidity = 0;
float airTemp = 0;
float tds = 0;
long waterLevel = 0;
float pH = 0;
float waterTemp = 0;
bool ledStatus = false;
bool pumpStatus = false;
unsigned long lastCheck = 0;

// HTML dashboard page stored as a string
const char index_html[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html lang="en">
<head>
  <meta charset="UTF-8">
  <meta name="viewport" content="width=device-width, initial-scale=1.0">
  <title>Smart Hydroponic System Dashboard</title>
  <script src="https://cdn.tailwindcss.com"></script>
  <script src="https://cdn.jsdelivr.net/npm/chart.js@4.4.0/dist/chart.umd.min.js"></script>
<style>
  body {
    background: url('/images/back.png') no-repeat center center fixed;
    background-size: cover;
  }
  /* Make card backgrounds slightly transparent to see the background */
  .bg-white, .bg-green-50 {
    background-color: rgba(255, 255, 255, 0.9) !important;
  }
  /* Add some text shadow to improve readability on image background */
  h1, h2, h3 {
    text-shadow: 0px 0px 3px rgba(255, 255, 255, 0.5);
  }
  /* Ensure good contrast for text on the background */
  .text-gray-600 {
    color: rgba(55, 65, 81, 0.9) !important;
  }
  /* Add a slight semi-transparent overlay to improve readability */
  .bg-gradient-to-r {
    background: linear-gradient(to right, rgba(6, 78, 59, 0.9), rgba(4, 120, 87, 0.9)) !important;
  }
</style>

</head>
<body class="min-h-screen flex flex-col items-center justify-center p-4">
  <div class="w-full max-w-6xl mx-auto bg-white bg-opacity-95 rounded-2xl shadow-2xl overflow-hidden">
    <div class="bg-gradient-to-r from-green-700 to-green-900 p-6">
      <div class="flex justify-between items-center">
        <h1 class="text-3xl font-bold text-white">Smart Hydroponic System Dashboard</h1>
        <div class="text-white text-sm">
          <span id="current-time" class="font-mono">00:00:00</span>
          <span class="ml-4">| System Status: <span id="system-status" class="font-semibold text-green-300">Operational</span></span>
        </div>
      </div>
    </div>

    <div class="grid md:grid-cols-3 gap-6 p-6">
      <div class="md:col-span-2 grid grid-cols-3 gap-4">
        <!-- Sensor Cards with Enhanced Styling and Alerts -->
        <div class="bg-green-50 p-4 rounded-lg shadow-md relative" id="tds-card">
          <div class="absolute top-2 right-2 text-xs text-gray-500">
            <span id="tds-status" class="font-semibold text-green-600">Normal</span>
          </div>
          <h3 class="text-sm text-gray-600 mb-2">TDS</h3>
          <div class="flex items-center justify-between">
            <span class="text-2xl font-bold text-green-800" id="tds">--</span>
            <span class="text-xs text-gray-500">ppm</span>
          </div>
        </div>

        <div class="bg-green-50 p-4 rounded-lg shadow-md relative" id="ph-card">
          <div class="absolute top-2 right-2 text-xs text-gray-500">
            <span id="ph-status" class="font-semibold text-green-600">Balanced</span>
          </div>
          <h3 class="text-sm text-gray-600 mb-2">pH Level</h3>
          <div class="flex items-center justify-between">
            <span class="text-2xl font-bold text-green-800" id="ph">--</span>
          </div>
        </div>

        <!-- Similar enhanced cards for other sensors -->
        <div class="bg-green-50 p-4 rounded-lg shadow-md" id="water-temp-card">
          <h3 class="text-sm text-gray-600 mb-2">Water Temp</h3>
          <div class="flex items-center justify-between">
            <span class="text-2xl font-bold text-green-800" id="water-temp">--</span>
            <span class="text-xs text-gray-500">°C</span>
          </div>
        </div>

        <div class="bg-green-50 p-4 rounded-lg shadow-md" id="humidity-card">
          <h3 class="text-sm text-gray-600 mb-2">Humidity</h3>
          <div class="flex items-center justify-between">
            <span class="text-2xl font-bold text-green-800" id="humidity">--</span>
            <span class="text-xs text-gray-500">%</span>
          </div>
        </div>

        <div class="bg-green-50 p-4 rounded-lg shadow-md" id="temp-card">
          <h3 class="text-sm text-gray-600 mb-2">Ambient Temp</h3>
          <div class="flex items-center justify-between">
            <span class="text-2xl font-bold text-green-800" id="temp">--</span>
            <span class="text-xs text-gray-500">°C</span>
          </div>
        </div>

        <div class="bg-green-50 p-4 rounded-lg shadow-md flex flex-col justify-between">
          <h3 class="text-sm text-gray-600 mb-2">Nutrient Tank Level</h3>
          <div class="w-full h-24 border-2 border-green-600 rounded-lg relative overflow-hidden">
            <div id="water-level" class="absolute bottom-0 left-0 right-0 bg-green-400 transition-all duration-500" style="height: 0%;">
              <div class="absolute bottom-0 left-0 right-0 bg-green-500 opacity-50 h-4"></div>
            </div>
            <div class="absolute inset-0 flex items-center justify-center text-green-900 font-bold" id="water-level-text">
              --%
            </div>
          </div>
        </div>
      </div>

      <div class="bg-green-50 rounded-lg p-4 flex flex-col justify-between">
        <div>
          <h3 class="text-lg font-semibold text-gray-700 mb-4">System Controls</h3>
          <div class="space-y-4">
            <button id="led-btn" class="w-full py-3 bg-green-600 hover:bg-green-700 text-white font-semibold rounded-lg transition duration-300 ease-in-out transform hover:scale-105 flex items-center justify-center">
              <svg xmlns="http://www.w3.org/2000/svg" class="h-5 w-5 mr-2" viewBox="0 0 20 20" fill="currentColor">
                <path d="M11 3a1 1 0 10-2 0v1a1 1 0 102 0V3zM15.657 5.757a1 1 0 00-1.414-1.414l-.707.707a1 1 0 001.414 1.414l.707-.707zM18 10a1 1 0 01-1 1h-1a1 1 0 110-2h1a1 1 0 011 1zM5.05 6.464A1 1 0 106.464 5.05l-.707-.707a1 1 0 00-1.414 1.414l.707.707zM5 10a1 1 0 01-1 1H3a1 1 0 110-2h1a1 1 0 011 1zm3 5a1 1 0 100-2H7a1 1 0 100 2h1zm5 3a1 1 0 100-2h-1a1 1 0 100 2h1z"/>
              </svg>
              Toggle LED
            </button>
            <button id="pump-btn" class="w-full py-3 bg-emerald-600 hover:bg-emerald-700 text-white font-semibold rounded-lg transition duration-300 ease-in-out transform hover:scale-105 flex items-center justify-center">
              <svg xmlns="http://www.w3.org/2000/svg" class="h-5 w-5 mr-2" viewBox="0 0 20 20" fill="currentColor">
                <path fill-rule="evenodd" d="M11.3 1.046A1 1 0 0112 2v5h4a1 1 0 01.82 1.573l-7 10A1 1 0 018 18v-5H4a1 1 0 01-.82-1.573l7-10a1 1 0 011.12-.38z" clip-rule="evenodd"/>
              </svg>
              Toggle Pump
            </button>
          </div>
        </div>
        <div class="mt-4 text-sm text-gray-600">
          <p>Last System Check: <span id="last-check">--</span></p>
          <p class="mt-1">Next Scheduled Check: <span id="next-check">--</span></p>
        </div>
      </div>
    </div>

    <div class="p-6 bg-white border-t bg-opacity-90">
      <h2 class="text-xl font-semibold text-gray-700 mb-4">Sensor Trends</h2>
      <canvas id="sensorChart"></canvas>
    </div>
  </div>

  <!-- Copyright Notice -->
  <div class="w-full max-w-6xl mx-auto mt-4 mb-2 text-center">
    <p class="font-semibold text-lg bg-green-700 bg-opacity-70 py-2 px-4 rounded-lg inline-block text-white">
      Made by 
      <a href="https://www.linkedin.com/in/sultanal-jrboa/" class="font-bold text-white">Sultan Al-Jarboa</a>
    </p>
  </div>
  
  <script>
    // Real-time clock with date
    function updateClock() {
      const now = new Date();
      const timeString = now.toLocaleTimeString([], { hour: '2-digit', minute: '2-digit', second: '2-digit' });
      document.getElementById('current-time').textContent = `${timeString}`;
    }
    setInterval(updateClock, 1000);
    updateClock();

    // Sensor Validation and Status Checking
    function validateSensorReadings() {
      const sensors = {
        tds: { value: parseFloat(document.getElementById('tds').textContent), 
               optimal: { min: 400, max: 600 } },
        ph: { value: parseFloat(document.getElementById('ph').textContent), 
              optimal: { min: 5.5, max: 6.5 } },
        waterTemp: { value: parseFloat(document.getElementById('water-temp').textContent), 
                     optimal: { min: 20, max: 25 } }
      };

      Object.entries(sensors).forEach(([sensorName, sensor]) => {
        if (isNaN(sensor.value)) return;
        
        const statusEl = document.getElementById(`${sensorName}-status`);
        const cardEl = document.getElementById(`${sensorName}-card`);
        
        if (sensor.value < sensor.optimal.min || sensor.value > sensor.optimal.max) {
          statusEl.textContent = 'Alert';
          statusEl.classList.replace('text-green-600', 'text-red-600');
          cardEl.classList.add('border-2', 'border-red-300');
        } else {
          statusEl.textContent = 'Normal';
          statusEl.classList.replace('text-red-600', 'text-green-600');
          cardEl.classList.remove('border-2', 'border-red-300');
        }
      });
    }

    // Chart Configuration
    const ctx = document.getElementById('sensorChart').getContext('2d');
    const sensorChart = new Chart(ctx, {
      type: 'line',
      data: {
        labels: ['--', '--', '--', '--', '--'],
        datasets: [
          { 
            label: 'TDS', 
            data: [0, 0, 0, 0, 0], 
            borderColor: 'rgba(22, 163, 74, 0.7)', 
            backgroundColor: 'rgba(22, 163, 74, 0.1)',
            tension: 0.4
          },
          { 
            label: 'pH', 
            data: [0, 0, 0, 0, 0], 
            borderColor: 'rgba(5, 150, 105, 0.7)', 
            backgroundColor: 'rgba(5, 150, 105, 0.1)',
            tension: 0.4
          },
          { 
            label: 'Water Temp', 
            data: [0, 0, 0, 0, 0], 
            borderColor: 'rgba(16, 185, 129, 0.7)', 
            backgroundColor: 'rgba(16, 185, 129, 0.1)',
            tension: 0.4
          },
          { 
            label: 'Humidity', 
            data: [0, 0, 0, 0, 0], 
            borderColor: 'rgba(52, 211, 153, 0.7)', 
            backgroundColor: 'rgba(52, 211, 153, 0.1)',
            tension: 0.4
          }
        ]
      },
      options: {
        responsive: true,
        interaction: {
          mode: 'index',
          intersect: false
        },
        scales: {
          x: { 
            title: { 
              display: true, 
              text: 'Time' 
            }
          },
          y: { 
            beginAtZero: false,
            title: { 
              display: true, 
              text: 'Sensor Values' 
            }
          }
        },
        plugins: {
          legend: {
            position: 'bottom'
          }
        }
      }
    });

    // Function to fetch sensor data from ESP32
    async function fetchSensorData() {
      try {
        const response = await fetch('/data');
        const data = await response.json();
        
        // Update sensor displays
        document.getElementById('tds').textContent = data.tds.toFixed(1);
        document.getElementById('ph').textContent = data.ph.toFixed(1);
        document.getElementById('water-temp').textContent = data.waterTemp.toFixed(1);
        document.getElementById('humidity').textContent = data.humidity.toFixed(1);
        document.getElementById('temp').textContent = data.airTemp.toFixed(1);
        
        // Update water level display
        document.getElementById('water-level').style.height = `${data.waterLevel}%`;
        document.getElementById('water-level-text').textContent = `${data.waterLevel}%`;
        
        // Update last check time
        document.getElementById('last-check').textContent = data.lastCheck;
        document.getElementById('next-check').textContent = '30 seconds';
        
        // Update chart with new data points
        updateChartData(data.tds, data.ph, data.waterTemp, data.humidity);
        
        validateSensorReadings();
      } catch (error) {
        console.error('Error fetching sensor data:', error);
        document.getElementById('system-status').textContent = 'Connection Error';
        document.getElementById('system-status').classList.replace('text-green-300', 'text-red-300');
      }
    }

    // Function to update chart data
    function updateChartData(tdsValue, phValue, waterTempValue, humidityValue) {
      const chart = sensorChart;
      const currentTime = new Date().toLocaleTimeString([], {hour: '2-digit', minute: '2-digit'});
      
      // Shift labels
      chart.data.labels.shift();
      chart.data.labels.push(currentTime);

      // Update each dataset
      chart.data.datasets[0].data.shift();
      chart.data.datasets[0].data.push(tdsValue);

      chart.data.datasets[1].data.shift();
      chart.data.datasets[1].data.push(parseFloat(phValue));

      chart.data.datasets[2].data.shift();
      chart.data.datasets[2].data.push(parseFloat(waterTempValue));

      chart.data.datasets[3].data.shift();
      chart.data.datasets[3].data.push(humidityValue);

      chart.update();
    }

    // Event Listeners for System Controls
    document.getElementById('led-btn').addEventListener('click', async () => {
      try {
        await fetch('/toggle-led');
        fetchSensorData(); // Refresh data to get updated LED status
      } catch (error) {
        console.error('Error toggling LED:', error);
      }
    });

    document.getElementById('pump-btn').addEventListener('click', async () => {
      try {
        await fetch('/toggle-pump');
        fetchSensorData(); // Refresh data to get updated pump status
      } catch (error) {
        console.error('Error toggling pump:', error);
      }
    });

    // Initial data fetch and setup interval for updates
    fetchSensorData();
    setInterval(fetchSensorData, 30000); // Update every 30 seconds
  </script>
</body>
</html>
)rawliteral";

float readWaterTemperature() {
  int sensorValue = analogRead(WATER_TEMP_PIN);
  float voltage = sensorValue * (3.3 / 4095.0);
  return (voltage / 3.3) * 100.0;
}

long readWaterLevelPercentage() {
  digitalWrite(TRIG_PIN, LOW);
  delayMicroseconds(2);
  digitalWrite(TRIG_PIN, HIGH);           
  delayMicroseconds(10);                  
  digitalWrite(TRIG_PIN, LOW);            

  long duration = pulseIn(ECHO_PIN, HIGH, 30000);
  if (duration == 0) return 0;
  long distance = (duration / 2) * 0.0343;
  if (distance <= 2 || distance >= 400) distance = 0;

  long minDistance = 10;
  long maxDistance = 30;
  if (distance < minDistance) distance = minDistance;
  if (distance > maxDistance) distance = maxDistance;

  float percentage = 100 - (((float)(distance - minDistance) / (maxDistance - minDistance)) * 100);
  if (percentage > 100) percentage = 100;
  if (percentage < 0) percentage = 0;
  return (long)percentage;
}

float readTDS() {
  const int samples = 10;
  long sum = 0;
  for (int i = 0; i < samples; i++) {
    sum += analogRead(TDS_PIN);
    delay(10);
  }
  float avgADC = sum / (float)samples;
  float voltage = (avgADC / 4095.0) * 3.3;
  return voltage * 150.0;
}

float readPH() {
  int sensorValue = analogRead(PH_PIN);
  float voltage = sensorValue * (3.3 / 4095.0);
  return 7 + ((voltage - 2.5) / 0.18);
}

void updateSensorData() {
  humidity = dht.readHumidity();
  airTemp = dht.readTemperature();
  tds = readTDS();
  waterLevel = readWaterLevelPercentage();
  pH = readPH();
  waterTemp = readWaterTemperature();
  lastCheck = millis();
  updateTFTDisplay();
}

void updateTFTDisplay() {
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
  tft.print("Water Level: ");
  tft.print(waterLevel);
  tft.print(" %");

  tft.setCursor(10, 140);
  tft.print("pH: ");
  tft.print(pH);

  tft.setCursor(10, 170);
  tft.print("Water Temp: ");
  tft.print(waterTemp);
  tft.print(" C");
  
  tft.setCursor(10, 200);
  tft.print("LED: ");
  tft.print(ledStatus ? "ON" : "OFF");
  
  tft.setCursor(10, 230);
  tft.print("Pump: ");
  tft.print(pumpStatus ? "ON" : "OFF");
}

void handleRoot() {
  server.send(200, "text/html", index_html);
}

void handleData() {
  DynamicJsonDocument doc(1024);
  doc["humidity"] = humidity;
  doc["airTemp"] = airTemp;
  doc["tds"] = tds;
  doc["waterLevel"] = waterLevel;
  doc["ph"] = pH;
  doc["waterTemp"] = waterTemp;
  doc["ledStatus"] = ledStatus;
  doc["pumpStatus"] = pumpStatus;
  
  unsigned long timeSinceCheck = (millis() - lastCheck) / 1000;
  if (timeSinceCheck < 60)
    doc["lastCheck"] = String(timeSinceCheck) + " seconds ago";
  else
    doc["lastCheck"] = String(timeSinceCheck / 60) + " mins ago";
  
  String response;
  serializeJson(doc, response);
  server.send(200, "application/json", response);
}

void handleToggleLED() {
  ledStatus = !ledStatus;
  if (ledStatus) {
    for (int i = 0; i < NUM_LEDS; i++) {
      strip.setPixelColor(i, strip.Color(255, 242, 0));
    }
  } else {
    for (int i = 0; i < NUM_LEDS; i++) {
      strip.setPixelColor(i, strip.Color(0, 0, 0));
    }
  }
  strip.show();
  server.send(200, "text/plain", "LED toggled");
  updateTFTDisplay();
}

void handleTogglePump() {
  pumpStatus = !pumpStatus;
  digitalWrite(WATER_PUMP_PIN, pumpStatus ? HIGH : LOW);
  server.send(200, "text/plain", "Pump toggled");
  updateTFTDisplay();
}

void setup() {
  Serial.begin(115200);
  
  if (!SPIFFS.begin(true)) {
    Serial.println("An error occurred while mounting SPIFFS");
    return;
  }
  
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
  pinMode(WATER_PUMP_PIN, OUTPUT);
  digitalWrite(WATER_PUMP_PIN, LOW);
  
  strip.begin();
  strip.setBrightness(BRIGHTNESS);
  strip.show();
  
  tft.setCursor(10, 10);
  tft.println("Connecting to WiFi...");
  
  WiFi.begin(ssid, password);
  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED && attempts < 20) {
    delay(500);
    Serial.print(".");
    attempts++;
  }
  
  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("\nWiFi connected");
    Serial.print("IP Address: ");
    Serial.println(WiFi.localIP());
    
    tft.setCursor(10, 40);
    tft.print("IP: ");
    tft.println(WiFi.localIP().toString());
  } else {
    Serial.println("WiFi connection failed");
    tft.setCursor(10, 40);
    tft.println("WiFi connection failed");
  }
  
  server.on("/", HTTP_GET, handleRoot);
  server.on("/data", HTTP_GET, handleData);
  server.on("/toggle-led", HTTP_GET, handleToggleLED);
  server.on("/toggle-pump", HTTP_GET, handleTogglePump);
  // Add route for serving static files
  server.serveStatic("/images/", SPIFFS, "/images/");
  
  server.begin();
  Serial.println("HTTP server started");
  
  updateSensorData();
  delay(2000);
}

void loop() {
  server.handleClient();
  
  static unsigned long lastSensorUpdate = 0;
  if (millis() - lastSensorUpdate > 30000) {
    updateSensorData();
    lastSensorUpdate = millis();
  }
}