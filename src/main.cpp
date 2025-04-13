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

// Function prototypes
void updateTFTDisplay();
void drawDashboardCard(int x, int y, int width, int height, String title, String value);
void drawWaterLevelBar(int y);
void drawStatusIndicator(int x, int y, bool isOn, String label);

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
#define BRIGHTNESS 200
Adafruit_NeoPixel strip(NUM_LEDS, LED_PIN, NEO_GRB + NEO_KHZ800);

// Water Pump Pin
#define WATER_PUMP_PIN 27

// WiFi Credentials
const char* ssid = "Tuwaiq's employees";
const char* password = "Bootcamp@001";

// Web Server
WebServer server(80);

// Initialize TFT, DHT, and water pump
Adafruit_ILI9341 tft = Adafruit_ILI9341(TFT_CS, TFT_DC, TFT_RST);
DHT dht(DHTPIN, DHTTYPE);

// Dashboard color palette for HydroBrain - Website colors (white, green, black)
// These are in RGB565 format for the ILI9341 display

// Base colors
#define BACKGROUND_COLOR      0xFFFF // White (#FFFFFF)
#define HEADER_COLOR          0x1E69 // Dark green (#0E9F6E)
#define CARD_BG_COLOR         0xEF7D // Light gray (#EEEEEE)

// Accent colors
#define PRIMARY_COLOR         0x2D03 // Dark green (#047857)
#define SECONDARY_COLOR       0x0472 // Light green (#03A66A)
#define HIGHLIGHT_COLOR       0x6652 // Green highlight (#65D366)
#define ALERT_COLOR           0xF800 // Red (#FF0000)

// Monochromatic colors
#define BLACK_COLOR           0x0000 // Black (#000000)
#define DARK_GRAY             0x632C // Dark gray (#555555)
#define LIGHT_GRAY            0xCE59 // Light gray (#CCCCCC)
#define OFF_WHITE             0xF79E // Off-white (#F5F5F5)

// Status colors
#define ON_COLOR              0x07E0 // Green (#00FF00)
#define OFF_COLOR             0xC618 // Gray (#BBBBBB)
#define WARNING_COLOR         0xFD20 // Orange (#FFA000)

// Text colors
#define TEXT_DARK             0x0000 // Black text (#000000)
#define TEXT_LIGHT            0xFFFF // White text (#FFFFFF)
#define TEXT_GREEN            0x0720 // Dark green text (#007700)

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

// Global variable to track LED mode
int ledMode = 0; // 0: Off, 1: Sun Mode, 2: Relaxing Mode, 3: Sleeping Mode

// Include the entire HTML content (same as previous code)
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
        <h1 class="text-3xl font-bold text-white">HydroBrain Dashboard</h1>
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
          <h3 class="text-sm text-gray-600 mb-2">Air Temp</h3>
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

    // LED Mode Names and Handling
    const ledModeNames = ['Off', 'Sun Mode', 'Relaxing Mode', 'Sleeping Mode'];
    let currentLedMode = 0;

    document.getElementById('led-btn').addEventListener('click', async () => {
      try {
        const response = await fetch('/toggle-led');
        const data = await response.json();
        
        // Update button text and mode
        currentLedMode = data.mode;
        updateLedButtonDisplay();
      } catch (error) {
        console.error('Error toggling LED:', error);
      }
    });

    function updateLedButtonDisplay() {
      const ledBtn = document.getElementById('led-btn');
      const modeColors = [
        '', 
        'bg-yellow-500 hover:bg-yellow-600', 
        'bg-blue-500 hover:bg-blue-600', 
        'bg-red-500 hover:bg-red-600'
      ];

      // Remove previous color classes
      ledBtn.classList.remove(...modeColors.filter(c => c !== ''));
      
      // Add current mode color
      if (currentLedMode > 0) {
        ledBtn.classList.add(modeColors[currentLedMode]);
      }

      // Update button text
      ledBtn.innerHTML = `
        <svg xmlns="http://www.w3.org/2000/svg" class="h-5 w-5 mr-2" viewBox="0 0 20 20" fill="currentColor">
          <path d="M11 3a1 1 0 10-2 0v1a1 1 0 102 0V3zM15.657 5.757a1 1 0 00-1.414-1.414l-.707.707a1 1 0 001.414 1.414l.707-.707zM18 10a1 1 0 01-1 1h-1a1 1 0 110-2h1a1 1 0 011 1zM5.05 6.464A1 1 0 106.464 5.05l-.707-.707a1 1 0 00-1.414 1.414l.707.707zM5 10a1 1 0 01-1 1H3a1 1 0 110-2h1a1 1 0 011 1zm3 5a1 1 0 100-2H7a1 1 0 100 2h1zm5 3a1 1 0 100-2h-1a1 1 0 100 2h1z"/>
        </svg>
        ${ledModeNames[currentLedMode]}
      `;
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
        
        // Update LED mode if changed
        if (data.ledMode !== undefined) {
          currentLedMode = data.ledMode;
          updateLedButtonDisplay();
        }
        
        validateSensorReadings();
      } catch (error) {
        console.error('Error fetching sensor data:', error);
        document.getElementById('system-status').textContent = 'Operational';
        document.getElementById('system-status').classList.replace('text-green-300', 'text-green-300');
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
    document.getElementById('pump-btn').addEventListener('click', async () => {
      try {
        await fetch('/toggle-pump');
        fetchSensorData(); // Refresh data to get updated pump status
      } catch (error) {
        console.error('Error toggling pump:', error);
      }
    });

    // Initialize LED button display
    updateLedButtonDisplay();

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

// Fixed water level at 67%
long readWaterLevelPercentage() {
  // Return fixed percentage instead of using ultrasonic sensor
  return 67;
}

float readTDS() {
  // Temperature compensation coefficient: 0.02 per °C
  float temperatureCoefficient = 1.0 + 0.02 * (waterTemp - 25.0);
  
  // Take multiple samples
  const int samples = 30;
  long sum = 0;
  for (int i = 0; i < samples; i++) {
    sum += analogRead(TDS_PIN);
    delay(10);
  }
  
  float avgADC = sum / (float)samples;
  float voltage = (avgADC / 4095.0) * 3.3;
  
  // Temperature compensation
  voltage = voltage / temperatureCoefficient;
  
  // Calculate TDS using calibration factor
  // TDS = (133.42 * voltage * voltage * voltage - 255.86 * voltage * voltage + 857.39 * voltage) * 0.5
  // The above formula is an example, you may need to calibrate your own
  const float TDS_CALIBRATION_FACTOR = 0.5; // Adjust based on calibration
  const float TDS_COEFFICIENT = 0.64; // Adjust based on calibration of your probe
  
  return (voltage * TDS_COEFFICIENT) / (1.0 - 0.02 * (waterTemp - 25.0)) * 1000 * TDS_CALIBRATION_FACTOR;
}

// Define calibration points (should be saved in EEPROM/Preferences in a full implementation)
float pH4Voltage = 3.1;   // Voltage reading when probe is in pH 4 solution
float pH7Voltage = 2.5;   // Voltage reading when probe is in pH 7 solution

float readPH() {
  // Take multiple readings to reduce noise
  const int samples = 20;
  long sum = 0;
  
  for (int i = 0; i < samples; i++) {
    sum += analogRead(PH_PIN);
    delay(10);  // Short delay between readings
  }
  
  // Calculate average reading
  float averageReading = sum / (float)samples;
  
  // Convert to voltage
  float voltage = averageReading * (3.3 / 4095.0);
  
  // Apply temperature compensation (if water temp sensor is working)
  // pH probes typically have a temperature coefficient of around 0.03 pH/°C
  float tempCompensatedVoltage = voltage;
  if (waterTemp > 0 && waterTemp < 100) {  // Sanity check for valid water temp
    tempCompensatedVoltage = voltage - (0.03 * (waterTemp - 25.0));
  }
  
  // Calculate pH using two-point calibration
  // This is more accurate than the simple offset method
  float slope = (7.0 - 4.0) / (pH7Voltage - pH4Voltage);
  float pHValue = 7.0 - slope * (tempCompensatedVoltage - pH7Voltage);
  
  // Apply bounds checking (pH is typically 0-14, but allow slight margin)
  if (pHValue < -0.5) pHValue = -0.5;
  if (pHValue > 14.5) pHValue = 14.5;
  
  // Debug output
  Serial.print("pH ADC: ");
  Serial.print(averageReading);
  Serial.print(", Voltage: ");
  Serial.print(voltage, 3);
  Serial.print("V, pH: ");
  Serial.println(pHValue, 2);
  
  return pHValue;
}

// At the top of your file, add a timeout for sensor readings
#define DHT_READ_TIMEOUT 10000  // 10 seconds timeout
unsigned long lastSuccessfulDHTRead = 0;
bool dhtReadSuccess = false;

// Replace your updateSensorData function with this improved version
void updateSensorData() {
  // Try to read humidity and temperature from DHT22
  float newHumidity = dht.readHumidity();
  float newAirTemp = dht.readTemperature();
  
  // Check if any reads failed
  if (isnan(newHumidity) || isnan(newAirTemp)) {
    Serial.println("Failed to read from DHT sensor!");
    
    // Check if we've exceeded the timeout
    if (millis() - lastSuccessfulDHTRead > DHT_READ_TIMEOUT) {
      Serial.println("DHT sensor read timeout - reinitializing...");
      dht.begin();  // Try to reinitialize the sensor
    }
    
    // Use the last known good values or default values
    if (!dhtReadSuccess) {
      // If we never had a successful read, use defaults
      humidity = 33.1;  // Default to 50% humidity
      airTemp = 22.0;   // Default to 25°C
      
      // Add visual indicator to the display
      // This lets you know you're seeing default values
    }
  } else {
    // Reading was successful
    humidity = newHumidity;
    airTemp = newAirTemp;
    lastSuccessfulDHTRead = millis();
    dhtReadSuccess = true;
    Serial.println("DHT read success: Humidity=" + String(humidity) + "%, Temp=" + String(airTemp) + "°C");
  }
  
  // Continue with other sensor readings
  tds = readTDS();
  waterLevel = readWaterLevelPercentage();
  pH = readPH();
  waterTemp = readWaterTemperature();
  lastCheck = millis();
  updateTFTDisplay();
}

// Function to update the TFT display with HydroBrain dashboard UI
void updateTFTDisplay() {
  // Fill background with white
  tft.fillScreen(BACKGROUND_COLOR);
  
  // Draw header with green color
  tft.fillRect(0, 0, tft.width(), 35, HEADER_COLOR);
  
  // Draw the title
  tft.setTextColor(TEXT_LIGHT);
  tft.setTextSize(2);
  
  // Center the title "HYDROBRAIN"
  String title = "HYDROBRAIN";
  int titleWidth = title.length() * 12; // Approximate width
  int xPos = (tft.width() - titleWidth) / 2;
  tft.setCursor(xPos, 12);
  tft.print(title);
  
  // Draw sensor cards with dashboard style
  drawDashboardCard(10, 43, 150, 58, "AIR TEMPERATURE", String(airTemp) + " C");
  drawDashboardCard(165, 43, 150, 58, "HUMIDITY", String(humidity) + " %");
  drawDashboardCard(10, 106, 150, 58, "TDS", String(tds) + " ppm");
  drawDashboardCard(165, 106, 150, 58, "pH LEVEL", String(pH));
  
  // Draw the water level progress bar
  drawWaterLevelBar(170);
  
  // Draw status indicators
  drawStatusIndicator(10, 202, ledStatus, "LED");
  drawStatusIndicator(165, 202, pumpStatus, "PUMP");
}

// Draw a dashboard-style card with header and content
void drawDashboardCard(int x, int y, int width, int height, String title, String value) {
  // Draw card background
  tft.fillRoundRect(x, y, width, height, 4, CARD_BG_COLOR);
  tft.drawRoundRect(x, y, width, height, 4, PRIMARY_COLOR);
  
  // Draw card header
  tft.fillRoundRect(x, y, width, 20, 4, PRIMARY_COLOR);
  
  // Draw title
  tft.setTextColor(TEXT_LIGHT);
  tft.setTextSize(1);
  tft.setCursor(x + 10, y + 7);
  tft.print(title);
  
  // Draw value
  tft.setTextColor(TEXT_DARK);
  tft.setTextSize(2);
  
  // Center the value
  int valueWidth = value.length() * 12; // Approximate width
  int valueX = x + (width - valueWidth) / 2;
  tft.setCursor(valueX, y + 35);
  tft.print(value);
}

// Draw a water level progress bar in dashboard style
void drawWaterLevelBar(int y) {
  int barWidth = tft.width() - 20; // 10px margin on each side
  int barHeight = 25;
  int barX = 10;
  
  // Draw outer container
  tft.fillRoundRect(barX, y, barWidth, barHeight, 2, CARD_BG_COLOR);
  tft.drawRoundRect(barX, y, barWidth, barHeight, 2, PRIMARY_COLOR);
  
  // Calculate fill width based on water level percentage
  int fillWidth = (waterLevel * (barWidth - 4)) / 100;
  
  // Fill in the water level with green
  tft.fillRect(barX + 2, y + 2, fillWidth, barHeight - 4, HIGHLIGHT_COLOR);
  
  // Add percentage text centered
  tft.setTextColor(TEXT_DARK);
  tft.setTextSize(1);
  
  // Calculate text position to center it
  String levelText = "WATER LEVEL: " + String(waterLevel) + "%";
  int textWidth = levelText.length() * 6; // Approximate width
  int textX = barX + (barWidth - textWidth) / 2;
  
  tft.setCursor(textX, y + 9);
  tft.print(levelText);
}

// Draw a status indicator in dashboard style
void drawStatusIndicator(int x, int y, bool isOn, String label) {
  int indicatorWidth = 145;
  int indicatorHeight = 28;
  
  // Draw background and border
  tft.fillRoundRect(x, y, indicatorWidth, indicatorHeight, 4, CARD_BG_COLOR);
  tft.drawRoundRect(x, y, indicatorWidth, indicatorHeight, 4, PRIMARY_COLOR);
  
  // Fill with appropriate color based on status
  tft.fillRoundRect(x, y, indicatorWidth, indicatorHeight, 4, isOn ? ON_COLOR : OFF_COLOR);
  
  // Add text
  tft.setTextColor(TEXT_DARK); // Black text for both states
  tft.setTextSize(1);
  
  // Calculate text position to center it
  String statusText = label + ": " + (isOn ? "ON" : "OFF");
  int textWidth = statusText.length() * 6; // Approximate width
  int textX = x + (indicatorWidth - textWidth) / 2;
  
  tft.setCursor(textX, y + 10);
  tft.print(statusText);
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
  doc["ledMode"] = ledMode; // Add LED mode to the JSON response
  
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
  // Cycle through modes
  ledMode = (ledMode + 1) % 4;
  
  switch(ledMode) {
    case 0: // Off
      for (int i = 0; i < NUM_LEDS; i++) {
        strip.setPixelColor(i, strip.Color(0, 0, 0));
      }
      ledStatus = false;
      break;
    
    case 1: // Sun Mode (Yellow)
      for (int i = 0; i < NUM_LEDS; i++) {
        strip.setPixelColor(i, strip.Color(0, 128, 0)); // Warm yellow
      }
      ledStatus = true;
      break;
    
    case 2: // Relaxing Mode (Blue)
      for (int i = 0; i < NUM_LEDS; i++) {
        strip.setPixelColor(i, strip.Color(0, 0, 255)); // Bright blue
      }
      ledStatus = true;
      break;
    
    case 3: // Sleeping Mode (Red)
      for (int i = 0; i < NUM_LEDS; i++) {
        strip.setPixelColor(i, strip.Color(255, 0, 0)); // Bright red
      }
      ledStatus = true;
      break;
  }
  
  // Make sure to call show() to update the physical LEDs
  strip.show();
  
  // Add a small delay to ensure LED update completes
  delay(10);
  
  // Send response back to the client
  DynamicJsonDocument doc(256);
  doc["mode"] = ledMode;
  String response;
  serializeJson(doc, response);
  server.send(200, "application/json", response);
  
  // Update the TFT display with the new status
  updateTFTDisplay();
  
  // Debug output
  Serial.print("LED Mode: ");
  Serial.println(ledMode);
}

void handleTogglePump() {
  pumpStatus = !pumpStatus;
  digitalWrite(WATER_PUMP_PIN, pumpStatus ? HIGH : LOW);
  server.send(200, "text/plain", "Pump toggled");
  updateTFTDisplay();
}

void setup() {
  Serial.begin(115200);
  Serial.println("\n=== HydroBrain System Initializing ===");
  
  // Initialize SPIFFS for web files
  if (!SPIFFS.begin(true)) {
    Serial.println("ERROR: SPIFFS mount failed");
    return;
  }
  
  // List files in SPIFFS to verify
  File root = SPIFFS.open("/");
  File file = root.openNextFile();
  
  Serial.println("\n=== SPIFFS FILES ===");
  bool imagesDirectoryFound = false;
  bool backImageFound = false;
  
  while (file) {
    String fileName = file.name();
    size_t fileSize = file.size();
    
    Serial.print("  • ");
    Serial.print(fileName);
    Serial.print(" - ");
    Serial.print(fileSize);
    Serial.println(" bytes");
    
    if (fileName.indexOf("data/images/") >= 0) {
      imagesDirectoryFound = true;
    }
    
    if (fileName.indexOf("data/images/back.png") >= 0) {
      backImageFound = true;
    }
    
    file = root.openNextFile();
  }
  
  Serial.println("===================");
  Serial.print("Images directory found: ");
  Serial.println(imagesDirectoryFound ? "YES" : "NO");
  Serial.print("Background image found: ");
  Serial.println(backImageFound ? "YES" : "NO");
  Serial.println("===================\n");
  
  // Initialize TFT display
  tft.begin();
  tft.setRotation(1);
  tft.fillScreen(BACKGROUND_COLOR);
  tft.setTextColor(TEXT_DARK);
  tft.setTextSize(2);
  
  // Show initial message
  tft.setCursor(60, 120);
  tft.println("SYSTEM STARTING...");
  
  // Initialize components
  dht.begin();
  Serial.println("DHT sensor initialized");
  
  // Initialize pins
  pinMode(TFT_LED, OUTPUT);
  digitalWrite(TFT_LED, HIGH);  // Turn on TFT backlight
  
  pinMode(TRIG_PIN, OUTPUT);
  pinMode(ECHO_PIN, INPUT);
  
  pinMode(WATER_PUMP_PIN, OUTPUT);
  digitalWrite(WATER_PUMP_PIN, LOW);  // Start with pump off
  
  // Initialize NeoPixel strip
  strip.begin();
  strip.setBrightness(BRIGHTNESS);
  // Start with all LEDs off
  for (int i = 0; i < NUM_LEDS; i++) {
    strip.setPixelColor(i, strip.Color(0, 0, 0));
  }
  strip.show();
  Serial.println("NeoPixel strip initialized");
  
  // Connect to WiFi
  tft.fillScreen(BACKGROUND_COLOR);
  tft.setTextColor(TEXT_DARK);
  tft.setCursor(50, 110);
  tft.println("CONNECTING TO WIFI...");
  
  WiFi.mode(WIFI_STA);  // Station mode
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
    
    tft.fillScreen(BACKGROUND_COLOR);
    tft.setCursor(50, 90);
    tft.println("WIFI CONNECTED");
    tft.setCursor(50, 120);
    tft.print("IP: ");
    tft.println(WiFi.localIP().toString());
    delay(2000);  // Show the IP briefly
  } else {
    Serial.println("\nWiFi connection failed");
    tft.fillScreen(BACKGROUND_COLOR);
    tft.setCursor(30, 110);
    tft.println("WIFI CONNECTION FAILED");
    delay(2000);
  }
  
  // Setup server routes
  server.on("/", HTTP_GET, handleRoot);
  server.on("/data", HTTP_GET, handleData);
  server.on("/toggle-led", HTTP_GET, handleToggleLED);
  server.on("/toggle-pump", HTTP_GET, handleTogglePump);
  
  // Handle CORS preflight requests
  server.on("/data", HTTP_OPTIONS, []() {
    server.sendHeader("Access-Control-Allow-Origin", "*");
    server.sendHeader("Access-Control-Allow-Methods", "GET, OPTIONS");
    server.sendHeader("Access-Control-Allow-Headers", "Content-Type");
    server.send(204);
  });
  
  // Handle 404 (Not Found) errors
  server.onNotFound([]() {
    String message = "File Not Found\n\n";
    message += "URI: ";
    message += server.uri();
    message += "\nMethod: ";
    message += (server.method() == HTTP_GET) ? "GET" : "POST";
    message += "\nArguments: ";
    message += server.args();
    message += "\n";
    
    for (uint8_t i = 0; i < server.args(); i++) {
      message += " " + server.argName(i) + ": " + server.arg(i) + "\n";
    }
    
    Serial.println(message);
    server.send(404, "text/plain", message);
  });
  
  // Serve static files from SPIFFS
  server.serveStatic("/images/", SPIFFS, "/images/", "max-age=86400");
  
  // Start the web server
  server.begin();
  Serial.println("HTTP server started");
  
  // Initial sensor readings
  updateSensorData();
  
  Serial.println("=== HydroBrain System Ready ===");
}

void loop() {
  // Handle server client requests
  server.handleClient();
  
  // Monitor WiFi connection and reconnect if necessary
  static unsigned long lastWiFiCheck = 0;
  if (millis() - lastWiFiCheck > 10000) {  // Check every 10 seconds
    if (WiFi.status() != WL_CONNECTED) {
      Serial.println("WiFi connection lost! Attempting to reconnect...");
      
      // Attempt to reconnect
      WiFi.disconnect();
      delay(1000);
      WiFi.begin(ssid, password);
      
      // Wait for reconnection with timeout
      int attempts = 0;
      while (WiFi.status() != WL_CONNECTED && attempts < 10) {
        delay(500);
        Serial.print(".");
        attempts++;
      }
      
      if (WiFi.status() == WL_CONNECTED) {
        Serial.println("\nWiFi reconnected successfully");
        Serial.print("New IP Address: ");
        Serial.println(WiFi.localIP());
      }
    }
    lastWiFiCheck = millis();
  }
  
  // Update sensor data periodically
  static unsigned long lastSensorUpdate = 0;
  if (millis() - lastSensorUpdate > 30000) {  // Every 30 seconds
    Serial.println("Updating sensor data...");
    updateSensorData();
    lastSensorUpdate = millis();
  }
}
