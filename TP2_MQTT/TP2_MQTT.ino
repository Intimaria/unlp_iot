#include <WiFi.h>
#include <WiFiManager.h>
#include <PubSubClient.h>
#include <DHT.h>
#include <Preferences.h>
#include <SPIFFS.h>
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>

// ==========================================
// CONFIGURACIÓN DE SENSORES Y ACTUADORES
// ==========================================
#define DHTPIN    4    // Pin digital del DHT22
#define DHTTYPE   DHT22
#define MQ135_PIN 34   // Pin analógico — sensor de gases
#define LDR_PIN   35   // Pin analógico — fotorresistencia (LDR + 10kΩ a GND)
#define LED_PIN   2    // Pin digital del LED integrado

DHT dht(DHTPIN, DHTTYPE);

// ==========================================
// MQTT — IP del broker configurada en el portal
// ==========================================
// Buffer donde WiFiManager guarda la IP que el usuario ingresa en el portal
char mqtt_server[40] = "192.168.1.39"; // valor por defecto (se sobreescribe desde el portal)

WiFiClient espClient;
PubSubClient client(espClient);
AsyncWebServer server(80); // Servidor web en el puerto 80

unsigned long lastTime = 0;
const unsigned long timerDelay = 5000; // publicar cada 5 segundos

// ==========================================
// CALLBACK MQTT — CONTROL DE LED (BIDIRECCIONAL)
// ==========================================
void callback(char* topic, byte* payload, unsigned int length) {
  Serial.print("Mensaje recibido [");
  Serial.print(topic);
  Serial.print("]: ");
  
  String msg = "";
  for (int i = 0; i < length; i++) {
    msg += (char)payload[i];
  }
  Serial.println(msg);

  if (strcmp(topic, "sensor/led/control") == 0) {
    if (msg == "1") {
      digitalWrite(LED_PIN, HIGH);
      Serial.println("LED Encendido");
      client.publish("sensor/led/state", "1", true);
    } else if (msg == "0") {
      digitalWrite(LED_PIN, LOW);
      Serial.println("LED Apagado");
      client.publish("sensor/led/state", "0", true);
    }
  }
}

// ==========================================
// RECONEXIÓN AL BROKER MQTT
// ==========================================
void reconnect() {
  while (!client.connected()) {
    Serial.print("Conectando a Mosquitto...");
    String clientId = "ESP32Client-" + String(random(0xffff), HEX);

    if (client.connect(clientId.c_str())) {
      Serial.println(" ¡Conectado!");
      client.subscribe("sensor/led/control");
      // Publicar estado actual del LED para sincronizar el frontend
      String estado = String(digitalRead(LED_PIN));
      client.publish("sensor/led/state", estado.c_str(), true);
    } else {
      Serial.print(" Falló (rc=");
      Serial.print(client.state());
      Serial.println("). Reintentando en 5s...");
      delay(5000);
    }
  }
}

// ==========================================
// SETUP
// ==========================================
void setup() {
  Serial.begin(115200);
  dht.begin();

  // --- Cargar IP del broker guardada en flash ---
  Preferences prefs;
  prefs.begin("mqtt-config", true); // true = read-only
  String saved_ip = prefs.getString("broker_ip", "192.168.1.39");
  prefs.end();
  saved_ip.toCharArray(mqtt_server, sizeof(mqtt_server));
  Serial.print("IP broker cargada desde flash: ");
  Serial.println(mqtt_server);

  // --- WiFiManager con parámetro personalizado para el broker MQTT ---
  WiFiManager wm;

  // Parámetro extra que aparece en el portal cautivo (pre-cargado con el valor guardado)
  WiFiManagerParameter param_mqtt("mqtt", "IP del broker MQTT", mqtt_server, 40);
  wm.addParameter(&param_mqtt);

  // wm.resetSettings(); // Descomentar UNA VEZ para forzar nuevo portal

  bool conectado = wm.autoConnect("ESP32_MQTT_Config");
  if (!conectado) {
    Serial.println("Error al conectar. Reiniciando...");
    delay(3000);
    ESP.restart();
  }

  // Si el usuario ingresó una IP nueva en el portal, guardarla en flash
  const char* nueva_ip = param_mqtt.getValue();
  if (strlen(nueva_ip) > 0 && strcmp(nueva_ip, mqtt_server) != 0) {
    strncpy(mqtt_server, nueva_ip, sizeof(mqtt_server));
    prefs.begin("mqtt-config", false); // false = read-write
    prefs.putString("broker_ip", mqtt_server);
    prefs.end();
    Serial.println("Nueva IP del broker guardada en flash.");
  }

  Serial.println("\n¡WiFi conectado!");
  Serial.print("IP del ESP32: ");
  Serial.println(WiFi.localIP());
  Serial.print("Broker MQTT: ");
  Serial.println(mqtt_server);

  // --- Configurar Pin del LED ---
  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, LOW); // Arranca apagado

  client.setServer(mqtt_server, 1883);
  client.setCallback(callback); // Registrar callback de recepción MQTT

  // --- Iniciar SPIFFS ---
  if (!SPIFFS.begin(true)) {
    Serial.println("Error al montar SPIFFS");
  } else {
    Serial.println("SPIFFS montado correctamente.");
  }

  // --- Servidor Web Asincrónico ---
  // Sirve los archivos estáticos de la carpeta /data
  server.serveStatic("/", SPIFFS, "/").setDefaultFile("index.html");

  // Endpoint REST para entregar la IP del broker al frontend en el navegador
  server.on("/api/config", HTTP_GET, [](AsyncWebServerRequest *request){
    String json = "{\"broker_ip\":\"" + String(mqtt_server) + "\"}";
    request->send(200, "application/json", json);
  });

  server.begin();
  Serial.println("Servidor Web Asincrónico iniciado en puerto 80");
}

// ==========================================
// LOOP
// ==========================================
void loop() {
  if (!client.connected()) {
    reconnect();
  }
  client.loop();

  if ((millis() - lastTime) > timerDelay) {
    float t = dht.readTemperature();
    float h = dht.readHumidity();

    if (isnan(t) || isnan(h)) {
      Serial.println("Error leyendo el DHT22.");
    } else {
      // Promedio de 10 lecturas analógicas para reducir ruido
      long sumaGas = 0, sumaLuz = 0;
      for (int i = 0; i < 10; i++) {
        sumaGas += analogRead(MQ135_PIN);
        sumaLuz += analogRead(LDR_PIN);
        delay(10);
      }
      int gasValue  = sumaGas / 10;
      int luzValue  = sumaLuz / 10;

      // JSON: {"temp": 24.50, "hum": 60.20, "gas": 1200, "luz": 3000}
      char msg[128];
      snprintf(msg, sizeof(msg),
               "{\"temp\": %.2f, \"hum\": %.2f, \"gas\": %d, \"luz\": %d}",
               t, h, gasValue, luzValue);

      Serial.print("Publicando: ");
      Serial.println(msg);
      client.publish("sensor/ambiente", msg);
    }

    lastTime = millis();
  }
}
