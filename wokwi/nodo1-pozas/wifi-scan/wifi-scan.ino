/* ESP32 WiFi Scanning example */

#include <WiFi.h>
#include <PubSubClient.h>
#include <OneWire.h>
#include <DallasTemperature.h>
#include <ArduinoJson.h>

// --- Configuración WiFi y MQTT (HiveMQ Público) ---
const char* ssid = "Wokwi-GUEST"; 
const char* password = "";
const char* mqtt_server = "broker.hivemq.com";
const char* id_cliente = "G1_Nodo1_Pozas";

// --- Tópicos ---
const char* topic_datos = "upc/g1/nodo1/datos";
const char* topic_estado = "upc/g1/nodo1/estado";
const char* topic_cmd = "upc/g1/nodo/cmd"; 

// --- Pines corregidos según tu diagrama físico ---
const int PIN_MQ2 = 4;       // Entrada analógica (Gas)
const int PIN_DS18B20 = 15;  // Entrada digital 1-Wire (Temperatura)
const int PIN_BUZZER = 16;   // Salida digital (Sirena local)
const int PIN_RELE = 17;     // Salida digital (Ventilación)

// --- Variables de Sensores y Seguridad ---
OneWire oneWire(PIN_DS18B20);
DallasTemperature sensors(&oneWire);
WiFiClient espClient;
PubSubClient client(espClient);

int gasAnterior = 0;
unsigned long ultimoEnvio = 0;

// Reglas de negocio y defensa
const int UMBRAL_GAS_CRITICO = 1500;
const int VALOR_MAX_GAS = 10000;   // Defensa: Rango imposible
const int SALTO_MAXIMO = 5000;     // Defensa: Salto brusco

void setup_wifi() {
  Serial.println("Conectando a WiFi...");
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println(" Conectado!");
}

void callback(char* topic, byte* payload, unsigned int length) {
  String mensaje = "";
  for (int i = 0; i < length; i++) {
    mensaje += (char)payload[i];
  }
  Serial.println("Orden recibida en " + String(topic) + ": " + mensaje);
  
  if (mensaje.indexOf("VENT_MAX") >= 0) {
    digitalWrite(PIN_RELE, HIGH);
    Serial.println("ACCIÓN: Ventilación máxima activada por orden del Gateway.");
  }
}

void reconnect() {
  while (!client.connected()) {
    Serial.print("Conectando a MQTT...");
    if (client.connect(id_cliente, "upc/g1/nodo1/estado", 0, true, "offline")) {
      Serial.println("Conectado");
      client.publish(topic_estado, "online", true); 
      client.subscribe(topic_cmd);
    } else {
      Serial.print("Fallo, rc=");
      Serial.print(client.state());
      delay(5000);
    }
  }
}

void setup() {
  Serial.begin(115200);
  delay(1500); // Solución para que el Monitor Serial no se congele

  pinMode(PIN_BUZZER, OUTPUT);
  pinMode(PIN_RELE, OUTPUT);
  digitalWrite(PIN_BUZZER, LOW);
  digitalWrite(PIN_RELE, LOW);

  sensors.begin();
  setup_wifi();
  client.setServer(mqtt_server, 1883);
  client.setCallback(callback);
}

void loop() {
  if (!client.connected()) {
    reconnect();
  }
  client.loop();

  unsigned long ahora = millis();
  if (ahora - ultimoEnvio > 2000) { 
    ultimoEnvio = ahora;

    int lecturaGasActual = analogRead(PIN_MQ2);
    sensors.requestTemperatures();
    float temperatura = sensors.getTempCByIndex(0);

    bool lecturaValida = true;
    String motivoRechazo = "";

    if (lecturaGasActual > VALOR_MAX_GAS) {
      lecturaValida = false;
      motivoRechazo = "Fuera de rango";
    } else if (abs(lecturaGasActual - gasAnterior) > SALTO_MAXIMO) {
      lecturaValida = false;
      motivoRechazo = "Salto maximo excedido";
    }

    if (!lecturaValida) {
      Serial.println("ATAQUE DETECTADO Y RECHAZADO: Lectura " + String(lecturaGasActual) + " ppm. Motivo: " + motivoRechazo);
      return; 
    }
    
    gasAnterior = lecturaGasActual;

    if (lecturaGasActual > UMBRAL_GAS_CRITICO) {
      digitalWrite(PIN_BUZZER, HIGH);
      digitalWrite(PIN_RELE, HIGH);
      Serial.println("ALERTA LOCAL: Sirena y Ventilación ON");
    } else {
      digitalWrite(PIN_BUZZER, LOW);
      digitalWrite(PIN_RELE, LOW); 
    }

    StaticJsonDocument<200> doc;
    doc["id"] = "Nodo1";
    doc["gas"] = lecturaGasActual;
    doc["temp"] = temperatura;
    doc["calidad"] = "OK";
    
    char jsonBuffer[512];
    serializeJson(doc, jsonBuffer);
    client.publish(topic_datos, jsonBuffer);
    
    Serial.println("Publicado: " + String(jsonBuffer));
  }
}