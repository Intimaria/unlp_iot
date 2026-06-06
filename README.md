# Internet de las Cosas (IoT) - UNLP

**Facultad de Informática | Universidad Nacional de La Plata**

Repositorio central con los trabajos prácticos desarrollados durante la cursada de Internet de las Cosas. Los proyectos están enfocados en la programación de microcontroladores (ESP32), lectura de sensores, y la implementación de arquitecturas y protocolos de comunicación para IoT.

---

## 📂 Índice de Trabajos Prácticos

### [TP1: Servidor Web Asincrónico Avanzado](./TP1_AsyncServer_Advanced/)
Implementación de una estación de monitoreo ambiental basada en arquitectura de "Polling" (Consulta).
* **Conceptos clave:** 
  * Servidor web asincrónico (ESPAsyncWebServer) y API REST.
  * Frontend SPA (Single Page Application) servido desde la memoria flash (SPIFFS).
  * Gráficos en tiempo real en la web usando Chart.js y AJAX (`fetch`).
  * Endpoint nativo `/metrics` para **Prometheus**.
  * Stack Docker con **Prometheus, InfluxDB y Grafana** para visualización de historial.

### [TP2: Estación de Monitoreo Ambiental con MQTT](./TP2_MQTT/)
Evolución del TP1 hacia una arquitectura profesional IoT desacoplada utilizando el protocolo MQTT y un pipeline de datos completo en contenedores Docker.
* **Conceptos clave:** 
  * Publicación/Suscripción con MQTT (Mosquitto).
  * Auto-configuración de red con WiFiManager (Portal Cautivo).
  * Transformación y enrutamiento de datos con Node-RED.
  * Persistencia de series temporales en InfluxDB.
  * Visualización histórica mediante Dashboards en Grafana.
  * Integración de comandos remotos mediante un Bot de Telegram.

---

## 🛠️ Tecnologías y Herramientas Utilizadas

* **Hardware:** ESP32, Sensores DHT22 (Temperatura/Humedad), MQ-135 (Gases), fotorresistencias (LDR).
* **Software:** Arduino IDE / C++, Docker & Docker Compose.
* **Protocolos:** HTTP, WebSockets, MQTT.

> **Nota:** Cada carpeta contiene su propio `README.md` con instrucciones detalladas de instalación, dependencias y esquemas de conexión de hardware específicos para cada trabajo práctico.
