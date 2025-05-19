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
#include <HTTPClient.h>
#include <time.h>


// Function prototypes
const char* firebaseHost = "https://hydrobrain-1f3c2-default-rtdb.firebaseio.com";
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
#define pH_PIN 34
#define TRIG_PIN 14
#define ECHO_PIN 15
#define WATER_TEMP_PIN 36

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
float ec = 0; // EC in μS/cm
long waterLevel = 0;
float pH = 0;
float waterTemp = 0;
bool ledStatus = false;
bool pumpStatus = false;
unsigned long lastCheck = 0;
// Add this at the top of your code where other global variables are defined
bool useFakeData = true; // Set to true to use fake data, false to use real sensors
unsigned long lastFakeDataUpdate = 0;

// Global variable to track LED mode
int ledMode = 0; // 0: Off, 1: Sun Mode, 2: Relaxing Mode, 3: Sleeping Mode

// Include the entire HTML content 
// Include the entire HTML content 
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
    background: linear-gradient(to bottom right, #e8f5e9, #d0f0e0);
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
    .notification-item.warning {
      background-color: rgba(245, 158, 11, 0.1);
      border-left: 3px solid #f59e0b;
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
    .plant-btn {
      transition: all 0.2s ease;
    }
    .plant-btn.active {
      background-color: #047857;
      color: white;
      transform: scale(1.05);
    }
    .plant-btn:hover:not(.active) {
      background-color: #d1fae5;
      transform: translateY(-2px);
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

    <!-- Plant Selection Section -->
    <div class="p-4 bg-header text-white border-b border-green-100">
      <div class="flex flex-col md:flex-row justify-between items-center">
        <h2 class="text-lg font-semibold text-green-200 mb-3 md:mb-0">
          <i class="fas fa-seedling mr-2"></i>
          Select Plant Type
        </h2>
        <div class="flex space-x-3">
          <button type="button" onclick="updatePlantProfile('mint')" class="plant-btn px-6 py-2 bg-white rounded-lg shadow-sm border border-green-300 text-green-800 font-medium active" id="mint-btn">
            Mint
          </button>
          <button type="button" onclick="updatePlantProfile('lettuce')" class="plant-btn px-6 py-2 bg-white rounded-lg shadow-sm border border-green-300 text-green-800 font-medium" id="lettuce-btn">
            Lettuce
          </button>
          <button type="button" onclick="updatePlantProfile('strawberry')" class="plant-btn px-6 py-2 bg-white rounded-lg shadow-sm border border-green-300 text-green-800 font-medium" id="strawberry-btn">
            Strawberry
          </button>
        </div>
      </div>
      <div class="mt-3 text-sm md:text-base text-green-200 text-center font-medium">
        <span class="text-white">Selected:</span> <span id="current-plant" class="text-white">Mint</span> - 
        <span id="plant-description" class="text-green-200">Optimal for essential oil production, requires moderate nutrients and warm water.</span>
      </div>
    </div>
        <button onclick="openAddPlantForm()" class="plant-btn px-6 py-2 bg-green-100 border border-green-300 rounded-lg text-green-700 font-medium">
          + Add Plant
        </button>
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
            <div class="mt-2 text-xs text-gray-500">Optimal: <span id="tds-range">400-600 ppm</span></div>
          </div>
        </div>
        
        <!-- EC Card -->
        <div class="sensor-card bg-card p-4 rounded-lg shadow-md optimal" id="ec-card">
          <div class="flex justify-between items-start">
            <h3 class="text-sm font-semibold text-gray-700 flex items-center">
              <i class="fas fa-bolt mr-2"></i>
              EC
            </h3>
            <span class="status-badge optimal" id="ec-status">Optimal</span>
          </div>
          <div class="mt-3">
            <div class="flex items-end">
              <span class="sensor-value text-gray-800" id="ec">--</span>
              <span class="text-sm text-gray-500 ml-1 mb-1">μS/cm</span>
            </div>
            <div class="mt-2 text-xs text-gray-500">Optimal: <span id="ec-range">800-1200 μS/cm</span></div>
          </div>
        </div>

        <!-- pH Card -->
        <div class="sensor-card bg-card p-4 rounded-lg shadow-md optimal" id="pH-card">
          <div class="flex justify-between items-start">
            <h3 class="text-sm font-semibold text-gray-700 flex items-center">
              <i class="fas fa-vial mr-2"></i>
              pH Level
            </h3>
            <span class="status-badge optimal" id="pH-status">Optimal</span>
          </div>
          <div class="mt-3">
            <div class="flex items-end">
              <span class="sensor-value text-gray-800" id="pH">--</span>
            </div>
            <div class="mt-2 text-xs text-gray-500">Optimal: <span id="pH-range">5.5-6.5</span></div>
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
            <div class="mt-2 text-xs text-gray-500">Optimal: <span id="water-temp-range">20-25 °C</span></div>
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
            <div class="mt-2 text-xs text-gray-500">Optimal: <span id="humidity-range">55-70%</span></div>
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
            <div class="mt-2 text-xs text-gray-500">Optimal: <span id="temp-range">22-28 °C</span></div>
          </div>
        </div>

        <!-- Nutrient Tank Level with improved visuals -->
        <div class="bg-card p-4 rounded-lg shadow-md col-span-2 md:col-span-3" id="water-level-card">
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
              <span class="font-medium">HydroOS 1.3.0</span>
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
              <span class="text-gray-500">Current Plant</span>
              <span class="font-medium" id="current-plant-info">Mint</span>
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
        © 2025 <a href="https://www.linkedin.com/in/sultanal-jrboa/" target="_blank" class="underline hover:text-green-200 transition">Sultan Al-Jarboa</a> • Version 1.3.0
      </div>
    </div>
  </div>

  <script>
    // Initialize System Data
    let systemData = {
      ledMode: 0,
      pumpStatus: false,
      bootTime: new Date().getTime(),
      currentPlant: 'mint',
      notifications: [
        { message: "System initialized", type: "info", time: formatTime(new Date()) }
      ]
    };
    
    // Plant profiles with optimal ranges
    // Plant profiles with optimal ranges
    const plantProfiles = {
      lettuce: {
        name: "Lettuce",
        description: "Fast-growing leafy green, prefers cooler temperatures and moderate nutrients.",
        tds: { min: 560, max: 840 },
        ec: { min: 1100, max: 1700 },
        pH: { min: 5.5, max: 6.5 },
        waterTemp: { min: 18, max: 22 },
        humidity: { min: 50, max: 70 },
        airTemp: { min: 16, max: 24 }
      },
      basil: {
        name: "Basil",
        description: "Fragrant herb used in cooking, thrives in warm nutrient-rich environments.",
        tds: { min: 560, max: 700 },
        ec: { min: 1100, max: 1400 },
        pH: { min: 5.5, max: 6.5 },
        waterTemp: { min: 22, max: 26 },
        humidity: { min: 50, max: 60 },
        airTemp: { min: 20, max: 28 }
      },
      mint: {
        name: "Mint",
        description: "Optimal for essential oil production, requires moderate nutrients and warm water.",
        tds: { min: 400, max: 600 },
        ec: { min: 800, max: 1200 },
        pH: { min: 5.5, max: 6.5 },
        waterTemp: { min: 20, max: 25 },
        humidity: { min: 55, max: 70 },
        airTemp: { min: 22, max: 28 }
      },
      strawberry: {
        name: "Strawberry",
        description: "Fruit-bearing plant that requires higher nutrient concentration and humidity.",
        tds: { min: 560, max: 1120 },
        ec: { min: 1120, max: 2240 },
        pH: { min: 5.5, max: 6.5 },
        waterTemp: { min: 18, max: 24 },
        humidity: { min: 70, max: 80 },
        airTemp: { min: 18, max: 24 }
      },
      tomato: {
        name: "Tomato",
        description: "Requires strong light and nutrients; ideal for fruiting hydroponic setups.",
        tds: { min: 1400, max: 3500 },
        ec: { min: 2000, max: 5000 },
        pH: { min: 5.5, max: 6.5 },
        waterTemp: { min: 20, max: 26 },
        humidity: { min: 60, max: 70 },
        airTemp: { min: 18, max: 26 }
      },
      cucumber: {
        name: "Cucumber",
        description: "Vine plant that needs support and high nutrient flow.",
        tds: { min: 1190, max: 1750 },
        ec: { min: 1700, max: 2500 },
        pH: { min: 5.5, max: 6.0 },
        waterTemp: { min: 22, max: 26 },
        humidity: { min: 60, max: 70 },
        airTemp: { min: 20, max: 28 }
      },
      spinach: {
        name: "Spinach",
        description: "Cool-season crop requiring slightly higher EC and pH values.",
        tds: { min: 1260, max: 1610 },
        ec: { min: 1800, max: 2300 },
        pH: { min: 6.0, max: 7.0 },
        waterTemp: { min: 16, max: 22 },
        humidity: { min: 60, max: 70 },
        airTemp: { min: 18, max: 24 }
      },
      arugula: {
        name: "Arugula",
        description: "Peppery leaf vegetable; fast growing and nutrient hungry.",
        tds: { min: 980, max: 1260 },
        ec: { min: 1400, max: 1800 },
        pH: { min: 6.0, max: 7.0 },
        waterTemp: { min: 18, max: 22 },
        humidity: { min: 60, max: 70 },
        airTemp: { min: 18, max: 26 }
      },
      kale: {
        name: "Kale",
        description: "Hardy leafy green, tolerates cooler temperatures well.",
        tds: { min: 1050, max: 1400 },
        ec: { min: 1500, max: 2000 },
        pH: { min: 6.0, max: 7.5 },
        waterTemp: { min: 16, max: 22 },
        humidity: { min: 55, max: 70 },
        airTemp: { min: 16, max: 26 }
      },
      parsley: {
        name: "Parsley",
        description: "Mild herb used fresh or dried; thrives in moderate environments.",
        tds: { min: 560, max: 1260 },
        ec: { min: 1100, max: 1800 },
        pH: { min: 5.5, max: 6.0 },
        waterTemp: { min: 20, max: 26 },
        humidity: { min: 60, max: 70 },
        airTemp: { min: 18, max: 26 }
      },
      coriander: {
        name: "Coriander",
        description: "Fresh herb with dual uses: leaves (cilantro) and seeds.",
        tds: { min: 700, max: 1260 },
        ec: { min: 1400, max: 1800 },
        pH: { min: 6.0, max: 6.8 },
        waterTemp: { min: 18, max: 24 },
        humidity: { min: 60, max: 70 },
        airTemp: { min: 18, max: 25 }
      },
      chives: {
        name: "Chives",
        description: "Thin-leaved herb from the onion family; delicate flavor.",
        tds: { min: 980, max: 1260 },
        ec: { min: 1400, max: 1800 },
        pH: { min: 6.0, max: 6.5 },
        waterTemp: { min: 18, max: 24 },
        humidity: { min: 60, max: 75 },
        airTemp: { min: 18, max: 25 }
      }
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

    // Function to update plant profile display
    function updatePlantProfile(plantType) {
      const profile = plantProfiles[plantType];
      if (!profile) return;
      
      systemData.currentPlant = plantType;
      
      // Update UI elements
      document.getElementById('current-plant').textContent = profile.name;
      document.getElementById('current-plant-info').textContent = profile.name;
      document.getElementById('plant-description').textContent = profile.description;
      
      // Update optimal ranges in UI
      document.getElementById('tds-range').textContent = `${profile.tds.min}-${profile.tds.max} ppm`;
      document.getElementById('ec-range').textContent = `${profile.ec.min}-${profile.ec.max} μS/cm`; // Add EC range
      document.getElementById('pH-range').textContent = `${profile.pH.min}-${profile.pH.max}`;
      document.getElementById('water-temp-range').textContent = `${profile.waterTemp.min}-${profile.waterTemp.max} °C`;
      document.getElementById('humidity-range').textContent = `${profile.humidity.min}-${profile.humidity.max}%`;
      document.getElementById('temp-range').textContent = `${profile.airTemp.min}-${profile.airTemp.max} °C`;
          
      // Update button states
      document.querySelectorAll('.plant-btn').forEach(btn => {
        btn.classList.remove('active');
      });
      
      document.getElementById(`${plantType}-btn`).classList.add('active');
      
      // Revalidate sensors with new ranges
      validateAllSensors();
      
      // Add notification
      addNotification(`Plant profile changed to ${profile.name}`, 'success');
    }

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

    // Function to validate all sensor readings based on current plant profile
    function validateAllSensors() {
      const profile = plantProfiles[systemData.currentPlant];
      if (!profile) return;

      const tds = parseFloat(document.getElementById('tds').textContent.replace(/[^\d.-]/g, ''));
      const ecValue = parseFloat(document.getElementById('ec').textContent.replace(/[^\d.-]/g, ''));
      const pHValue = parseFloat(document.getElementById('pH').textContent.replace(/[^\d.-]/g, ''));
      const waterTempValue = parseFloat(document.getElementById('water-temp').textContent.replace(/[^\d.-]/g, ''));
      const humidityValue = parseFloat(document.getElementById('humidity').textContent.replace(/[^\d.-]/g, ''));
      const airTempValue = parseFloat(document.getElementById('temp').textContent.replace(/[^\d.-]/g, ''));

      validateSensor(tds, profile.tds.min, profile.tds.max, 'tds', 'tds-status');
      validateSensor(ecValue, profile.ec.min, profile.ec.max, 'ec', 'ec-status');
      validateSensor(pHValue, profile.pH.min, profile.pH.max, 'pH', 'pH-status');
      validateSensor(waterTempValue, profile.waterTemp.min, profile.waterTemp.max, 'water-temp', 'water-temp-status');
      validateSensor(humidityValue, profile.humidity.min, profile.humidity.max, 'humidity', 'humidity-status');
      validateSensor(airTempValue, profile.airTemp.min, profile.airTemp.max, 'temp', 'temp-status');
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
            label: 'EC', 
            data: [0, 0, 0, 0, 0], 
            borderColor: 'rgba(249, 115, 22, 0.7)', 
            backgroundColor: 'rgba(249, 115, 22, 0.1)',
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
  function updateChartData(tds, ecValue, pHValue, waterTempValue, humidityValue) {
    const chart = sensorChart;
    const currentTime = new Date().toLocaleTimeString([], {hour: '2-digit', minute: '2-digit'});
    
    // Shift labels
    chart.data.labels.shift();
    chart.data.labels.push(currentTime);

    // Update each dataset with proper parameters
    // Dataset 0: TDS
    chart.data.datasets[0].data.shift();
    chart.data.datasets[0].data.push(tds);

    // Dataset 1: EC
    chart.data.datasets[1].data.shift();
    chart.data.datasets[1].data.push(ecValue);

    // Dataset 2: pH
    chart.data.datasets[2].data.shift();
    chart.data.datasets[2].data.push(pHValue);

    // Dataset 3: Water Temp
    chart.data.datasets[3].data.shift();
    chart.data.datasets[3].data.push(waterTempValue);

    // Dataset 4: Humidity
    chart.data.datasets[4].data.shift();
    chart.data.datasets[4].data.push(humidityValue);

    chart.update('none'); // Use 'none' for smoother updates
  }

    // Function to fetch sensor data from ESP32
    async function fetchSensorData() {
  try {
    const response = await fetch('/data');
    const data = await response.json();
    
    // Debug log to see what's being received
    console.log("Data received from ESP32:", data);
    
    // First check if connection is good by presence of data
    if (!data) throw new Error('No data received');
    
    // Update system status to online with pulse animation
    document.getElementById('system-status').textContent = 'Operational';
    document.getElementById('system-status').classList.remove('text-yellow-300', 'text-red-300');
    document.getElementById('system-status').classList.add('text-green-300');
    
    // Update sensor displays with animation - add null/undefined checks
    if (data.tds !== undefined && data.tds !== null) {
      animateValueChange('tds', parseFloat(data.tds).toFixed(1));
    }
    
    if (data.ec !== undefined && data.ec !== null) {
      animateValueChange('ec', parseFloat(data.ec).toFixed(1));
    }
    
    if (data.pH !== undefined && data.pH !== null) {
      animateValueChange('pH', parseFloat(data.pH).toFixed(1));
    }
    
    if (data.waterTemp !== undefined && data.waterTemp !== null) {
      animateValueChange('water-temp', parseFloat(data.waterTemp).toFixed(1));
    }
    
    if (data.humidity !== undefined && data.humidity !== null) {
      animateValueChange('humidity', parseFloat(data.humidity).toFixed(1));
    }
    
    if (data.airTemp !== undefined && data.airTemp !== null) {
      animateValueChange('temp', parseFloat(data.airTemp).toFixed(1));
    }
    
    // Update water level display with smooth animation
    if (data.waterLevel !== undefined && data.waterLevel !== null) {
      const waterLevelEl = document.getElementById('water-level');
      const waterLevelTextEl = document.getElementById('water-level-text');
      const waterLevelValue = parseFloat(data.waterLevel);
      
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
    }
    
    // Update last check time
    document.getElementById('last-check').textContent = 'Just now';
    document.getElementById('next-check').textContent = '30 seconds';
    
    setTimeout(() => {
      document.getElementById('last-check').textContent = 'A moment ago';
    }, 5000);
    
    // Update chart with new data points - ensure all values exist
    const tdsValue = data.tds !== undefined ? parseFloat(data.tds) : 0;
    const ecValue = data.ec !== undefined ? parseFloat(data.ec) : 0;
    const pHValue = data.pH !== undefined ? parseFloat(data.pH) : 0;
    const waterTempValue = data.waterTemp !== undefined ? parseFloat(data.waterTemp) : 0;
    const humidityValue = data.humidity !== undefined ? parseFloat(data.humidity) : 0;
    
    updateChartData(tdsValue, ecValue, pHValue, waterTempValue, humidityValue);
    
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
    }async function fetchSensorData() {
      try {
        const response = await fetch('/data');
        const data = await response.json();
        
        // Debug log to see what's being received
        console.log("Data received from ESP32:", data);
        
        // First check if connection is good by presence of data
        if (!data) throw new Error('No data received');
        
        // Update system status to online with pulse animation
        document.getElementById('system-status').textContent = 'Operational';
        document.getElementById('system-status').classList.remove('text-yellow-300', 'text-red-300');
        document.getElementById('system-status').classList.add('text-green-300');
        
        // Update sensor displays with animation - add null/undefined checks
        if (data.tds !== undefined && data.tds !== null) {
          animateValueChange('tds', parseFloat(data.tds).toFixed(1));
        }
        
        if (data.ec !== undefined && data.ec !== null) {
          animateValueChange('ec', parseFloat(data.ec).toFixed(1));
        }
        
        if (data.pH !== undefined && data.pH !== null) {
          animateValueChange('pH', parseFloat(data.pH).toFixed(1));
        }
        
        if (data.waterTemp !== undefined && data.waterTemp !== null) {
          animateValueChange('water-temp', parseFloat(data.waterTemp).toFixed(1));
        }
        
        if (data.humidity !== undefined && data.humidity !== null) {
          animateValueChange('humidity', parseFloat(data.humidity).toFixed(1));
        }
        
        if (data.airTemp !== undefined && data.airTemp !== null) {
          animateValueChange('temp', parseFloat(data.airTemp).toFixed(1));
        }
        
        // Update water level display with smooth animation
        if (data.waterLevel !== undefined && data.waterLevel !== null) {
          const waterLevelEl = document.getElementById('water-level');
          const waterLevelTextEl = document.getElementById('water-level-text');
          const waterLevelValue = parseFloat(data.waterLevel);
          
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
        }
        
        // Update last check time
        document.getElementById('last-check').textContent = 'Just now';
        document.getElementById('next-check').textContent = '30 seconds';
        
        setTimeout(() => {
          document.getElementById('last-check').textContent = 'A moment ago';
        }, 5000);
        
        // Update chart with new data points - ensure all values exist
        const tdsValue = data.tds !== undefined ? parseFloat(data.tds) : 0;
        const ecValue = data.ec !== undefined ? parseFloat(data.ec) : 0;
        const pHValue = data.pH !== undefined ? parseFloat(data.pH) : 0;
        const waterTempValue = data.waterTemp !== undefined ? parseFloat(data.waterTemp) : 0;
        const humidityValue = data.humidity !== undefined ? parseFloat(data.humidity) : 0;
        
        updateChartData(tdsValue, ecValue, pHValue, waterTempValue, humidityValue);
        
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
          ec: 1041.0, // EC value (TDS * 2)
          pH: 6.2,
          waterTemp: 22.5,
          humidity: 65,
          airTemp: 24.3,
          waterLevel: 78,
          ledMode: 1,
          pumpStatus: true
        };
        
        document.getElementById('tds').textContent = demoData.tds.toFixed(1);
        document.getElementById('pH').textContent = demoData.pH.toFixed(1);
        document.getElementById('ec').textContent = demoData.ec.toFixed(1);
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
          updateChartData(demoData.tds, demoData.ec, demoData.pH, demoData.waterTemp, demoData.humidity);
        
        addNotification('Demo mode activated', 'info');
      }
    }, 3000);
      function addNewPlantProfile() {
    const name = document.getElementById('new-name').value.trim().toLowerCase();
    if (!name) return alert("Please enter a valid name");

    const lowerName = name.toLowerCase();
    plantProfiles[lowerName] = {
      name: document.getElementById('new-name').value.trim(),
      description: document.getElementById('new-desc').value.trim(),
      tds: { min: parseFloat(document.getElementById('new-tds-min').value), max: parseFloat(document.getElementById('new-tds-max').value) },
      ec: { min: parseFloat(document.getElementById('new-ec-min').value), max: parseFloat(document.getElementById('new-ec-max').value) },
      pH: { min: parseFloat(document.getElementById('new-ph-min').value), max: parseFloat(document.getElementById('new-ph-max').value) },
      waterTemp: { min: parseFloat(document.getElementById('new-water-min').value), max: parseFloat(document.getElementById('new-water-max').value) },
      humidity: { min: parseFloat(document.getElementById('new-humidity-min').value), max: parseFloat(document.getElementById('new-humidity-max').value) },
      airTemp: { min: parseFloat(document.getElementById('new-air-min').value), max: parseFloat(document.getElementById('new-air-max').value) }
    };

    // إضافة زر جديد في القائمة:
    const newBtn = document.createElement('button');
    newBtn.className = 'plant-btn px-6 py-2 bg-white rounded-lg shadow-sm border border-green-300 text-green-800 font-medium';
    newBtn.textContent = plantProfiles[lowerName].name;
    newBtn.onclick = () => updatePlantProfile(name);
    document.querySelector('.flex.space-x-3').appendChild(newBtn);

    closeAddPlantForm();
    updatePlantProfile(name); // اختيارها مباشرة بعد الإضافة
    addNotification(`Added new plant: ${plantProfiles[name].name}`, 'success');
  }
  function openAddPlantForm() {
    document.getElementById('add-plant-modal').classList.remove('hidden');
  }

  function closeAddPlantForm() {
    document.getElementById('add-plant-modal').classList.add('hidden');
  }
  // Fill form fields automatically when plant name matches
window.addEventListener("DOMContentLoaded", () => {
  const nameInput = document.getElementById("new-name");

  nameInput.addEventListener("input", function () {
    const inputName = this.value.trim().toLowerCase();
    const profile = plantProfiles[inputName];

    if (profile) {
      document.getElementById("new-desc").value = profile.description || '';
      document.getElementById("new-tds-min").value = profile.tds.min || '';
      document.getElementById("new-tds-max").value = profile.tds.max || '';
      document.getElementById("new-ec-min").value = profile.ec.min || '';
      document.getElementById("new-ec-max").value = profile.ec.max || '';
      document.getElementById("new-ph-min").value = profile.pH.min || '';
      document.getElementById("new-ph-max").value = profile.pH.max || '';
      document.getElementById("new-water-min").value = profile.waterTemp.min || '';
      document.getElementById("new-water-max").value = profile.waterTemp.max || '';
      document.getElementById("new-humidity-min").value = profile.humidity.min || '';
      document.getElementById("new-humidity-max").value = profile.humidity.max || '';
      document.getElementById("new-air-min").value = profile.airTemp.min || '';
      document.getElementById("new-air-max").value = profile.airTemp.max || '';
    }
  });
});


  </script>
<!-- نموذج إضافة نبات جديد -->
<div id="add-plant-modal" class="fixed inset-0 bg-black bg-opacity-40 flex items-center justify-center hidden z-50">
  <div class="bg-white rounded-lg p-6 w-full max-w-md shadow-lg">
    <h2 class="text-xl font-bold text-gray-800 mb-4">Add New Plant Profile</h2>
    <div class="grid grid-cols-2 gap-4 text-sm">
      <input type="text" placeholder="Plant Name" id="new-name" class="border rounded p-2" />
      <input type="text" placeholder="Description" id="new-desc" class="border rounded p-2 col-span-2" />
      <input type="number" placeholder="TDS Min" id="new-tds-min" class="border rounded p-2" />
      <input type="number" placeholder="TDS Max" id="new-tds-max" class="border rounded p-2" />
      <input type="number" placeholder="EC Min" id="new-ec-min" class="border rounded p-2" />
      <input type="number" placeholder="EC Max" id="new-ec-max" class="border rounded p-2" />
      <input type="number" placeholder="pH Min" id="new-ph-min" class="border rounded p-2" />
      <input type="number" placeholder="pH Max" id="new-ph-max" class="border rounded p-2" />
      <input type="number" placeholder="Water Temp Min" id="new-water-min" class="border rounded p-2" />
      <input type="number" placeholder="Water Temp Max" id="new-water-max" class="border rounded p-2" />
      <input type="number" placeholder="Humidity Min" id="new-humidity-min" class="border rounded p-2" />
      <input type="number" placeholder="Humidity Max" id="new-humidity-max" class="border rounded p-2" />
      <input type="number" placeholder="Air Temp Min" id="new-air-min" class="border rounded p-2" />
      <input type="number" placeholder="Air Temp Max" id="new-air-max" class="border rounded p-2" />
    </div>
    <div class="flex justify-end mt-6 space-x-2">
      <button onclick="closeAddPlantForm()" class="px-4 py-2 bg-gray-300 rounded hover:bg-gray-400">Cancel</button>
      <button onclick="addNewPlantProfile()" class="px-4 py-2 bg-green-600 text-white rounded hover:bg-green-700">Save</button>
    </div>
  </div>
</div>

</body>
</html>
)rawliteral";
void setupFakeData() {
  // Set fixed fake data for testing
  airTemp = 24.5;
  humidity = 66.7;
  tds = 573.0;  // This value already appears to be working
  ec = 1146.0;  // EC = TDS * 2
  pH = 6.3;
  waterTemp = 22.4;
  waterLevel = 78;
  
  // Debug output
  Serial.println("=== FAKE DATA INITIALIZED ===");
  Serial.println("Air Temp: " + String(airTemp) + "°C");
  Serial.println("Humidity: " + String(humidity) + "%");
  Serial.println("TDS: " + String(tds) + " ppm");
  Serial.println("EC: " + String(ec) + " μS/cm");
  Serial.println("pH: " + String(pH));
  Serial.println("Water Temp: " + String(waterTemp) + "°C");
  Serial.println("Water Level: " + String(waterLevel) + "%");
}
void sendToFirebase() {
  if (WiFi.status() == WL_CONNECTED) {
    HTTPClient http;
    String path = String(firebaseHost) + "/logs.json";

    DynamicJsonDocument doc(384); // Increased size
    doc["temp"] = airTemp;
    doc["humidity"] = humidity;
    doc["tds"] = tds;
    doc["ec"] = ec; // Add EC reading
    doc["pH"] = pH;
    doc["waterTemp"] = waterTemp;
    doc["waterLevel"] = waterLevel;

    // Get current time
    time_t now = time(nullptr);
    struct tm* timeinfo = localtime(&now);
    char timestamp[30];
    strftime(timestamp, sizeof(timestamp), "%Y-%m-%d %H:%M:%S", timeinfo);
    doc["timestamp"] = timestamp;

    String jsonStr;
    serializeJson(doc, jsonStr);

    http.begin(path);
    http.addHeader("Content-Type", "application/json");

    int httpCode = http.POST(jsonStr);
    if (httpCode > 0) {
      Serial.printf("[Firebase] Response: %d\n", httpCode);
    } else {
      Serial.printf("[Firebase] Failed: %s\n", http.errorToString(httpCode).c_str());
    }

    http.end();
  }
}


// Fixed water level at 67%
long readWaterLevelPercentage() {
  // Return fixed percentage instead of using ultrasonic sensor
  return 67;
}
float readWaterTemperature() {
  // Take multiple readings to reduce noise
  const int samples = 20;
  long sum = 0;
  
  for (int i = 0; i < samples; i++) {
    sum += analogRead(WATER_TEMP_PIN);
    delay(10);  // Short delay between readings
  }
  
  // Calculate average reading
  float averageReading = sum / (float)samples;
  
  // Convert to voltage (3.3V reference on ESP32)
  float voltage = averageReading * (3.3 / 4095.0);
  
  // Convert voltage to temperature
  // For a typical 10k NTC thermistor with a 10k series resistor to GND
  // This is using the Steinhart-Hart equation simplified
  float resistance = 10000.0 * (3.3 / voltage - 1.0);
  
  // Constants for 10k NTC thermistor (adjust these based on your specific thermistor)
  float A = 0.001129148;
  float B = 0.000234125;
  float C = 0.0000000876741;
  
  // Steinhart-Hart equation
  float tempK = 1.0 / (A + B * log(resistance) + C * pow(log(resistance), 3));
  float tempC = tempK - 273.15;
  
  // Apply bounds checking for reasonable water temperature values
  if (tempC < 0) tempC = 0;
  if (tempC > 50) tempC = 50;
  
  Serial.print("Water Temp ADC: ");
  Serial.print(averageReading);
  Serial.print(", Voltage: ");
  Serial.print(voltage, 3);
  Serial.print("V, Temp: ");
  Serial.println(tempC, 2);
  
  return tempC;
}
float calculateEC(float tds) {
  // Standard conversion factor: TDS (ppm) = EC (μS/cm) * 0.5
  // So EC (μS/cm) = TDS (ppm) / 0.5
  return tds / 0.5;

  
}

float readTDS() {
  // Temperature compensation coefficient: 0.02 per °C
  float temperatureCoefficient = 1.0 + 0.02 * (waterTemp - 25.0);
  ec = calculateEC(tds);
  
  return tds;
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


float readpH() {
  // Take multiple readings to reduce noise
  
  const int samples = 20;
  long sum = 0;
  
  for (int i = 0; i < samples; i++) {
    sum += analogRead(pH_PIN);
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
  if (useFakeData) {
    // Use fake/simulated data instead of reading from sensors
    // Instead of calling generateFakeData(), let's add small random variations to the existing values
    ec = calculateEC(tds);


    // Add small random variations to simulate sensor changes
    airTemp += random(-5, 6) / 10.0;  // Small random change by +/- 0.5
    humidity += random(-5, 6) / 10.0;
    tds += random(-10, 11);
    ec = tds * 2.0;  // Keep EC consistent with TDS
    pH += random(-2, 3) / 10.0;
    waterTemp += random(-3, 4) / 10.0;
    waterLevel += random(-2, 3);
    
    // Keep all values in reasonable ranges
    if (airTemp < 20) airTemp = 20.0;
    if (airTemp > 30) airTemp = 30.0;
    if (humidity < 50) humidity = 50.0;
    if (humidity > 80) humidity = 80.0;
    if (tds < 300) tds = 300.0;
    if (tds > 800) tds = 800.0;
    if (pH < 5.0) pH = 5.0;
    if (pH > 7.5) pH = 7.5;
    if (waterTemp < 18) waterTemp = 18.0;
    if (waterTemp > 28) waterTemp = 28.0;
    if (waterLevel < 60) waterLevel = 60;
    if (waterLevel > 95) waterLevel = 95;
    
    // Debug output periodically
    static unsigned long lastDebugOutput = 0;
    if (millis() - lastDebugOutput > 30000) {  // Output debug data every 30 seconds
      lastDebugOutput = millis();
      
      Serial.println("Current fake sensor values:");
      Serial.println("Air Temp: " + String(airTemp) + "°C");
      Serial.println("Humidity: " + String(humidity) + "%");
      Serial.println("TDS: " + String(tds) + " ppm");
      Serial.println("EC: " + String(ec) + " μS/cm");
      Serial.println("pH: " + String(pH));
      Serial.println("Water Temp: " + String(waterTemp) + "°C");
      Serial.println("Water Level: " + String(waterLevel) + "%");
    }
  } else {
    // Original sensor reading code for real sensors
    // [existing code for reading real sensors]
  }
  
  // Update the TFT display with the current values (real or fake)
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
  doc["pH"] = pH;
  doc["ec"] = ec;
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
  
  // Make sure to call show() to update the pHysical LEDs
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
  
  // Set initial fake data values
  if (useFakeData) {
    setupFakeData();
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
  Serial.println("IP Address: " + WiFi.localIP().toString());
  tft.print("IP: ");
  tft.println(WiFi.localIP().toString());
  delay(2000);  // Show the IP briefly
  // ✅ مزامنة الوقت بعد التأكد من الاتصال
  configTime(10800, 0, "pool.ntp.org", "time.nist.gov"); 
  Serial.println("Waiting for NTP time sync...");
  server.begin();
  while (time(nullptr) < 100000) {
    Serial.print(".");
    delay(1000);
  }

  Serial.println("\nTime synced.");
  time_t now = time(nullptr);
  Serial.println(ctime(&now));  // ✅ طباعة الوقت بعد المزامنة
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
} else {
    Serial.println("\nWiFi connection failed. Time sync skipped.");
    tft.fillScreen(BACKGROUND_COLOR);
    tft.setCursor(50, 90);
    tft.println("WIFI CONNECTED");
    tft.setCursor(50, 120);
    tft.print("IP: ");
    tft.println(WiFi.localIP().toString());
    delay(2000);  // Show the IP briefly
  
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
  Serial.println("HTTP server started");
  
  // Initial sensor readings
  updateSensorData();
  
  Serial.println("=== HydroBrain System Ready ===");
}
}
void loop() {
  server.handleClient();

  // تحديث بيانات Firebase كل 15 دقيقة (تم تصغير الوقت للاختبار إلى دقيقة واحدة)
  static unsigned long lastFirebaseUpdate = 0;
  if (millis() - lastFirebaseUpdate > 60000) {
    Serial.println("Sending data to Firebase...");
    sendToFirebase();
    lastFirebaseUpdate = millis();
  }

  // التحقق من اتصال الواي فاي وإعادة الاتصال عند الانقطاع كل 10 ثواني
  static unsigned long lastWiFiCheck = 0;
  if (millis() - lastWiFiCheck > 10000) {
    lastWiFiCheck = millis();
    if (WiFi.status() != WL_CONNECTED) {
      Serial.println("WiFi connection lost! Attempting to reconnect...");
      WiFi.disconnect();
      delay(1000);
      WiFi.begin(ssid, password);
    }
  }

  // تحديث بيانات المستشعرات كل 5 ثواني (أكثر تكرارًا للتحديث السريع للوحة التحكم)
  static unsigned long lastSensorUpdate = 0;
  if (millis() - lastSensorUpdate > 5000) {  // Changed from 30000 (30 seconds) to 5000 (5 seconds)
    Serial.println("Updating sensor data...");
    updateSensorData();
    lastSensorUpdate = millis();
  }

static unsigned long pumpStartTime = 0;
static bool pumpIsRunning = false;
static unsigned long lastPumpCycle = 0;

if (!pumpIsRunning && millis() - lastPumpCycle >= 3600000UL) {
  // مرت ساعة، شغل المضخة
  digitalWrite(WATER_PUMP_PIN, HIGH);
  pumpStatus = true;
  pumpStartTime = millis();
  pumpIsRunning = true;
  Serial.println("Pump started for 15 minutes.");
  updateTFTDisplay();
}

if (pumpIsRunning && millis() - pumpStartTime >= 900000UL) {
  // مرت 15 دقيقة، طفي المضخة
  digitalWrite(WATER_PUMP_PIN, LOW);
  pumpStatus = false;
  pumpIsRunning = false;
  lastPumpCycle = millis();
  Serial.println("Pump stopped after 15 minutes.");
  updateTFTDisplay();
}
}