
#include <Arduino.h>
#include <WiFi.h>
#include <PubSubClient.h>
#include <SPI.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_ILI9341.h>
#include <Adafruit_NeoPixel.h>
#include <RTClib.h>

// G1 PESCA - NODO 5 - GATEWAY FOG
// Fase 2: WiFi + MQTT (sin órdenes ni HMAC aún)

// Conexiones existentes
#define TFT_CS  5
#define TFT_DC  27
#define TFT_RST 26
#define BTN_PIN 25
#define LED_PIN 32
#define I2C_SDA 21
#define I2C_SCL 22

// WiFi virtual de Wokwi
const char* WIFI_SSID = "Wokwi-GUEST";
const char* WIFI_PASS = "";

// Broker del laboratorio
const char* MQTT_HOST = "broker.hivemq.com";
const int MQTT_PORT = 1883;

const char* TOPIC_ESTADO = "upc/g1/nodo5/estado";

WiFiClient wifiClient;
PubSubClient mqtt(wifiClient);

Adafruit_ILI9341 tft(TFT_CS, TFT_DC, TFT_RST);
Adafruit_NeoPixel pixel(1, LED_PIN, NEO_GRB + NEO_KHZ800);
RTC_DS1307 rtc;

enum Estado {
  NORMAL,
  ALERTA,
  CRITICO
};

Estado estado = NORMAL;

bool rtcDisponible = false;
bool reconocida = false;
bool botonAnterior = HIGH;
bool botonEstable = HIGH;

unsigned long tiempoBoton = 0;
unsigned long ultimaHora = 0;
unsigned long ultimoHeartbeat = 0;
unsigned long ultimoIntentoMQTT = 0;
unsigned long ultimoIntentoWiFi = 0;

unsigned long mensajesRecibidos = 0;

// ----- NEOPIXEL -----

void actualizarLED() {
  uint32_t color;

  if (estado == NORMAL) {
    color = pixel.Color(0, 150, 0);
  } else if (estado == ALERTA) {
    color = pixel.Color(150, 100, 0);
  } else {
    color = pixel.Color(150, 0, 0);
  }

  pixel.setPixelColor(0, color);
  pixel.show();
}

// ----- PANTALLA -----

void actualizarHora() {
  tft.fillRect(10, 180, 300, 18, ILI9341_BLACK);
  tft.setTextSize(1);
  tft.setTextColor(ILI9341_WHITE);
  tft.setCursor(12, 184);

  if (rtcDisponible) {
    DateTime ahora = rtc.now();
    char fecha[30];

    snprintf(
      fecha, sizeof(fecha),
      "%02d/%02d/%04d %02d:%02d:%02d",
      ahora.day(), ahora.month(), ahora.year(),
      ahora.hour(), ahora.minute(), ahora.second()
    );

    tft.print(fecha);
  } else {
    tft.print("RTC NO DETECTADO");
  }
}

void actualizarPantalla() {
  tft.fillScreen(ILI9341_BLACK);

  tft.fillRect(0, 0, 320, 38, ILI9341_NAVY);
  tft.setTextColor(ILI9341_WHITE);
  tft.setTextSize(2);
  tft.setCursor(12, 10);
  tft.print("G1 PESCA");

  tft.setTextSize(1);
  tft.setCursor(12, 48);
  tft.print("NODO 5 - GATEWAY FOG");

  tft.setTextSize(2);
  tft.setCursor(12, 78);

  if (estado == NORMAL) {
    tft.setTextColor(ILI9341_GREEN);
    tft.print("NORMAL");
  } else if (estado == ALERTA) {
    tft.setTextColor(ILI9341_YELLOW);
    tft.print("ALERTA");
  } else {
    tft.setTextColor(ILI9341_RED);
    tft.print("CRITICO");
  }

  tft.setTextSize(1);
  tft.setTextColor(ILI9341_WHITE);
  tft.setCursor(12, 118);

  tft.print("WiFi: ");
  tft.println(
    WiFi.status() == WL_CONNECTED
      ? "CONECTADO" : "DESCONECTADO"
  );

  tft.setCursor(12, 136);
  tft.print("MQTT: ");
  tft.println(
    mqtt.connected()
      ? "CONECTADO" : "DESCONECTADO"
  );

  tft.setCursor(12, 158);
  tft.print("Mensajes recibidos: ");
  tft.println(mensajesRecibidos);

  actualizarHora();

  tft.setCursor(12, 207);
  tft.print(
    reconocida
      ? "Alerta reconocida"
      : "Boton: reconocer alerta"
  );
}

// ----- ESTADOS LOCALES -----

void cambiarEstado(Estado nuevo) {
  estado = nuevo;
  reconocida = false;

  actualizarLED();
  actualizarPantalla();

  Serial.println("Estado de prueba actualizado");
}

// ----- RECEPCION MQTT -----

void recibirMQTT(
  char* topic,
  byte* payload,
  unsigned int length
) {
  String mensaje = "";

  for (unsigned int i = 0; i < length && i < 350; i++) {
    mensaje += (char)payload[i];
  }

  Serial.println("-----------------------");
  Serial.print("TOPIC: ");
  Serial.println(topic);
  Serial.print("MENSAJE: ");
  Serial.println(mensaje);
  Serial.println("NO VERIFICADO - SOLO RECEPCION");

  // No contamos nuestro propio heartbeat.
  if (String(topic) == TOPIC_ESTADO) {
    return;
  }

  mensajesRecibidos++;
  actualizarPantalla();

  // En esta fase no actuamos ni aceptamos comandos.
  // HMAC, validación y lógica Fog se agregarán después.
}

// ----- PUBLICAR ESTADO -----

void publicarEstado() {
  if (!mqtt.connected()) return;

  String payload =
    "{\"id\":\"nodo5\",\"estado\":\"online\",\"ts\":" +
    String(millis()) +
    ",\"fase\":\"prueba_mqtt_sin_hmac\"}";

  mqtt.publish(
    TOPIC_ESTADO,
    payload.c_str(),
    true
  );

  Serial.println("Heartbeat MQTT publicado");
}

// ----- CONEXION MQTT -----

void conectarMQTT() {
  if (WiFi.status() != WL_CONNECTED) return;

  Serial.println("Conectando a HiveMQ...");

  String clientId =
    "g1-gateway-eduardo-" +
    String((uint32_t)esp_random(), HEX);

  const char* offline =
    "{\"id\":\"nodo5\",\"estado\":\"offline\"}";

  if (mqtt.connect(
    clientId.c_str(),
    TOPIC_ESTADO,
    1,
    true,
    offline
  )) {
    Serial.println("MQTT CONECTADO");

    mqtt.subscribe("upc/g1/+/datos");
    mqtt.subscribe("upc/g1/+/alerta");
    mqtt.subscribe("upc/g1/+/estado");
    mqtt.subscribe("upc/g1/seguridad/eventos");

    publicarEstado();
    ultimoHeartbeat = millis();

  } else {
    Serial.print("Error MQTT, codigo: ");
    Serial.println(mqtt.state());
  }

  actualizarPantalla();
}

// ----- SETUP -----

void setup() {
  Serial.begin(115200);
  pinMode(BTN_PIN, INPUT_PULLUP);

  Wire.begin(I2C_SDA, I2C_SCL);
  SPI.begin(18, 19, 23, TFT_CS);

  pixel.begin();
  pixel.setBrightness(80);
  pixel.clear();
  pixel.show();

  tft.begin();
  tft.setRotation(1);

  rtcDisponible = rtc.begin();

  Serial.println(
    rtcDisponible ? "RTC detectado" : "ERROR RTC"
  );

  mqtt.setServer(MQTT_HOST, MQTT_PORT);
  mqtt.setCallback(recibirMQTT);
  mqtt.setBufferSize(1024);
  mqtt.setKeepAlive(30);

  actualizarLED();
  actualizarPantalla();

  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASS, 6);

  Serial.println("G1 PESCA - GATEWAY INICIADO");
  Serial.println("Conectando WiFi...");

  Serial.println("N = Normal");
  Serial.println("A = Alerta");
  Serial.println("C = Critico");
}

// ----- LOOP -----

void loop() {
  unsigned long ahora = millis();

  // Reconexión WiFi
  if (WiFi.status() != WL_CONNECTED) {
    if (ahora - ultimoIntentoWiFi >= 10000) {
      ultimoIntentoWiFi = ahora;
      WiFi.reconnect();
      actualizarPantalla();
    }
  } else {
    // Reconexión MQTT sin bucle infinito
    if (!mqtt.connected()) {
      if (ahora - ultimoIntentoMQTT >= 5000) {
        ultimoIntentoMQTT = ahora;
        conectarMQTT();
      }
    } else {
      mqtt.loop();

      if (ahora - ultimoHeartbeat >= 30000) {
        ultimoHeartbeat = ahora;
        publicarEstado();
      }
    }
  }

  // Pruebas locales por consola
  if (Serial.available()) {
    char comando = Serial.read();

    if (comando == 'N' || comando == 'n')
      cambiarEstado(NORMAL);

    if (comando == 'A' || comando == 'a')
      cambiarEstado(ALERTA);

    if (comando == 'C' || comando == 'c')
      cambiarEstado(CRITICO);
  }

  // Pulsador con antirrebote
  bool lectura = digitalRead(BTN_PIN);

  if (lectura != botonAnterior) {
    tiempoBoton = ahora;
  }

  if (ahora - tiempoBoton > 50) {
    if (lectura != botonEstable) {
      botonEstable = lectura;

      if (botonEstable == LOW) {
        reconocida = true;
        Serial.println(
          "Pulsador detectado: reconocimiento"
        );
        actualizarPantalla();
      }
    }
  }

  botonAnterior = lectura;

  // Actualiza solamente la zona del reloj
  if (ahora - ultimaHora >= 1000) {
    ultimaHora = ahora;
    actualizarHora();
  }

  delay(10);
}
