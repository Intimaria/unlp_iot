# TP2: Estación de Monitoreo Ambiental IoT con MQTT

Una estación IoT construida para ESP32. Este proyecto extiende el TP1 incorporando el protocolo **MQTT** como capa de mensajería, reemplazando el servidor web con un pipeline completo de datos: el ESP32 publica métricas cada 5 segundos, **Mosquitto** las recibe, **Node-RED** las procesa y las guarda en **InfluxDB**, y **Grafana** las visualiza.

## Características

* **Protocolo MQTT:** Comunicación ultraliviana ESP32 → broker, ideal para IoT con recursos limitados.
* **Portal Cautivo (WiFiManager):** Configura Wi-Fi y la IP del broker MQTT sin tocar el código — sin credenciales hardcodeadas.
* **Pipeline de datos:** `ESP32 → Mosquitto → Node-RED → InfluxDB → Grafana`.
* **Promediado de sensores analógicos:** 10 lecturas por ciclo para estabilizar el ruido del MQ135 y el LDR.
* **Stack Docker completo:** Un solo `docker-compose up -d` levanta todo el backend.

## Arquitectura

```
ESP32 + Sensores  →  Mosquitto (MQTT)  →  Node-RED  →  InfluxDB  →  Grafana
     (publica JSON)     (broker)        (transforma)  (persiste)   (visualiza)
```

## Hardware Requerido

* **Placa de Desarrollo ESP32**
* **Sensor DHT22** (Temperatura y Humedad) — Pin Digital `4`
* **Sensor MQ-135** (Calidad de Aire / Gas) — Pin Analógico `34`
* **Fotorresistencia LDR** (Luminosidad) — Pin Analógico `35`, con resistencia de 10kΩ a GND

### Conexión del LDR

El LDR requiere un divisor de voltaje con una resistencia fija de **10kΩ**:

```
3.3V ──── LDR ──── Pin 35
                      │
                    10kΩ
                      │
                     GND
```

## Preparación del Entorno

Este proyecto fue desarrollado sobre Ubuntu 24.04 LTS. El código es compatible con otros sistemas operativos, pero algunos pasos de instalación son específicos de Linux.

### 1. Permisos USB (Linux)

Para que el Arduino IDE pueda comunicarse con el ESP32 por USB:

```bash
sudo usermod -a -G dialout $USER
```
*(Reiniciar sesión para que tome efecto. Alternativamente, usar `sudo chmod a+rw /dev/ttyUSB0` para una sesión temporal.)*

### 2. Librerías del Arduino IDE

Instalar desde el **Gestor de Librerías** (Herramientas → Administrar Librerías):

* `PubSubClient` (by Nick O'Leary) — cliente MQTT
* `WiFiManager` (by tzapu) — portal cautivo
* `DHT sensor library` (by Adafruit)



## Instalación y Despliegue

### Paso 1: Levantar el Stack Docker

Desde la carpeta `TP2_MQTT`, ejecutar:

```bash
docker-compose up -d
```

Esto descarga/construye e inicia de forma automática:
- **Mosquitto** (puerto `1883`) — Broker MQTT.
- **Node-RED** (puerto `1880`) — Con el plugin de InfluxDB pre-instalado y el flujo de transformación cargado y desplegado de forma automática.
- **InfluxDB** (puerto `8086`) — Base de datos inicializada (`iot`).
- **Grafana** (puerto `3000`) — Auto-configurado con el Data Source de InfluxDB y el Dashboard pre-diseñado para visualizar las métricas.

### Paso 2: Subir el Código al ESP32

1. Abrí `TP2_MQTT.ino` en el Arduino IDE.
2. Seleccioná la placa **ESP32 Dev Module** y el puerto `/dev/ttyUSB0`.
3. Hacé clic en **Subir**.

### Paso 3: Configurar Wi-Fi y Broker MQTT

Al iniciar por primera vez (o si no hay red guardada), el ESP32 crea una red Wi-Fi abierta llamada **`ESP32_MQTT_Config`**:

1. Conectate a esa red desde tu celular o computadora.
2. Se abre automáticamente un portal cautivo con dos campos:
   - **Tu red Wi-Fi** y contraseña.
   - **IP del broker MQTT** — ingresá la IP de tu computadora en la red local (verificá con `ip addr | grep 192.168`).
3. El ESP32 se reinicia y comienza a publicar datos cada 5 segundos en el topic `sensor/ambiente`.
4. Abrí el Monitor Serie para confirmar que conecta y publica mensajes con el formato:
   ```json
   {"temp": 24.50, "hum": 60.20, "gas": 1200, "luz": 3000}
   ```

> **Nota:** Para forzar una reconfiguración (por ejemplo, si cambió la red o la IP del broker), descomentá `wm.resetSettings();` en el `setup()`, subí el código una vez, y volvé a comentarlo.

## Monitoreo y Diagnóstico del Pipeline

Para entender el flujo de datos y diagnosticar problemas, podés verificar cada etapa del stack:

### 1. Mensajería MQTT (Mosquitto)
Podés subscribirte al broker de forma directa para ver los JSON crudos enviados por el ESP32 en tiempo real:
```bash
docker exec -it iot_mosquitto mosquitto_sub -t 'sensor/ambiente' -v
```
O ver los registros de conexión del broker:
```bash
docker logs iot_mosquitto --tail 20
```

### 2. Pipeline de Transformación (Node-RED)
Abrí **http://localhost:1880** en tu navegador. Encontrarás el flujo `ESP32 Sensor Pipeline` ya importado y ejecutándose. 
- Podés añadir nodos de tipo **`debug`** conectados a las salidas de los distintos nodos para inspeccionar los objetos `msg` en el panel lateral de debug sin alterar la base de datos.

### 3. Persistencia de Datos (InfluxDB)
Consultá de forma directa si las lecturas están guardándose correctamente en la base de datos temporal:
```bash
docker exec -it iot_influxdb influx -database iot -execute "SELECT * FROM ambiente ORDER BY time DESC LIMIT 5"
```

### 4. Visualización Histórica (Grafana)
Abrí **http://localhost:3000** en tu navegador (Usuario/Contraseña: `admin` / `admin`).
- Dirigite a **Dashboards** y abrí el dashboard autogenerado **`ESP32 Weather Station — InfluxDB Historics`**.
- Verás gráficos y paneles de estado actualizados en tiempo real con datos históricos de temperatura, humedad, calidad de aire y luz.

### 5. Dashboard Web Local (SPIFFS & WebSockets)
El ESP32 no solo publica en MQTT; también levanta un servidor web asincrónico que sirve una interfaz web de diseño premium (Glassmorphism dark mode) alojada en su memoria interna SPIFFS:
- **WebSockets en Tiempo Real:** El navegador se conecta directamente al puerto `9001` de Mosquitto por WebSockets, subscribiéndose al topic `sensor/ambiente` para ver las lecturas en vivo con animaciones fluidas sin consultar la base de datos.
- **Control Bidireccional de Actuadores (LED):** Al cambiar el switch en la página, se publica un comando MQTT en el topic `sensor/led/control`. El ESP32 reacciona cambiando el estado de su LED integrado (Pin 2) y confirma el cambio publicando en `sensor/led/state`, lo que sincroniza todas las interfaces abiertas al mismo tiempo.

#### Subir la Web al SPIFFS
Para cargar el sitio web en la memoria flash de tu ESP32:
1. Conectá la placa por USB.
2. Ejecutá el script provisto en la carpeta del proyecto para compilar y flashear automáticamente la carpeta `data/` al bloque SPIFFS correspondiente:
   ```bash
   chmod +x subir_web.sh
   ./subir_web.sh
   ```
3. Una vez subido, abrí **http://<IP_DE_TU_ESP32>** en tu navegador (la IP se imprime en el Monitor Serie al conectarse al Wi-Fi).

## Conclusiones

Al implementar esta estación pudimos observar cómo el protocolo MQTT desacopla completamente al ESP32 del backend. La placa no sabe ni le importa quién consume sus datos: simplemente publica en `sensor/ambiente`. 

Esto permite que convivan en paralelo:
1. Un pipeline clásico de backend persistiendo datos históricamente (`Node-RED → InfluxDB → Grafana`).
2. Clientes interactivos en tiempo real suscriptos directamente al broker (`Dashboard Web del Navegador` usando WebSockets) con comunicación bidireccional (control del LED).

**Mejoras a futuro:**
- Implementar autenticación SSL/TLS y usuarios en Mosquitto.
- Implementar *Deep Sleep* en el ESP32 si fuera alimentado por batería.
- Agregar retención de mensajes MQTT (`retained=true`) para que un nuevo suscriptor reciba el último valor inmediatamente al conectarse.
