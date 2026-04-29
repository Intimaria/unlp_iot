#include <WiFi.h>
#include <WiFiManager.h> // Librería para el portal cautivo
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <DHT.h>
#include <SPIFFS.h>

// Las credenciales WiFi se configuran dinámicamente desde el portal de WiFiManager.
// Pines
#define LED_PIN 2       // Pin del LED integrado
#define DHTPIN 4        // Pin del sensor DHT (basado en tu DHT22_basico)
#define DHTTYPE DHT22   // Tipo de sensor
#define MQ135_PIN 34    // Pin analógico para el sensor MQ-135

DHT dht(DHTPIN, DHTTYPE);
AsyncWebServer server(80); // Servidor web en el puerto 80

// Variables globales para guardar la última lectura
float t = 0.0;
float h = 0.0;
int gasValue = 0; // Almacena el valor crudo del ADC (0 - 4095)

// Variables para no bloquear el loop() usando millis()
unsigned long lastTime = 0;
unsigned long timerDelay = 5000; // Leer sensor cada 5 segundos

// NOTA: El HTML, CSS y Javascript fueron separados del código C++.
// Ahora se encuentran físicamente en el archivo "data/index.html" 
// y son servidos usando el sistema de archivos SPIFFS.

void setup() {
  Serial.begin(115200);
  
  // Configurar LED y Sensor
  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, LOW); // LED arranca apagado
  dht.begin();

  // Iniciar SPIFFS
  if(!SPIFFS.begin(true)){
    Serial.println("Error al montar SPIFFS");
    return;
  }

  // Conectar a WiFi usando WiFiManager
  Serial.println("Iniciando WiFiManager...");
  WiFiManager wm;
  
  // Descomenta la siguiente línea UNA VEZ si necesitas borrar la red Wi-Fi guardada y probar el portal de nuevo:
  // wm.resetSettings();
  
  // Intenta conectarse a la red guardada. Si no puede, crea una red WiFi abierta llamada "ESP32_Clima_Config"
  bool res = wm.autoConnect("ESP32_Clima_Config"); 
  
  if(!res) {
    Serial.println("Error al conectar (timeout). Reiniciando placa...");
    delay(3000);
    ESP.restart();
  } 
  
  Serial.println("\n¡WiFi conectado exitosamente!");
  Serial.print("IP asignada: ");
  Serial.println(WiFi.localIP());

  // === RUTAS DEL SERVIDOR WEB ASINCRÓNICO ===
  
  // Ruta principal: Sirve el archivo estático directamente desde SPIFFS
  server.serveStatic("/", SPIFFS, "/").setDefaultFile("index.html");

  // Ruta para Alternar (Toggle) LED (ahora sin redirección)
  server.on("/toggle", HTTP_GET, [](AsyncWebServerRequest *request){
    int estadoActual = digitalRead(LED_PIN);
    digitalWrite(LED_PIN, !estadoActual);
    request->send(200, "text/plain", "OK"); 
  });

  // REST API Endpoint: Devuelve todos los datos en formato JSON
  server.on("/api/metrics", HTTP_GET, [](AsyncWebServerRequest *request){
    String json = "{";
    json += "\"t\":\"" + String(t, 1) + "\",";
    json += "\"h\":\"" + String(h, 1) + "\",";
    json += "\"gas\":\"" + String(gasValue) + "\",";
    json += "\"led\":" + String(digitalRead(LED_PIN));
    json += "}";
    request->send(200, "application/json", json);
  });

  // Prometheus Metrics Endpoint: Exporta los datos en formato nativo para Prometheus
  server.on("/metrics", HTTP_GET, [](AsyncWebServerRequest *request){
    String prom = "# HELP esp32_temperature Temperatura en Celsius\n";
    prom += "# TYPE esp32_temperature gauge\n";
    prom += "esp32_temperature " + String(t, 1) + "\n";
    
    prom += "# HELP esp32_humidity Humedad relativa\n";
    prom += "# TYPE esp32_humidity gauge\n";
    prom += "esp32_humidity " + String(h, 1) + "\n";
    
    prom += "# HELP esp32_gas Nivel crudo de gas MQ135\n";
    prom += "# TYPE esp32_gas gauge\n";
    prom += "esp32_gas " + String(gasValue) + "\n";
    
    prom += "# HELP esp32_led_status Estado del LED (1=encendido, 0=apagado)\n";
    prom += "# TYPE esp32_led_status gauge\n";
    prom += "esp32_led_status " + String(digitalRead(LED_PIN)) + "\n";
    
    request->send(200, "text/plain", prom);
  });

  // Iniciar servidor web
  server.begin();
  Serial.println("Servidor iniciado en segundo plano");
}

void loop() {
  // A diferencia del server sincrónico, NO hay que revisar "client.connected()"
  // El loop queda exclusivamente para la lógica de los sensores (no bloqueante)
  
  if ((millis() - lastTime) > timerDelay) {
    float newT = dht.readTemperature();
    float newH = dht.readHumidity();

    // Si falló la lectura no actualizamos las variables globales
    if (isnan(newT) || isnan(newH)) {
      Serial.println("Error al leer el sensor DHT");
    } else {
      t = newT;
      h = newH;
    }
    
    // Leemos varias veces el sensor analógico para filtrar el ruido (evitar ceros)
    long sumaGas = 0;
    int lecturasValidas = 0;
    for(int i = 0; i < 10; i++) {
      int lectura = analogRead(MQ135_PIN);
      if(lectura > 10) { // Ignoramos los valores 0 o extremadamente bajos
        sumaGas += lectura;
        lecturasValidas++;
      }
      delay(10); // Pequeña pausa entre lecturas
    }
    
    if (lecturasValidas > 0) {
      gasValue = sumaGas / lecturasValidas;
    } 

    Serial.print("Leído -> T: "); Serial.print(t); 
    Serial.print("°C  -  H: "); Serial.print(h); 
    Serial.print("%  -  Gas: "); Serial.println(gasValue);
    lastTime = millis();
  }
}
