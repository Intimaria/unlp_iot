# ESP32 Advanced Air Quality Monitoring Station

An IoT station built for ESP32. This project features an asynchronous web server, separate frontend using SPIFFS, mobile compatibility, dynamic network configuration, and a complete monitoring stack with Prometheus and Grafana.

## Features
* **Asynchronous Architecture:** Uses `ESPAsyncWebServer` to support multiple clients without blocking the processor.
* **Separate Frontend (SPIFFS):** HTML, CSS, and JS are served directly from the ESP32's flash memory, keeping the C++ code clean.
* **Single Page Application (SPA):** Uses AJAX (`fetch`) to update telemetry every 2 seconds without page reloads.
* **Live History Graph:** Integrates `Chart.js` for real-time visualization of gas levels directly in the browser.
* **Captive Portal:** Uses `WiFiManager` to dynamically configure Wi-Fi credentials without hardcoding them.
* **Monitoring Stack:** Exposes a native `/metrics` endpoint for **Prometheus**, and includes a `docker-compose.yml` to spin up **Grafana** locally.

## Hardware Requirements
* **ESP32 Development Board**
* **DHT22 Sensor** (Temperature & Humidity) - Connected to `Pin 4`
* **MQ-135 Sensor** (Air Quality / Gas) - Connected to `Pin 34` (Analog Pin)
* **Onboard LED** - `Pin 2`

## Environment Setup

This project was developed on Ubuntu 24.04 LTS and is intended to be run on this operating system, although the code is compatible with other operating systems.

### 1. USB Permissions (Linux)
To allow the Arduino IDE to communicate with the ESP32 via USB, you must add your user to the `dialout` group:
```bash
sudo usermod -a -G dialout $USER
```
*(Reboot for changes to take effect).*

### 2. Arduino IDE Libraries
Install the following libraries from the Arduino Library Manager (Tools > Manage Libraries):
* `ESPAsyncWebServer` (by me-no-dev)
* `AsyncTCP` (by me-no-dev)
* `WiFiManager` (by tzapu)
* `DHT sensor library` (by Adafruit)

## Installation and Deployment

### Step 1: Upload the C++ Code
1. Open `TP1_AsyncServer_Advanced.ino` in your Arduino IDE.
2. Select your ESP32 board and the correct COM/USB port.
3. Click **Upload**.

### Step 2: Upload the Web Files (SPIFFS)
The standard way to upload the `data` folder to the ESP32 is using the **ESP32 Sketch Data Upload** plugin for the Arduino IDE.

1. Install the plugin following the [official instructions](https://github.com/me-no-dev/arduino-esp32fs-plugin).
2. In the Arduino IDE, go to **Tools > ESP32 Sketch Data Upload**.
3. Wait for the process to finish.

#### Addendum: Linux Snap / Special Environments
If you are using the Arduino IDE installed via **Snap** (common in Ubuntu 24.04), the plugin above may fail due to sandbox restrictions. To solve this, a custom bash script is provided in this repository:

1. Make the script executable: `chmod +x subir_web.sh`
2. There are some configurations in the script, such as the USB port and the baud rate that may change.
3. After reviewing the configurations, run the script: `./subir_web.sh`

*(Note: Ensure the Arduino IDE Serial Monitor is CLOSED before running the script, otherwise the port will be busy).*

### Step 3: Configure Wi-Fi
1. Power on the ESP32.
2. From your phone or computer, connect to the new open Wi-Fi network called **`ESP32_Clima_Config`**.
3. A captive portal will automatically open. Select your home Wi-Fi network and enter the password.
4. The ESP32 will reboot, connect to your router, and show its new IP address in the Serial Monitor.
5. Enter that IP address in your browser to see the control panel.

## Integration with Grafana and Prometheus
To collect long-term historical data and create advanced dashboards, a Docker stack is included.
1. Open a terminal in the project folder and run:
   ```bash
   docker-compose up -d
   ```
2. Enter Grafana from your browser: `http://localhost:3000`.
3. Log in with credentials `admin` / `admin`.
4. Add **Prometheus** as a Data Source using the URL `http://prometheus:9090`.
   *(**IMPORTANT**: Do NOT use `localhost`, as Grafana runs inside a Docker container and needs to use the service name to find Prometheus).*
5. **Import the Dashboard:**
   - In the left menu, go to **Dashboards -> Import**.
   - Click **"Upload JSON file"** and select the `grafana_dashboard.json` file included in this folder.
   - Select Prometheus as the Data Source and click Import.
   - You will see an Air Quality control panel in Grafana.
