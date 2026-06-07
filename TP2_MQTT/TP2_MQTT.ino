#include <WiFi.h>
#include <WiFiManager.h>
#include <PubSubClient.h>
#include <DHT.h>
#include <Preferences.h>
#include <SPIFFS.h>
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <math.h>

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
// CALIBRACIÓN MQ135 — MÚLTIPLES GASES (ppm)
// ==========================================
// Resistencia de carga en la placa breakout del MQ135 (típicamente 10 kΩ o 20 kΩ)
#define MQ135_RL_KOHM   10.0f   // kΩ — verificar en tu módulo (silk "RL")

// Coeficientes de las curvas del datasheet (gráfico Rs/R0 vs ppm, escala log-log):
//   log(Rs/R0) = A * log(ppm) + B
// A = pendiente, B = ordenada al origen

// CO2 (Calculados a partir de los puntos de la figura)
#define MQ135_CO2_A    -0.3679f
#define MQ135_CO2_B     0.8819f

// Monóxido de Carbono (CO) - Valores aproximados del datasheet
#define MQ135_CO_A     -0.3396f
#define MQ135_CO_B      0.6521f

// Alcohol - Valores aproximados del datasheet
#define MQ135_ALCOHOL_A -0.3154f
#define MQ135_ALCOHOL_B  0.7266f

// Amoníaco (NH4) - Valores aproximados del datasheet
#define MQ135_NH4_A    -0.4103f
#define MQ135_NH4_B     0.8384f

// Tolueno - Valores aproximados del datasheet
#define MQ135_TOLUENO_A -0.3452f
#define MQ135_TOLUENO_B  0.7915f

// R0: resistencia del sensor en aire limpio (se mide en calibrateR0()).
// En aire limpio la concentración de CO₂ es ~400 ppm → Rs/R0 ≈ 0.976
// (calculado con la ecuación inversa: R0 = Rs / 10^(A*log(400)+B))
// Se puede sobreescribir desde Preferences si el usuario la guardó antes.
float mq135_R0 = 10.0f;  // valor inicial; se recalcula en calibrateR0()

// ---------- helpers ----------

/**
 * Lee el ADC del MQ135 (promedio de N muestras) y devuelve Rs en kΩ.
 *   Vout = (ADC / 4095) * 3.3 V
 *   Rs   = RL * (Vcc - Vout) / Vout  →  Rs = RL * (4095 - ADC) / ADC
 * (asume divisor resistivo típico de los breakouts)
 */
float mq135ReadRs(int samples = 10) {
  long suma = 0;
  for (int i = 0; i < samples; i++) {
    suma += analogRead(MQ135_PIN);
    delay(10);
  }
  float adc = (float)(suma / samples);
  if (adc < 1) adc = 1;           // evitar división por cero
  return MQ135_RL_KOHM * (4095.0f - adc) / adc;
}

/**
 * Calibra R0 asumiendo que el aire está "limpio" (≈400 ppm CO₂).
 * Rs_clean / R0 = 10^(A * log10(400) + B)  →  R0 = Rs_clean / ratio
 */
float calibrateR0() {
  float ratio_clean_air = pow(10.0f, MQ135_CO2_A * log10(400.0f) + MQ135_CO2_B);
  float rs = mq135ReadRs(50);   // promedio largo para mayor precisión
  float r0 = rs / ratio_clean_air;
  
  // Sanity check: si el sensor está frío o leyendo basura, R0 dará valores extremos.
  // Algunos MQ135 tienen un R0 de hasta cientos de kOhm.
  if (r0 < 1.0f || r0 > 1000.0f) {
    Serial.println("R0 fuera de rango (¿sensor frío?). Usando valor por defecto (30 kOhm).");
    return 30.0f;
  }
  return r0;
}

/**
 * Convierte Rs medido a ppm usando la ecuación logarítmica para cualquier gas:
 *   log10(Rs/R0) = A * log10(ppm) + B
 *   ppm = 10 ^ ((log10(Rs/R0) - B) / A)
 */
float mq135ToGasPPM(float rs, float a, float b) {
  float ratio = rs / mq135_R0;
  if (ratio <= 0) ratio = 0.001f;
  float ppm = pow(10.0f, (log10(ratio) - b) / a);
  return ppm;
}

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

// --- Variables para Promedio Móvil Exponencial (EWMA, ~15 seg) ---
float ewma_t = 0;
float ewma_h = 0;
float ewma_luz = 0;
float ewma_co2 = 0;
float ewma_co = 0;
float ewma_alcohol = 0;
float ewma_nh4 = 0;
float ewma_tolueno = 0;
bool ewma_initialized = false;
const float EWMA_ALPHA = 0.25f; // dt / (tau + dt) = 5 / (15 + 5)

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

  // --- Calibrar sensor MQ135 ---
  // Se hace AL FINAL del setup para que la radio WiFi ya esté encendida
  // y el regulador de voltaje (3.3V) esté estable. Si no, el ADC lee mal.
  Serial.println("Calentando y calibrando sensor de gases (R0)... Por favor, asegúrese de estar en aire limpio.");
  delay(3000); // Dar tiempo adicional al heater
  mq135_R0 = calibrateR0();
  Serial.print("R0 calibrado estable: ");
  Serial.print(mq135_R0);
  Serial.println(" kOhm");
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
      // Promedio de 10 lecturas analógicas para reducir ruido del LDR
      long sumaLuz = 0;
      for (int i = 0; i < 10; i++) {
        sumaLuz += analogRead(LDR_PIN);
        delay(10);
      }
      int luzValue  = sumaLuz / 10;

      // Calcular PPM RAW para los diferentes gases a partir de una única lectura de Rs
      float rs_current = mq135ReadRs(10);
      float co2_raw     = mq135ToGasPPM(rs_current, MQ135_CO2_A, MQ135_CO2_B);
      
      // El MQ135 no puede distinguir gases. El CO2 natural (~400ppm) baja la resistencia,
      // haciendo que las otras fórmulas "vean" concentraciones altas irreales en aire limpio.
      // Restamos el valor teórico en aire limpio (ratio = 0.84) para "encerar" los demás gases.
      float co_raw      = mq135ToGasPPM(rs_current, MQ135_CO_A, MQ135_CO_B) - 138.0f;
      float alcohol_raw = mq135ToGasPPM(rs_current, MQ135_ALCOHOL_A, MQ135_ALCOHOL_B) - 347.0f;
      float nh4_raw     = mq135ToGasPPM(rs_current, MQ135_NH4_A, MQ135_NH4_B) - 168.0f;
      float tolueno_raw = mq135ToGasPPM(rs_current, MQ135_TOLUENO_A, MQ135_TOLUENO_B) - 323.0f;

      // Clamping para evitar valores negativos
      if (co2_raw < 400.0f) co2_raw = 400.0f;
      if (co_raw < 0.0f) co_raw = 0.0f;
      if (alcohol_raw < 0.0f) alcohol_raw = 0.0f;
      if (nh4_raw < 0.0f) nh4_raw = 0.0f;
      if (tolueno_raw < 0.0f) tolueno_raw = 0.0f;

      // Aplicar filtro EWMA (promedio de los últimos ~15 segundos)
      if (!ewma_initialized) {
        ewma_t = t;
        ewma_h = h;
        ewma_luz = luzValue;
        ewma_co2 = co2_raw;
        ewma_co = co_raw;
        ewma_alcohol = alcohol_raw;
        ewma_nh4 = nh4_raw;
        ewma_tolueno = tolueno_raw;
        ewma_initialized = true;
      } else {
        ewma_t = EWMA_ALPHA * t + (1.0f - EWMA_ALPHA) * ewma_t;
        ewma_h = EWMA_ALPHA * h + (1.0f - EWMA_ALPHA) * ewma_h;
        ewma_luz = EWMA_ALPHA * luzValue + (1.0f - EWMA_ALPHA) * ewma_luz;
        ewma_co2 = EWMA_ALPHA * co2_raw + (1.0f - EWMA_ALPHA) * ewma_co2;
        ewma_co = EWMA_ALPHA * co_raw + (1.0f - EWMA_ALPHA) * ewma_co;
        ewma_alcohol = EWMA_ALPHA * alcohol_raw + (1.0f - EWMA_ALPHA) * ewma_alcohol;
        ewma_nh4 = EWMA_ALPHA * nh4_raw + (1.0f - EWMA_ALPHA) * ewma_nh4;
        ewma_tolueno = EWMA_ALPHA * tolueno_raw + (1.0f - EWMA_ALPHA) * ewma_tolueno;
      }

      // JSON ampliado
      char msg[256];
      snprintf(msg, sizeof(msg),
               "{\"temp\": %.2f, \"hum\": %.2f, \"co2\": %.2f, \"co\": %.2f, \"alcohol\": %.2f, \"nh4\": %.2f, \"tolueno\": %.2f, \"luz\": %d}",
               ewma_t, ewma_h, ewma_co2, ewma_co, ewma_alcohol, ewma_nh4, ewma_tolueno, (int)ewma_luz);

      Serial.print("Publicando (Promediado): ");
      Serial.println(msg);
      client.publish("sensor/ambiente", msg);
    }

    lastTime = millis();
  }
}
