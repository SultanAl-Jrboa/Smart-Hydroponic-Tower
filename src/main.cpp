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
const char* ssid = "Battal 4G_EXTnew";
const char* password = "0505169538";

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
  <title>HydroBrain Dashboard</title>
  <script src="https://cdn.tailwindcss.com"></script>
  <script src="https://cdn.jsdelivr.net/npm/chart.js@4.4.0/dist/chart.umd.min.js"></script>
  <link rel="stylesheet" href="https://cdnjs.cloudflare.com/ajax/libs/font-awesome/6.0.0/css/all.min.css">
  <style>
    body {
      background: url('/images/back.png') no-repeat center center fixed;
      background-size: cover;
    }
    .bg-card {
      background-color: rgba(255, 255, 255, 0.9) !important;
    }
    .bg-header {
      background: linear-gradient(to right, rgba(6, 78, 59, 0.95), rgba(4, 120, 87, 0.95)) !important;
    }
    .sensor-value {
      font-size: 1.75rem;
      font-weight: 700;
      transition: all 0.3s ease-in-out;
    }
    .sensor-card {
      transition: all 0.3s ease;
      border-left: 4px solid transparent;
    }
    .sensor-card.alert {
      border-left: 4px solid #ef4444;
      background-color: rgba(254, 242, 242, 0.9) !important;
    }
    .sensor-card.warning {
      border-left: 4px solid #f59e0b;
      background-color: rgba(255, 251, 235, 0.9) !important;
    }
    .sensor-card.optimal {
      border-left: 4px solid #10b981;
      background-color: rgba(236, 253, 245, 0.9) !important;
    }
    .status-badge {
      padding: 0.15rem 0.5rem;
      border-radius: 9999px;
      font-size: 0.75rem;
      font-weight: 600;
    }
    .status-badge.optimal {
      background-color: rgba(16, 185, 129, 0.1);
      color: #047857;
    }
    .status-badge.alert {
      background-color: rgba(239, 68, 68, 0.1);
      color: #b91c1c;
    }
    .status-badge.warning {
      background-color: rgba(245, 158, 11, 0.1);
      color: #d97706;
    }
    .water-ripple {
      position: absolute;
      bottom: 0;
      left: 0;
      right: 0;
      height: 4px;
      background-color: rgba(255, 255, 255, 0.3);
      animation: ripple 2s infinite linear;
    }
    @keyframes ripple {
      0% { transform: translateY(0); opacity: 0.7; }
      50% { transform: translateY(-10px); opacity: 0; }
      100% { transform: translateY(-10px); opacity: 0; }
    }
    .toggle-switch {
      position: relative;
      display: inline-block;
      width: 50px;
      height: 24px;
    }
    .toggle-switch input {
      opacity: 0;
      width: 0;
      height: 0;
    }
    .slider {
      position: absolute;
      cursor: pointer;
      top: 0;
      left: 0;
      right: 0;
      bottom: 0;
      background-color: #ccc;
      transition: .4s;
      border-radius: 24px;
    }
    .slider:before {
      position: absolute;
      content: "";
      height: 18px;
      width: 18px;
      left: 3px;
      bottom: 3px;
      background-color: white;
      transition: .4s;
      border-radius: 50%;
    }
    input:checked + .slider {
      background-color: #10b981;
    }
    input:checked + .slider:before {
      transform: translateX(26px);
    }
    .control-card {
      background-color: rgba(255, 255, 255, 0.9);
      border-radius: 0.5rem;
      padding: 0.75rem;
      display: flex;
      justify-content: space-between;
      align-items: center;
      margin-bottom: 0.75rem;
      transition: all 0.3s ease;
    }
    .control-card:hover {
      transform: translateY(-2px);
      box-shadow: 0 4px 6px -1px rgba(0, 0, 0, 0.1), 0 2px 4px -1px rgba(0, 0, 0, 0.06);
    }
    .notification-item {
      padding: 0.5rem;
      border-radius: 0.375rem;
      margin-bottom: 0.5rem;
      font-size: 0.75rem;
    }
    .notification-item.info {
      background-color: rgba(59, 130, 246, 0.1);
      border-left: 3px solid #3b82f6;
    }
    .notification-item.success {
      background-color: rgba(16, 185, 129, 0.1);
      border-left: 3px solid #10b981;
    }
    .pulse {
      animation: pulse 2s infinite;
    }
    @keyframes pulse {
      0% {
        transform: scale(0.95);
        box-shadow: 0 0 0 0 rgba(16, 185, 129, 0.7);
      }
      70% {
        transform: scale(1);
        box-shadow: 0 0 0 10px rgba(16, 185, 129, 0);
      }
      100% {
        transform: scale(0.95);
        box-shadow: 0 0 0 0 rgba(16, 185, 129, 0);
      }
    }
  </style>
</head>
<body class="min-h-screen flex flex-col items-center justify-center p-4">
  <div class="w-full max-w-6xl mx-auto bg-card rounded-xl shadow-2xl overflow-hidden border border-gray-200">
    <!-- Header -->
    <div class="bg-header p-6">
      <div class="flex flex-col md:flex-row justify-between items-start md:items-center">
        <div>
          <h1 class="text-3xl font-bold text-white flex items-center">
            <i class="fas fa-leaf mr-3"></i>
            HydroBrain Dashboard
          </h1>
          <p class="text-green-200 text-sm mt-1">Enterprise Hydroponic Management System</p>
        </div>
        <div class="text-white mt-3 md:mt-0 flex flex-col items-end">
          <div class="text-lg font-mono" id="current-time">00:00:00</div>
          <div class="text-xs text-green-200" id="current-date">Loading date...</div>
          <div class="mt-1 text-xs px-3 py-1 bg-green-700 rounded-full flex items-center">
            <span class="h-2 w-2 bg-green-300 rounded-full mr-2 animate-pulse"></span>
            System Status: <span id="system-status" class="font-semibold text-green-300">Operational</span>
          </div>
        </div>
      </div>
    </div>

    <!-- Dashboard Grid -->
    <div class="grid md:grid-cols-3 gap-6 p-6">
      <!-- Main Content Area - Sensor Cards -->
      <div class="md:col-span-2 grid grid-cols-2 md:grid-cols-3 gap-4">
        <!-- TDS Card -->
        <div class="sensor-card bg-card p-4 rounded-lg shadow-md optimal" id="tds-card">
          <div class="flex justify-between items-start">
            <h3 class="text-sm font-semibold text-gray-700 flex items-center">
              <i class="fas fa-tachometer-alt mr-2"></i>
              TDS
            </h3>
            <span class="status-badge optimal" id="tds-status">Optimal</span>
          </div>
          <div class="mt-3">
            <div class="flex items-end">
              <span class="sensor-value text-gray-800" id="tds">--</span>
              <span class="text-sm text-gray-500 ml-1 mb-1">ppm</span>
            </div>
            <div class="mt-2 text-xs text-gray-500">Optimal: 400-600 ppm</div>
          </div>
        </div>

        <!-- pH Card -->
        <div class="sensor-card bg-card p-4 rounded-lg shadow-md optimal" id="ph-card">
          <div class="flex justify-between items-start">
            <h3 class="text-sm font-semibold text-gray-700 flex items-center">
              <i class="fas fa-vial mr-2"></i>
              pH Level
            </h3>
            <span class="status-badge optimal" id="ph-status">Optimal</span>
          </div>
          <div class="mt-3">
            <div class="flex items-end">
              <span class="sensor-value text-gray-800" id="ph">--</span>
            </div>
            <div class="mt-2 text-xs text-gray-500">Optimal: 5.5-6.5</div>
          </div>
        </div>

        <!-- Water Temp Card -->
        <div class="sensor-card bg-card p-4 rounded-lg shadow-md optimal" id="water-temp-card">
          <div class="flex justify-between items-start">
            <h3 class="text-sm font-semibold text-gray-700 flex items-center">
              <i class="fas fa-temperature-low mr-2"></i>
              Water Temp
            </h3>
            <span class="status-badge optimal" id="water-temp-status">Optimal</span>
          </div>
          <div class="mt-3">
            <div class="flex items-end">
              <span class="sensor-value text-gray-800" id="water-temp">--</span>
              <span class="text-sm text-gray-500 ml-1 mb-1">°C</span>
            </div>
            <div class="mt-2 text-xs text-gray-500">Optimal: 20-25 °C</div>
          </div>
        </div>

        <!-- Humidity Card -->
        <div class="sensor-card bg-card p-4 rounded-lg shadow-md" id="humidity-card">
          <div class="flex justify-between items-start">
            <h3 class="text-sm font-semibold text-gray-700 flex items-center">
              <i class="fas fa-water mr-2"></i>
              Humidity
            </h3>
            <span class="status-badge optimal" id="humidity-status">Optimal</span>
          </div>
          <div class="mt-3">
            <div class="flex items-end">
              <span class="sensor-value text-gray-800" id="humidity">--</span>
              <span class="text-sm text-gray-500 ml-1 mb-1">%</span>
            </div>
            <div class="mt-2 text-xs text-gray-500">Optimal: 55-70%</div>
          </div>
        </div>

        <!-- Air Temp Card -->
        <div class="sensor-card bg-card p-4 rounded-lg shadow-md" id="temp-card">
          <div class="flex justify-between items-start">
            <h3 class="text-sm font-semibold text-gray-700 flex items-center">
              <i class="fas fa-thermometer-half mr-2"></i>
              Air Temp
            </h3>
            <span class="status-badge optimal" id="temp-status">Optimal</span>
          </div>
          <div class="mt-3">
            <div class="flex items-end">
              <span class="sensor-value text-gray-800" id="temp">--</span>
              <span class="text-sm text-gray-500 ml-1 mb-1">°C</span>
            </div>
            <div class="mt-2 text-xs text-gray-500">Optimal: 22-28 °C</div>
          </div>
        </div>

        <!-- Nutrient Tank Level with improved visuals -->
        <div class="bg-card p-4 rounded-lg shadow-md" id="water-level-card">
          <h3 class="text-sm font-semibold text-gray-700 flex items-center">
            <i class="fas fa-fill-drip mr-2"></i>
            Nutrient Tank
          </h3>
          <div class="mt-3">
            <div class="w-full h-28 bg-gray-100 border border-gray-300 rounded-lg relative overflow-hidden">
              <div id="water-level" class="absolute bottom-0 left-0 right-0 bg-green-400 transition-all duration-1000" style="height: 0%;">
                <div class="water-ripple"></div>
                <div class="water-ripple" style="animation-delay: 0.5s"></div>
                <div class="water-ripple" style="animation-delay: 1s"></div>
              </div>
              <div class="absolute inset-0 flex items-center justify-center">
                <div class="bg-white bg-opacity-80 px-3 py-1 rounded-full shadow-sm">
                  <span class="font-bold text-green-700" id="water-level-text">--%</span>
                </div>
              </div>
            </div>
            <div class="mt-2 text-xs text-gray-500 flex justify-between">
              <span>Low</span>
              <span>Optimal</span>
              <span>Full</span>
            </div>
          </div>
        </div>
      </div>

      <!-- Sidebar - Control Panel -->
      <div class="md:col-span-1 grid grid-cols-1 gap-4">
        <!-- System Controls Card -->
        <div class="bg-card p-4 rounded-lg shadow-md">
          <h3 class="text-base font-semibold text-gray-700 flex items-center mb-4">
            <i class="fas fa-sliders-h mr-2"></i>
            System Controls
          </h3>
          
          <div class="space-y-3">
            <!-- LED Control -->
            <div class="control-card" id="led-control">
              <div class="flex items-center">
                <div class="h-8 w-8 rounded-full flex items-center justify-center mr-3 bg-green-100 text-green-600">
                  <i class="fas fa-lightbulb"></i>
                </div>
                <div>
                  <p class="text-sm font-medium text-gray-700">LED Lighting</p>
                  <p class="text-xs text-gray-500" id="led-mode-text">Loading...</p>
                </div>
              </div>
              <button id="led-btn" class="px-3 py-2 bg-green-600 hover:bg-green-700 text-white font-semibold rounded-lg transition duration-300 text-sm">
                <i class="fas fa-power-off mr-1"></i>
                Change Mode
              </button>
            </div>
            
            <!-- Pump Control -->
            <div class="control-card" id="pump-control">
              <div class="flex items-center">
                <div class="h-8 w-8 rounded-full flex items-center justify-center mr-3 bg-blue-100 text-blue-600">
                  <i class="fas fa-tint"></i>
                </div>
                <div>
                  <p class="text-sm font-medium text-gray-700">Nutrient Pump</p>
                  <p class="text-xs text-gray-500" id="pump-status-text">Loading...</p>
                </div>
              </div>
              <button id="pump-btn" class="px-3 py-2 bg-blue-600 hover:bg-blue-700 text-white font-semibold rounded-lg transition duration-300 text-sm">
                <i class="fas fa-power-off mr-1"></i>
                Toggle
              </button>
            </div>
          </div>
          
          <div class="mt-6 pt-4 border-t border-gray-200">
            <div class="flex justify-between text-xs text-gray-600">
              <div>Last System Check:</div>
              <div class="font-medium" id="last-check">--</div>
            </div>
            <div class="mt-2 flex justify-between text-xs text-gray-600">
              <div>Next Scheduled Check:</div>
              <div class="font-medium" id="next-check">--</div>
            </div>
          </div>
        </div>
        
        <!-- System Notifications -->
        <div class="bg-card p-4 rounded-lg shadow-md">
          <h3 class="text-base font-semibold text-gray-700 flex items-center mb-4">
            <i class="fas fa-bell mr-2"></i>
            System Notifications
          </h3>
          
          <div id="notifications-container" class="space-y-2 max-h-64 overflow-y-auto pr-1">
            <div class="notification-item info">
              <div class="flex justify-between">
                <span>System initialized</span>
                <span class="text-gray-500">Loading...</span>
              </div>
            </div>
          </div>
        </div>
        
        <!-- System Information -->
        <div class="bg-card p-4 rounded-lg shadow-md hidden md:block">
          <h3 class="text-base font-semibold text-gray-700 flex items-center mb-3">
            <i class="fas fa-info-circle mr-2"></i>
            System Information
          </h3>
          
          <div class="space-y-2 text-xs">
            <div class="flex justify-between">
              <span class="text-gray-500">System Version</span>
              <span class="font-medium">HydroOS 1.2.0</span>
            </div>
            <div class="flex justify-between">
              <span class="text-gray-500">Uptime</span>
              <span class="font-medium" id="uptime">Loading...</span>
            </div>
            <div class="flex justify-between">
              <span class="text-gray-500">Connected Sensors</span>
              <span class="font-medium">5 / 5 Online</span>
            </div>
            <div class="flex justify-between">
              <span class="text-gray-500">Plants Growing</span>
              <span class="font-medium">Basil, Lettuce, Mint</span>
            </div>
          </div>
        </div>
      </div>
    </div>

    <!-- Chart Section -->
    <div class="p-6 bg-card border-t">
      <div class="flex justify-between items-center mb-4">
        <h2 class="text-xl font-semibold text-gray-700">Sensor Trends</h2>
        <div class="text-sm text-gray-500">
          <i class="fas fa-history mr-1"></i>
          Last 5 Readings
        </div>
      </div>
      <canvas id="sensorChart" class="w-full" height="250"></canvas>
    </div>

    <!-- Footer -->
    <div class="p-4 bg-header text-white text-sm flex flex-col md:flex-row justify-between items-center">
      <div>
        HydroBrain Dashboard • Professional Edition
      </div>
      <div class="text-green-300 mt-2 md:mt-0">
        © 2025 Sultan Al-Jarboa • Version 1.2.0
      </div>
    </div>
  </div>

  <script>
    // Initialize System Data
    let systemData = {
      ledMode: 0,
      pumpStatus: false,
      bootTime: new Date().getTime(),
      notifications: [
        { message: "System initialized", type: "info", time: formatTime(new Date()) }
      ]
    };
    
    // LED Mode Names
    const ledModeNames = ['Off', 'Sun Mode', 'Relaxing Mode', 'Sleeping Mode'];
    const ledModeDescriptions = [
      'LED lights are off',
      'Full spectrum growth lighting',
      'Mid-intensity blue spectrum',
      'Low-intensity red spectrum'
    ];
    
    // Function to format time
    function formatTime(date) {
      return date.toLocaleTimeString([], { hour: '2-digit', minute: '2-digit' });
    }
    
    // Update clock and date
    function updateDateTime() {
      const now = new Date();
      document.getElementById('current-time').textContent = now.toLocaleTimeString([], 
        { hour: '2-digit', minute: '2-digit', second: '2-digit' });
      document.getElementById('current-date').textContent = now.toLocaleDateString([], 
        { weekday: 'long', year: 'numeric', month: 'long', day: 'numeric' });
      
      // Update uptime
      const uptimeMs = now.getTime() - systemData.bootTime;
      const uptimeSec = Math.floor(uptimeMs / 1000);
      const days = Math.floor(uptimeSec / 86400);
      const hours = Math.floor((uptimeSec % 86400) / 3600);
      const mins = Math.floor((uptimeSec % 3600) / 60);
      
      let uptimeStr = '';
      if (days > 0) uptimeStr += `${days} days, `;
      uptimeStr += `${hours} hrs, ${mins} mins`;
      document.getElementById('uptime').textContent = uptimeStr;
    }
    
    setInterval(updateDateTime, 1000);
    updateDateTime();

    // Sensor Validation and Status
    function validateSensor(value, minValue, maxValue, elementId, statusElementId) {
      if (isNaN(value) || value === null) return;
      
      const element = document.getElementById(elementId);
      const statusElement = document.getElementById(statusElementId);
      const card = element.closest('.sensor-card');
      
      // Remove all status classes
      card.classList.remove('optimal', 'warning', 'alert');
      statusElement.classList.remove('optimal', 'warning', 'alert');
      
      let statusClass = 'optimal';
      let statusText = 'Optimal';
      
      if (value < minValue) {
        statusClass = 'warning';
        statusText = 'Low';
      } else if (value > maxValue) {
        statusClass = 'alert';
        statusText = 'High';
      }
      
      // Add appropriate class
      card.classList.add(statusClass);
      statusElement.classList.add(statusClass);
      statusElement.textContent = statusText;
    }

    // Function to validate all sensor readings
    function validateAllSensors() {
      const tdsValue = parseFloat(document.getElementById('tds').textContent);
      const phValue = parseFloat(document.getElementById('ph').textContent);
      const waterTempValue = parseFloat(document.getElementById('water-temp').textContent);
      const humidityValue = parseFloat(document.getElementById('humidity').textContent);
      const airTempValue = parseFloat(document.getElementById('temp').textContent);
      
      validateSensor(tdsValue, 400, 600, 'tds', 'tds-status');
      validateSensor(phValue, 5.5, 6.5, 'ph', 'ph-status');
      validateSensor(waterTempValue, 20, 25, 'water-temp', 'water-temp-status');
      validateSensor(humidityValue, 55, 70, 'humidity', 'humidity-status');
      validateSensor(airTempValue, 22, 28, 'temp', 'temp-status');
    }

    // Update LED status display
    function updateLedDisplay() {
      const ledBtn = document.getElementById('led-btn');
      const ledModeText = document.getElementById('led-mode-text');
      const modeColors = [
        'bg-gray-600 hover:bg-gray-700', 
        'bg-yellow-500 hover:bg-yellow-600', 
        'bg-blue-500 hover:bg-blue-600', 
        'bg-red-500 hover:bg-red-600'
      ];
      
      // Remove previous color classes and add current mode color
      modeColors.forEach(color => {
        ledBtn.classList.remove(...color.split(' '));
      });
      
      ledBtn.classList.add(...modeColors[systemData.ledMode].split(' '));
      
      // Update text
      ledModeText.textContent = ledModeDescriptions[systemData.ledMode];
      ledBtn.innerHTML = `
        <i class="fas fa-${systemData.ledMode === 0 ? 'power-off' : 'sync'}"></i>
        ${systemData.ledMode === 0 ? 'Turn On' : 'Change Mode'}
      `;
    }

    // Update pump status display
    function updatePumpDisplay() {
      const pumpStatusText = document.getElementById('pump-status-text');
      pumpStatusText.textContent = systemData.pumpStatus ? 'Active - Circulating' : 'Inactive - Standby';
      
      const pumpBtn = document.getElementById('pump-btn');
      pumpBtn.textContent = systemData.pumpStatus ? 'Turn Off' : 'Turn On';
      
      // Update button color
      if (systemData.pumpStatus) {
        pumpBtn.classList.remove('bg-gray-600', 'hover:bg-gray-700');
        pumpBtn.classList.add('bg-blue-600', 'hover:bg-blue-700');
      } else {
        pumpBtn.classList.remove('bg-blue-600', 'hover:bg-blue-700');
        pumpBtn.classList.add('bg-gray-600', 'hover:bg-gray-700');
      }
    }
    
    // Add notification to the system
    function addNotification(message, type = 'info') {
      const now = new Date();
      const notification = {
        message,
        type,
        time: formatTime(now)
      };
      
      systemData.notifications.unshift(notification);
      if (systemData.notifications.length > 10) {
        systemData.notifications.pop();
      }
      
      updateNotificationsDisplay();
    }
    
    // Update notifications display
    function updateNotificationsDisplay() {
      const container = document.getElementById('notifications-container');
      container.innerHTML = '';
      
      systemData.notifications.forEach(notification => {
        const div = document.createElement('div');
        div.className = `notification-item ${notification.type}`;
        div.innerHTML = `
          <div class="flex justify-between">
            <span>${notification.message}</span>
            <span class="text-gray-500">${notification.time}</span>
          </div>
        `;
        container.appendChild(div);
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
            tension: 0.4,
            borderWidth: 2
          },
          { 
            label: 'pH', 
            data: [0, 0, 0, 0, 0], 
            borderColor: 'rgba(37, 99, 235, 0.7)', 
            backgroundColor: 'rgba(37, 99, 235, 0.1)',
            tension: 0.4,
            borderWidth: 2
          },
          { 
            label: 'Water Temp', 
            data: [0, 0, 0, 0, 0], 
            borderColor: 'rgba(220, 38, 38, 0.7)', 
            backgroundColor: 'rgba(220, 38, 38, 0.1)',
            tension: 0.4,
            borderWidth: 2
          },
          { 
            label: 'Humidity', 
            data: [0, 0, 0, 0, 0], 
            borderColor: 'rgba(139, 92, 246, 0.7)', 
            backgroundColor: 'rgba(139, 92, 246, 0.1)',
            tension: 0.4,
            borderWidth: 2
          }
        ]
      },
      options: {
        responsive: true,
        interaction: {
          mode: 'index',
          intersect: false
        },
        plugins: {
          legend: {
            position: 'bottom',
            labels: {
              usePointStyle: true,
              boxWidth: 6,
              font: {
                size: 10
              }
            }
          },
          tooltip: {
            backgroundColor: 'rgba(255, 255, 255, 0.9)',
            titleColor: '#333',
            bodyColor: '#333',
            borderColor: '#e5e7eb',
            borderWidth: 1,
            cornerRadius: 4,
            boxPadding: 3,
            usePointStyle: true,
            callbacks: {
              labelPointStyle: function(context) {
                return {
                  pointStyle: 'circle',
                  rotation: 0
                };
              }
            }
          }
        },
        scales: {
          x: { 
            grid: {
                display: true,
                color: 'rgba(0, 0, 0, 0.05)',
                drawOnChartArea: true
              },
              ticks: {
                font: {
                  size: 10
                }
              }
            },
            y: { 
              beginAtZero: false,
              grid: {
                color: 'rgba(0, 0, 0, 0.05)',
              },
              ticks: {
                font: {
                  size: 10
                }
              }
            }
        }
      }
    });

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

      chart.update('none'); // Use 'none' for smoother updates
    }

    // Function to fetch sensor data from ESP32
    async function fetchSensorData() {
      try {
        const response = await fetch('/data');
        const data = await response.json();
        
        // First check if connection is good by presence of data
        if (!data) throw new Error('No data received');
        
        // Update system status to online with pulse animation
        document.getElementById('system-status').textContent = 'Operational';
        document.getElementById('system-status').classList.remove('text-yellow-300', 'text-red-300');
        document.getElementById('system-status').classList.add('text-green-300');
        
        // Update sensor displays with animation
        animateValueChange('tds', data.tds.toFixed(1));
        animateValueChange('ph', data.ph.toFixed(1));
        animateValueChange('water-temp', data.waterTemp.toFixed(1));
        animateValueChange('humidity', data.humidity.toFixed(1));
        animateValueChange('temp', data.airTemp.toFixed(1));
        
        // Update water level display with smooth animation
        const waterLevelEl = document.getElementById('water-level');
        const waterLevelTextEl = document.getElementById('water-level-text');
        const waterLevelValue = data.waterLevel;
        
        // Animated transition for water level
        waterLevelEl.style.height = `${waterLevelValue}%`;
        waterLevelTextEl.textContent = `${Math.round(waterLevelValue)}%`;
        
        // Update water level color based on value
        if (waterLevelValue < 30) {
          waterLevelEl.classList.remove('bg-green-400', 'bg-blue-400');
          waterLevelEl.classList.add('bg-red-400');
        } else if (waterLevelValue < 70) {
          waterLevelEl.classList.remove('bg-green-400', 'bg-red-400');
          waterLevelEl.classList.add('bg-blue-400');
        } else {
          waterLevelEl.classList.remove('bg-blue-400', 'bg-red-400');
          waterLevelEl.classList.add('bg-green-400');
        }
        
        // Update last check time
        document.getElementById('last-check').textContent = 'Just now';
        document.getElementById('next-check').textContent = '30 seconds';
        
        setTimeout(() => {
          document.getElementById('last-check').textContent = 'A moment ago';
        }, 5000);
        
        // Update chart with new data points
        updateChartData(data.tds, data.ph, data.waterTemp, data.humidity);
        
        // Update system data
        if (data.ledMode !== undefined && data.ledMode !== systemData.ledMode) {
          systemData.ledMode = data.ledMode;
          updateLedDisplay();
          addNotification(`LED mode changed to ${ledModeNames[data.ledMode]}`, 'info');
        }
        
        if (data.pumpStatus !== undefined && data.pumpStatus !== systemData.pumpStatus) {
          systemData.pumpStatus = data.pumpStatus;
          updatePumpDisplay();
          addNotification(`Pump ${data.pumpStatus ? 'activated' : 'deactivated'}`, 'info');
        }
        
        // Validate all sensors
        validateAllSensors();
        
      } catch (error) {
        console.error('Error fetching sensor data:', error);
        document.getElementById('system-status').textContent = 'Connection Error';
        document.getElementById('system-status').classList.remove('text-green-300', 'text-yellow-300');
        document.getElementById('system-status').classList.add('text-red-300');
        
        addNotification('Connection error. Retrying...', 'warning');
      }
    }
    
    // Function to animate value changes
    function animateValueChange(elementId, newValue) {
      const element = document.getElementById(elementId);
      const currentValue = element.textContent;
      
      // Skip animation if value is unchanged or initial value
      if (currentValue === newValue || currentValue === '--') {
        element.textContent = newValue;
        return;
      }
      
      // Add highlighting effect
      element.classList.add('transition-all', 'duration-500');
      element.classList.add('text-green-600', 'scale-110');
      
      // Update the value
      element.textContent = newValue;
      
      // Remove highlighting after a delay
      setTimeout(() => {
        element.classList.remove('text-green-600', 'scale-110');
      }, 700);
    }

    // Event Listeners for System Controls
    document.getElementById('led-btn').addEventListener('click', async () => {
      try {
        const response = await fetch('/toggle-led');
        const data = await response.json();
        
        // Update mode and button
        systemData.ledMode = data.mode;
        updateLedDisplay();
        
        addNotification(`LED mode changed to ${ledModeNames[data.mode]}`, 'success');
      } catch (error) {
        console.error('Error toggling LED:', error);
        addNotification('Error changing LED mode', 'error');
      }
    });

    document.getElementById('pump-btn').addEventListener('click', async () => {
      try {
        await fetch('/toggle-pump');
        
        // Toggle local state until next data refresh
        systemData.pumpStatus = !systemData.pumpStatus;
        updatePumpDisplay();
        
        addNotification(`Pump ${systemData.pumpStatus ? 'activated' : 'deactivated'}`, 'success');
        
      } catch (error) {
        console.error('Error toggling pump:', error);
        addNotification('Error toggling pump', 'error');
      }
    });

    // Initialize displays
    updateLedDisplay();
    updatePumpDisplay();
    updateNotificationsDisplay();

    // Initial data fetch and setup interval for updates
    fetchSensorData();
    
    // Update sensor data every 30 seconds
    setInterval(fetchSensorData, 30000);
    
    // Simulate initial data loading for demonstration
    setTimeout(() => {
      if (document.getElementById('tds').textContent === '--') {
        // If no real data arrived after 3 seconds, load demo data
        const demoData = {
          tds: 520.5,
          ph: 6.2,
          waterTemp: 22.5,
          humidity: 65,
          airTemp: 24.3,
          waterLevel: 78,
          ledMode: 1,
          pumpStatus: true
        };
        
        document.getElementById('tds').textContent = demoData.tds.toFixed(1);
        document.getElementById('ph').textContent = demoData.ph.toFixed(1);
        document.getElementById('water-temp').textContent = demoData.waterTemp.toFixed(1);
        document.getElementById('humidity').textContent = demoData.humidity.toFixed(1);
        document.getElementById('temp').textContent = demoData.airTemp.toFixed(1);
        
        document.getElementById('water-level').style.height = `${demoData.waterLevel}%`;
        document.getElementById('water-level-text').textContent = `${demoData.waterLevel}%`;
        
        systemData.ledMode = demoData.ledMode;
        systemData.pumpStatus = demoData.pumpStatus;
        
        updateLedDisplay();
        updatePumpDisplay();
        validateAllSensors();
        
        // Update chart
        updateChartData(demoData.tds, demoData.ph, demoData.waterTemp, demoData.humidity);
        
        addNotification('Demo mode activated', 'info');
      }
    }, 3000);
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
      airTemp = 22.1;   // Default to 25°CC
      
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
  server.sendHeader("Access-Control-Allow-Origin", "*");
  server.sendHeader("Access-Control-Allow-Methods", "GET, POST, OPTIONS");
  server.sendHeader("Access-Control-Allow-Headers", "*");
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
  server.sendHeader("Access-Control-Allow-Origin", "*");
  server.sendHeader("Access-Control-Allow-Methods", "GET, POST, OPTIONS");
  server.sendHeader("Access-Control-Allow-Headers", "*");
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
  server.sendHeader("Access-Control-Allow-Origin", "*");
  server.sendHeader("Access-Control-Allow-Methods", "GET, POST, OPTIONS");
  server.sendHeader("Access-Control-Allow-Headers", "*");
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
    server.sendHeader("Access-Control-Allow-Headers", "*");
    server.send(204);
  });
  server.on("/toggle-led", HTTP_OPTIONS, []() {
  server.sendHeader("Access-Control-Allow-Origin", "*");
  server.sendHeader("Access-Control-Allow-Methods", "GET, POST, OPTIONS");
  server.sendHeader("Access-Control-Allow-Headers", "*");
  server.send(204, "text/plain", "");
});

server.on("/toggle-pump", HTTP_OPTIONS, []() {
  server.sendHeader("Access-Control-Allow-Origin", "*");
  server.sendHeader("Access-Control-Allow-Methods", "GET, POST, OPTIONS");
  server.sendHeader("Access-Control-Allow-Headers", "*");
  server.send(204, "text/plain", "");
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
