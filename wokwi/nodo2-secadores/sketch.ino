/* G1 Pesca - Nodo 2: Secadores (ESP32, MQ-2, DHT22, RGB)
   Simulación académica: gas en % RELATIVO DEL ADC; NO ppm de H2S/NH3.
   MQTT firmado HMAC-SHA256 para telemetría; LED decide LOCALMENTE aun sin red.
   Este sketch NO acepta comandos remotos, evitando órdenes no autenticadas.
*/
#include <Arduino.h>
#include <WiFi.h>
#include <PubSubClient.h>
#include <DHTesp.h>
#include <mbedtls/md.h>
#include <time.h>
#include "config.h"

constexpr int PIN_DHT = 15, PIN_GAS = 34;
constexpr int PIN_R = 25, PIN_G = 26, PIN_B = 27;
constexpr unsigned long READ_MS = 2000, HEARTBEAT_MS = 30000, RETRY_MS = 5000;
// Umbrales de DEMOSTRACIÓN, modificables durante la visita docente:
constexpr float GAS_WARN = 30.0, GAS_ALARM = 60.0;
constexpr float TEMP_WARN = 40.0, TEMP_ALARM = 50.0;

WiFiClient tcp;
PubSubClient mqtt(tcp);
DHTesp dht;
String bootId;
uint32_t seqNumber = 0;
unsigned long lastRead = 0, lastHeartbeat = 0, lastWifiTry = 0, lastMqttTry = 0;
String previousStatus = "";
// Inyección controlada SOLO para pruebas académicas: nunca confundirla con el MQ-2 real.
// En Serial Monitor enviar v (verde 15%), a (amarillo 45%), r (rojo 85%), m (MQ-2 sin inyección).
// Los umbrales GAS_WARN/GAS_ALARM permanecen SIEMPRE en 30% y 60%.
int gasInyectadoPct = -1;
void handleSerialTest() {
  while (Serial.available()) {
    char c = static_cast<char>(Serial.read());
    if (c == 'v' || c == 'V') gasInyectadoPct = 15;
    else if (c == 'a' || c == 'A') gasInyectadoPct = 45;
    else if (c == 'r' || c == 'R') gasInyectadoPct = 85;
    else if (c == 'm' || c == 'M') gasInyectadoPct = -1;
    else continue;
    if (gasInyectadoPct < 0) Serial.println("[PRUEBA] Entrada MQ-2 simulada restaurada; sin inyeccion.");
    else Serial.printf("[PRUEBA] GAS INYECTADO = %d%% (NO es una lectura fisica de MQ-2)\n", gasInyectadoPct);
    lastRead = 0; // fuerza nueva lectura en el siguiente ciclo
  }
}

String toHex(const uint8_t *bytes, size_t len) {
  const char h[] = "0123456789abcdef";
  String out;
  out.reserve(len * 2);
  for (size_t i=0; i<len; ++i) { out += h[bytes[i] >> 4]; out += h[bytes[i] & 15]; }
  return out;
}

String hmacSha256(const String& message) {
  unsigned char output[32];
  const mbedtls_md_info_t *md = mbedtls_md_info_from_type(MBEDTLS_MD_SHA256);
  if (!md || mbedtls_md_hmac(md, reinterpret_cast<const unsigned char*>(HMAC_KEY),
           strlen(HMAC_KEY), reinterpret_cast<const unsigned char*>(message.c_str()),
           message.length(), output) != 0) return "";
  return toHex(output, sizeof(output));
}

// Debe coincidir byte a byte con backend/lib/protocol.js
String canonical(const String& boot, long ts, uint32_t seq, float t, float h,
                 int raw, float gas, const String& quality, const String& status) {
  return "v1|nodo2|" + boot + "|" + String(ts) + "|" + String(seq) + "|" +
    String(t, 2) + "|" + String(h, 2) + "|" + String(raw) + "|" +
    String(gas, 2) + "|gas_pct_simulado|" + quality + "|" + status;
}

void setRGB(bool red, bool green, bool blue) {
  digitalWrite(PIN_R, red ? HIGH : LOW);
  digitalWrite(PIN_G, green ? HIGH : LOW);
  digitalWrite(PIN_B, blue ? HIGH : LOW);
}

String decide(float temp, float gas, bool sensorOk) {
  if (!sensorOk) return "FALLA_SENSOR";
  if (gas >= GAS_ALARM || temp >= TEMP_ALARM) return "ALERTA";
  if (gas >= GAS_WARN || temp >= TEMP_WARN) return "ADVERTENCIA";
  return "NORMAL";
}

void applyEdge(const String& status) {
  if (status == "NORMAL") setRGB(false, true, false);
  else if (status == "ADVERTENCIA") setRGB(true, true, false);
  else setRGB(true, false, false); // alerta o error de sensor: señal roja
}

void ensureNetwork() {
  unsigned long now = millis();
  if (WiFi.status() != WL_CONNECTED) {
    if (now - lastWifiTry >= RETRY_MS || lastWifiTry == 0) {
      lastWifiTry = now;
      WiFi.begin("Wokwi-GUEST", "", 6);
      Serial.println("[RED] Intentando conectar WiFi...");
    }
    return;
  }
  if (mqtt.connected()) return;
  if (now - lastMqttTry < RETRY_MS && lastMqttTry != 0) return;
  lastMqttTry = now;
  String clientId = "g1-secadores-" + bootId;
  bool ok = mqtt.connect(clientId.c_str(), nullptr, nullptr,
                         MQTT_ROOT "/estado", 1, true, "offline");
  if (ok) {
    mqtt.publish(MQTT_ROOT "/estado", "online", true);
    lastHeartbeat = now;
    Serial.println("[MQTT] Conectado. LWT=offline, estado=online");
  } else {
    Serial.printf("[MQTT] Reintento: rc=%d\n", mqtt.state());
  }
}

void publishMeasurement(const String& topic, float t, float h, int raw,
                        float gas, const String& quality, const String& status) {
  if (!mqtt.connected()) return;
  time_t current = time(nullptr);
  if (current < 1700000000) {
    Serial.println("[MQTT] Hora UTC pendiente: se conserva actuación local");
    return;
  }
  long ts = static_cast<long>(current);
  uint32_t seq = ++seqNumber;
  String material = canonical(bootId, ts, seq, t, h, raw, gas, quality, status);
  String signature = hmacSha256(material);
  if (signature.length() != 64) return;
  String payload = "{\"id\":\"nodo2\",\"boot\":\"" + bootId +
    "\",\"ts\":" + String(ts) + ",\"seq\":" + String(seq) +
    ",\"temperatura\":" + String(t,2) + ",\"humedad\":" + String(h,2) +
    ",\"gas_raw\":" + String(raw) + ",\"gas\":" + String(gas,2) +
    ",\"unidad\":\"gas_pct_simulado\",\"calidad\":\"" + quality +
    "\",\"estado\":\"" + status + "\",\"firma\":\"" + signature + "\"}";
  bool sent = mqtt.publish(topic.c_str(), payload.c_str(), false);
  Serial.printf("[MQTT] %s seq=%lu %s\n", topic.c_str(), (unsigned long)seq, sent ? "OK":"ERROR");
}

void setup() {
  Serial.begin(115200);
  pinMode(PIN_R, OUTPUT); pinMode(PIN_G, OUTPUT); pinMode(PIN_B, OUTPUT);
  pinMode(PIN_GAS, INPUT);
  setRGB(true,false,false); // señal de arranque / hasta primera lectura válida
  dht.setup(PIN_DHT, DHTesp::DHT22);
  analogReadResolution(12);
  analogSetPinAttenuation(PIN_GAS, ADC_11db);
  bootId = String(esp_random(), HEX) + String(esp_random(), HEX);
  WiFi.mode(WIFI_STA);
  mqtt.setServer(MQTT_BROKER, MQTT_PORT);
  mqtt.setBufferSize(768);
  mqtt.setKeepAlive(30);
  configTime(0, 0, "pool.ntp.org", "time.google.com");
  Serial.println("Nodo 2 Secadores - accion EDGE y MQTT HMAC");
  Serial.println("[PRUEBA] Serial: v=verde(15%), a=amarillo(45%), r=rojo(85%), m=MQ-2.");
}

void loop() {
  handleSerialTest();
  unsigned long now = millis();
  // Primero se decide localmente; Wi-Fi / MQTT nunca son condición de alarma.
  if (lastRead == 0 || now - lastRead >= READ_MS) {
    lastRead = now;
    TempAndHumidity dh = dht.getTempAndHumidity();
    bool ok = isfinite(dh.temperature) && isfinite(dh.humidity);
    float t = ok ? dh.temperature : -99.0f;
    float h = ok ? dh.humidity : -99.0f;
    int rawMQ2 = analogRead(PIN_GAS);
    int raw = gasInyectadoPct < 0 ? rawMQ2 : (gasInyectadoPct * 4095 + 50) / 100;
    float gas = raw * 100.0f / 4095.0f; // índice relativo, NO ppm
    String quality = !ok ? "SENSOR_ERROR" : gasInyectadoPct >= 0 ? "PRUEBA_INYECTADA" : "OK";
    String status = decide(t, gas, ok);
    applyEdge(status);
    Serial.printf("[EDGE] Temp=%.2f C | Hum=%.2f %% | Gas ADC usado=%d (%.2f %%) | %s | calidad=%s | ADC MQ2 simulado=%d\n",
                  t,h,raw,gas,status.c_str(),quality.c_str(),rawMQ2);
    publishMeasurement(MQTT_ROOT "/datos", t,h,raw,gas,quality,status);
    if (status != previousStatus && status != "NORMAL") {
      publishMeasurement(MQTT_ROOT "/alerta", t,h,raw,gas,quality,status);
    }
    previousStatus = status;
  }
  ensureNetwork();
  if (mqtt.connected()) {
    mqtt.loop();
    if (now - lastHeartbeat >= HEARTBEAT_MS) {
      lastHeartbeat = now;
      mqtt.publish(MQTT_ROOT "/estado", "online", true);
    }
  }
  delay(10);
}
