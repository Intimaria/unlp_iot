# Estación Avanzada de Calidad de Aire con ESP32 

Una estación IoT construida para ESP32. Este proyecto cuenta con un servidor web asincrónico, frontend separado usando SPIFFS, compatibilidad móvil, configuración dinámica de red y un stack completo de monitoreo con Prometheus y Grafana.

## Características
* **Arquitectura Asincrónica:** Utiliza `ESPAsyncWebServer` para soportar múltiples clientes sin bloquear el procesador.
* **Frontend Separado (SPIFFS):** El HTML, CSS y JS se sirven directamente desde la memoria flash del ESP32, manteniendo el código C++ totalmente limpio.
* **Single Page Application (SPA):** Utiliza peticiones AJAX (`fetch`) para actualizar la telemetría cada 2 segundos sin recargar la página.
* **Gráfico Histórico en Vivo:** Integra `Chart.js` para la visualización en tiempo real de los niveles de gas directamente en el navegador.
* **Portal Cautivo:** Usa `WiFiManager` para configurar dinámicamente las credenciales Wi-Fi sin tener que escribirlas en el código.
* **Stack de Monitoreo:** Expone un endpoint nativo `/metrics` para **Prometheus**, e incluye un `docker-compose.yml` para levantar **Grafana** localmente

## Requisitos de Hardware
* **Placa de Desarrollo ESP32**
* **Sensor DHT22** (Temperatura y Humedad) - Conectado al `Pin 4`
* **Sensor MQ-135** (Calidad de Aire / Gas) - Conectado al `Pin 34` (Pin Analógico)
* **LED Integrado** - `Pin 2`

## Preparación del Entorno

Este proyecto se desarrolló sobre Ubuntu 24.04 LTS y está pensado para ser ejecutado en este sistema operativo, aunque el código es compatible con otros sistemas operativos.

### 1. Permisos USB (Linux)
Para permitir que el IDE de Arduino se comunique con el ESP32 a través del puerto USB, debes agregar tu usuario al grupo `dialout`:
```bash
sudo usermod -a -G dialout $USER
```
*(Hacer reboot a entrar para que los cambios surtan efecto).*

### 2. Librerías del IDE de Arduino
Instala las siguientes librerías desde el Gestor de Librerías de Arduino (en Herramientas > Administrar librerías):
* `ESPAsyncWebServer` (por me-no-dev)
* `AsyncTCP` (por me-no-dev)
* `WiFiManager` (por tzapu)
* `DHT sensor library` (por Adafruit)

## Instalación y Despliegue

### Paso 1: Subir el Código C++
1. Abre el archivo `TP1_AsyncServer_Advanced.ino` en tu IDE de Arduino.
2. Selecciona tu placa ESP32 y el puerto COM/USB correcto.
3. Haz clic en **Subir**.

### Paso 2: Subir los Archivos Web (SPIFFS)
La forma estándar de subir la carpeta `data` al ESP32 es utilizando el plugin **ESP32 Sketch Data Upload** para el IDE de Arduino.

1. Instalar el plugin siguiendo las [instrucciones oficiales](https://github.com/me-no-dev/arduino-esp32fs-plugin).
2. En el IDE de Arduino, ve a **Herramientas > ESP32 Sketch Data Upload**.
3. Espera a que termine el proceso.

#### Adenda: Linux Snap / Entornos Especiales
Si utilizas el IDE de Arduino instalado vía **Snap** (común en Ubuntu 24.04), el plugin anterior puede fallar por restricciones de permisos. Para solucionar esto, se incluye un script en Bash en este repositorio que automatiza el proceso:

1. Dale permisos de ejecución al script: `chmod +x subir_web.sh`
2. Hay algunas configuraciones en el script, como el puerto USB y el baudh rate que pueden cambiar.
3. Luego de revisar configuraciones, ejecuta el script: `./subir_web.sh`

*(Nota: Asegúrate de que el Monitor Serie del IDE de Arduino esté CERRADO antes de ejecutar el script, de lo contrario el puerto estará ocupado).*

### Paso 3: Configurar el Wi-Fi
1. Enciende el ESP32.
2. Desde tu celular o computadora, conéctate a la nueva red Wi-Fi abierta llamada **`ESP32_Clima_Config`**.
3. Automáticamente se abrirá un portal cautivo. Selecciona la red Wi-Fi de tu casa e ingresa la contraseña.
4. El ESP32 se reiniciará, se conectará a tu router y mostrará su nueva dirección IP en el Monitor Serie.
5. Ingresa esa IP en tu navegador para ver el panel de control

## Integración con Grafana y Prometheus
Para recolectar datos históricos a largo plazo y crear dashboards avanzados, se incluye un stack de Docker.
1. Abre una terminal en la carpeta del proyecto y ejecuta:
   ```bash
   docker-compose up -d
   ```
2. Entra a Grafana desde tu navegador: `http://localhost:3000`.
3. Inicia sesión con las credenciales `admin` / `admin`.
4. Agrega **Prometheus** como Data Source utilizando la URL `http://prometheus:9090`.
   *(**IMPORTANTE**: NO uses `localhost`, ya que Grafana corre dentro de un contenedor Docker y necesita usar el nombre del servicio para encontrar a Prometheus).*
5. **Importar el Dashboard:**
   - En el menú izquierdo, ve a **Dashboards -> Import**.
   - Haz clic en **"Upload JSON file"** y selecciona el archivo `grafana_dashboard.json` que viene incluido en esta carpeta.
   - Selecciona Prometheus como Data Source y dale a Import.
   - Verás un panel de control de Calidad de Aire en Grafana
