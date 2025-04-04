# Smart Hydroponic System

This project is a smart hydroponic system built with various sensors and an ESP32-based microcontroller to monitor and control the environmental factors of a hydroponic setup. The system is equipped with sensors to measure temperature, humidity, water level, pH, TDS (Total Dissolved Solids), and water temperature. The system includes a web interface and a TFT screen to display the readings in real-time.

## Features

- **Real-time sensor monitoring:** Displays live data for temperature, humidity, pH, TDS, water temperature, and water level.
- **Control options:** Allows control of the water pump and LED strip through a web interface.
- **Web Dashboard:** A user-friendly dashboard to monitor system status, display graphs, and manage system controls.
- **TFT Display:** Displays sensor data such as temperature, humidity, TDS, and pH levels.

## Components

- **Microcontroller:** ESP32 (for Wi-Fi connectivity and control).
- **TFT Display:** 3.2" ILI9341 display.
- **DHT22 Sensor:** For humidity and temperature.
- **TDS Sensor:** For monitoring water quality (ppm).
- **pH Sensor:** For monitoring the pH level of the water.
- **Water Level Sensor:** To detect water levels in the tank.
- **Water Temperature Sensor:** To measure the temperature of the water.
- **NeoPixel LED Strip:** For visual feedback (indicating system status or alerts).
- **Water Pump:** Controlled via GPIO pin to pump water as needed.

## Setup

### Required Libraries
- **Adafruit_GFX** and **Adafruit_ILI9341**: For controlling the TFT display.
- **DHT sensor library**: For the DHT22 sensor.
- **Adafruit_NeoPixel**: For controlling the NeoPixel LED strip.
- **WiFi.h**: For connecting the ESP32 to Wi-Fi.
- **WebServer.h**: For setting up the web server to control the system via a browser.

### Hardware Connections
- **TFT Display:**
  - CS: GPIO 5
  - RST: GPIO 4
  - DC: GPIO 2
  - MOSI: GPIO 23
  - CLK: GPIO 18
  - LED: GPIO 33

- **DHT22 Sensor:**
  - Data: GPIO 21

- **TDS Sensor:**
  - Analog Pin: GPIO 35

- **pH Sensor:**
  - Analog Pin: GPIO 34

- **Water Level Sensor:**
  - TRIG: GPIO 14
  - ECHO: GPIO 15

- **Water Temperature Sensor:**
  - Analog Pin: GPIO 32

- **NeoPixel LED Strip:**
  - Pin: GPIO 19

- **Water Pump:**
  - Pin: GPIO 27

### Web Interface
The web interface allows you to control the water pump and LED strip directly from any device connected to the same Wi-Fi network.

- **Turn on/off Water Pump**
- **Turn on/off LED Strip**

### Example Routes
- **/pump/on:** Turns on the water pump.
- **/pump/off:** Turns off the water pump.
- **/led/on:** Turns on the LED strip.
- **/led/off:** Turns off the LED strip.

### Wi-Fi Setup
Make sure to update the `ssid` and `password` variables with your Wi-Fi credentials in the code to allow the ESP32 to connect to the network.

## Dashboard UI

The system comes with a modern web-based dashboard built using **TailwindCSS** and **Chart.js** for real-time data visualization.

- **TDS:** Displays the Total Dissolved Solids level of the water in ppm.
- **pH:** Displays the current pH level of the water.
- **Water Temp:** Displays the water temperature.
- **Humidity and Temperature:** Shows the environmental conditions from the DHT22 sensor.
- **Water Level:** A visual representation of the current water level in the nutrient tank.

### Example Display Cards
- **TDS (Total Dissolved Solids):** Shows the water quality in ppm.
- **pH Level:** Shows the pH of the water, with a status of "Balanced" or "Unbalanced."
- **Water Temp:** Shows the temperature of the water.
- **Humidity:** Displays the humidity in the environment.
- **Ambient Temp:** Displays the temperature of the surrounding environment.
- **Nutrient Tank Level:** A progress bar that shows the water level in the tank.

### Control Buttons
- **LED Control Button:** Turns the LED strip on or off.
- **Pump Control Button:** Turns the water pump on or off.

## Code Walkthrough

### Main Code (Arduino Sketch)

This code initializes the sensors, TFT display, and NeoPixel LED strip. It connects the ESP32 to Wi-Fi and starts a web server to handle user input through a browser. The sensor readings are displayed on the TFT screen, and they are also available for viewing on the web dashboard.

Key functions:
- `readWaterTemperature()`: Reads water temperature.
- `readWaterLevelPercentage()`: Calculates the water level as a percentage.
- `readTDS()`: Measures the Total Dissolved Solids in the water.
- `readPH()`: Reads the pH level from the sensor.

### Web Interface Code (HTML)

The dashboard is built using **TailwindCSS** for styling and **Chart.js** for future data visualizations. It displays the current sensor readings and allows for control over the water pump and LED strip.

Key features:
- Real-time updates on sensor values.
- Control buttons for interacting with the hardware.
- User-friendly interface for monitoring the system.

## Conclusion

This smart hydroponic system provides real-time monitoring and control over your hydroponic setup. By integrating various sensors and using an intuitive web interface, you can keep track of essential parameters such as water quality, pH levels, and temperature, ensuring optimal growth conditions for your plants.

---

Feel free to modify the code and dashboard to meet your specific needs. Enjoy building your smart hydroponic system!
