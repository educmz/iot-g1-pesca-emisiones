/*
  G1 Pesca — Nodo 3: PERÍMETRO
  Responsable: Gonzalo Aliaga Urbina · Rama: nodo3-gonzalo

  Parte del problema que resuelve:
    Verifica si la emisión LLEGA a la comunidad según el viento.

  Detecta:  MQ-2 (analógica, GPIO34)          -> gas en el borde de la planta (ppm simulados)
            Potenciómetro (analógica, GPIO35) -> dirección del viento (0–359°)
  Actúa:    LED rojo (GPIO26)                 -> indicador de riesgo hacia la comunidad
              apagado  = BAJO  (no hay gas en el borde)
              parpadeo = MEDIO (hay gas, pero el viento va a otro lado o aún no es crítico)
              fijo     = ALTO  (gas crítico y viento hacia la comunidad)

  Contrato MQTT del grupo (docs/contrato-mqtt/README.md):
    - Un mensaje por sensor: id, ts (ms desde arranque), seq, sensor, valor, unidad, calidad, firma
    - Estado retenido + LWT {"id":"nodo3","estado":"offline"}, latido cada 30 s
    - Eventos de seguridad en upc/g1/seguridad/eventos
    - Solicitudes de ataque del dashboard en upc/g1/sim/ataque
*/

#include <WiFi.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>
#include <time.h>
#include "mbedtls/md.h"
#include "secrets.h"     // WIFI_SSID, WIFI_PASSWORD, MQTT_HOST, MQTT_PORT, HMAC_KEY_NODO3 (NO se sube a GitHub)

// ---------------- Pines ----------------
// GPIO34 y 35 son del ADC1: el ADC2 deja de funcionar cuando el Wi-Fi está activo.
const int PIN_GAS    = 34;
const int PIN_VIENTO = 35;
const int PIN_LED    = 26;

// ---------------- Parámetros (el docente puede pedir cambiar uno) ----------------
// El MQ-2 de Wokwi entrega una señal LOGARÍTMICA: cada vez que el gas se multiplica por 10,
// el ADC sube un monto parecido. Se calibró con dos puntos medidos en Wokwi:
//   deslizador 400 ppm -> ADC ~3628   ·   deslizador 100000 ppm -> ADC ~4042
// Si al probar no coincide con el deslizador, ajustar estas dos constantes.
const float ADC_EN_400PPM   = 3628;
const float ADC_POR_DECADA  = 172;   // (4042 - 3628) / (log10(100000) - log10(400))
const float UMBRAL_ALERTA  = 1000;   // ppm simulados MQ-2: ya hay olor en el borde
const float UMBRAL_CRITICO = 5000;   // ppm simulados MQ-2: la emisión está saliendo de la planta
const float HISTERESIS     = 0.10;   // 10 %: para bajar de nivel el gas debe caer un poco más
const int   DIR_COMUNIDAD  = 90;     // la zona residencial está al ESTE (90°)
const int   TOLERANCIA_DIR = 45;     // ±45° cuenta como "viento hacia la comunidad"
const int   SALTO_MAXIMO   = 800;    // cuentas de ADC en 0,5 s: más que esto es sospechoso

const unsigned long PERIODO_LECTURA  = 500;     // ms
const unsigned long PERIODO_PUBLICAR = 2000;    // ms (laboratorio: cada 1 a 2 s)
const unsigned long PERIODO_LATIDO   = 30000;   // ms (contrato: 30 s)
const unsigned long TIMEOUT_GATEWAY  = 60000;   // ms sin gateway -> modo seguro

// ---------------- Tópicos ----------------
const char* NODO_ID   = "nodo3";
const char* T_DATOS   = "upc/g1/nodo3/datos";
const char* T_ALERTA  = "upc/g1/nodo3/alerta";
const char* T_ESTADO  = "upc/g1/nodo3/estado";
const char* T_CMD     = "upc/g1/nodo3/cmd";
const char* T_GW      = "upc/g1/nodo5/estado";        // latido del gateway
const char* T_POZAS   = "upc/g1/nodo1/datos";         // contexto para la coherencia
const char* T_SECAD   = "upc/g1/nodo2/datos";
const char* T_SEG     = "upc/g1/seguridad/eventos";
const char* T_SIM     = "upc/g1/sim/ataque";

WiFiClient wifi;
PubSubClient mqtt(wifi);

// ---------------- Estado ----------------
enum Riesgo { BAJO, MEDIO, ALTO };
const char* NOMBRE_RIESGO[] = { "BAJO", "MEDIO", "ALTO" };

float  gasPpm = 0;
int    vientoGrados = 0;
bool   haciaComunidad = false;
String calidad = "ok";
Riesgo riesgo = BAJO, riesgoPublicado = BAJO;
bool   modoSeguro = true;              // arranca seguro hasta escuchar al gateway
unsigned long ultimoGateway = 0;
unsigned long seq = 0;                 // contador de mensajes propios
String pozasTxt = "sin dato", secadTxt = "sin dato";
String mensajeViejo = "";              // guardado para simular el ataque de repetición
unsigned long tLectura = 0, tPublicar = 0, tLatido = 0, tReconexion = 0, tUltimoEvento = 0, tUltimoDesmentido = 0;

// ================= Utilidades =================
String hmacHex(const String& msg) {
  uint8_t out[32];
  mbedtls_md_context_t ctx;
  mbedtls_md_init(&ctx);
  mbedtls_md_setup(&ctx, mbedtls_md_info_from_type(MBEDTLS_MD_SHA256), 1);
  mbedtls_md_hmac_starts(&ctx, (const unsigned char*)HMAC_KEY_NODO3, strlen(HMAC_KEY_NODO3));
  mbedtls_md_hmac_update(&ctx, (const unsigned char*)msg.c_str(), msg.length());
  mbedtls_md_hmac_finish(&ctx, out);
  mbedtls_md_free(&ctx);
  char hex[65];
  for (int i = 0; i < 32; i++) sprintf(hex + 2 * i, "%02x", out[i]);
  hex[64] = 0;
  return String(hex);
}

// Texto canónico que se firma (propuesta para el contrato del grupo).
// id|ts|seq|sensor|valor(2 decimales)|unidad|calidad   -> HMAC-SHA256 en hex (64 caracteres)
String textoCanonico(const char* id, unsigned long ts, unsigned long s, const char* sensor,
                     float valor, const char* unidad, const char* cal) {
  char buf[160];
  snprintf(buf, sizeof(buf), "%s|%lu|%lu|%s|%.2f|%s|%s", id, ts, s, sensor, valor, unidad, cal);
  return String(buf);
}

int diferenciaAngular(int a, int b) {
  int d = abs(a - b) % 360;
  return d > 180 ? 360 - d : d;
}

String horaTexto() {                   // hora de reloj si hubo NTP; si no, segundos desde el arranque
  struct tm t;
  if (getLocalTime(&t, 10)) { char b[12]; strftime(b, sizeof(b), "%H:%M:%S", &t); return String(b); }
  return "t+" + String(millis() / 1000) + "s";
}

// ================= Detectar =================
int rawGas = 0, rawGasAnterior = -1;

void leerSensores() {
  rawGas        = analogRead(PIN_GAS);
  int rawViento = analogRead(PIN_VIENTO);

  float ppm = pow(10, log10(400.0) + (rawGas - ADC_EN_400PPM) / ADC_POR_DECADA);
  gasPpm = roundf(ppm);                                      // ppm enteros (unidades simuladas MQ-2)
  vientoGrados = (int)(rawViento * 359L / 4095L);
  haciaComunidad = diferenciaAngular(vientoGrados, DIR_COMUNIDAD) <= TOLERANCIA_DIR;

  if (rawGas <= 0 || rawGas >= 4095) calidad = "fuera_de_rango";          // suelto o saturado
  else if (rawGasAnterior >= 0 && abs(rawGas - rawGasAnterior) > SALTO_MAXIMO) calidad = "sospechosa";
  else calidad = "ok";
  rawGasAnterior = rawGas;
}

// ================= Decidir (edge) =================
void decidirRiesgo() {
  // Sin gateway (modo seguro) somos más conservadores: basta el nivel de ALERTA.
  float limiteAlto = modoSeguro ? UMBRAL_ALERTA : UMBRAL_CRITICO;
  float f = (riesgo == BAJO) ? 1.0 : (1.0 - HISTERESIS);

  if (gasPpm >= limiteAlto * f && haciaComunidad) riesgo = ALTO;
  else if (gasPpm >= UMBRAL_ALERTA * f)            riesgo = MEDIO;
  else                                             riesgo = BAJO;
}

// ================= Actuar =================
void actualizarLed() {
  if (riesgo == ALTO)       digitalWrite(PIN_LED, HIGH);
  else if (riesgo == MEDIO) digitalWrite(PIN_LED, (millis() / 250) % 2);   // parpadeo 2 Hz
  else                      digitalWrite(PIN_LED, LOW);
}

// ================= Publicar =================
void publicarLectura(const char* sensor, float valor, const char* unidad, const char* cal) {
  if (!mqtt.connected()) return;
  seq++;
  unsigned long ts = millis();
  JsonDocument doc;
  doc["id"] = NODO_ID;  doc["ts"] = ts;  doc["seq"] = seq;  doc["sensor"] = sensor;
  doc["valor"] = serialized(String(valor, 2));
  doc["unidad"] = unidad;  doc["calidad"] = cal;
  doc["firma"] = hmacHex(textoCanonico(NODO_ID, ts, seq, sensor, valor, unidad, cal));
  char out[320];
  serializeJson(doc, out);
  mqtt.publish(T_DATOS, out);
  if (mensajeViejo.length() == 0 && strcmp(sensor, "gas") == 0) mensajeViejo = out;
}

void publicarTodo() {
  publicarLectura("gas", gasPpm, "ppm", calidad.c_str());
  publicarLectura("viento", vientoGrados, "grados", "ok");
  publicarLectura("riesgo", (float)riesgo, "nivel", "ok");     // 0 BAJO, 1 MEDIO, 2 ALTO (estado del LED)
}

void publicarAlerta(const char* tipo, const String& motivo) {
  Serial.printf("[ALERTA] %s: %s\n", tipo, motivo.c_str());
  if (!mqtt.connected()) return;
  unsigned long ts = millis();
  JsonDocument doc;
  doc["id"] = NODO_ID;  doc["ts"] = ts;  doc["tipo"] = tipo;
  doc["riesgo"] = NOMBRE_RIESGO[riesgo];
  doc["gas"] = serialized(String(gasPpm, 2));
  doc["viento"] = vientoGrados;  doc["hacia"] = haciaComunidad;
  doc["motivo"] = motivo;
  doc["firma"] = hmacHex(String(NODO_ID) + "|" + ts + "|" + tipo + "|" + motivo);
  char out[512];
  serializeJson(doc, out);
  mqtt.publish(T_ALERTA, out);
}

// Evento de seguridad con los campos que lee el dashboard del grupo
void publicarEventoSeguridad(const char* ataque, const String& accion, const String& motivo,
                             long seqRechazado, const String& firmaRechazada) {
  Serial.printf("[SEGURIDAD] %s | %s | %s\n", ataque, accion.c_str(), motivo.c_str());
  if (!mqtt.connected()) return;
  JsonDocument doc;
  doc["hora"] = horaTexto();  doc["nodo"] = NODO_ID;  doc["ataque"] = ataque;
  doc["accion"] = accion;  doc["motivo"] = motivo;
  doc["seq_rechazado"] = seqRechazado;
  doc["firma_rechazada"] = firmaRechazada.substring(0, 16);
  doc["ts"] = millis();
  char out[600];
  serializeJson(doc, out);
  mqtt.publish(T_SEG, out);
  mqtt.publish(T_ALERTA, out);
}

void publicarEstado() {
  if (!mqtt.connected()) return;
  JsonDocument doc;
  doc["id"] = NODO_ID;  doc["ts"] = millis();  doc["estado"] = "online";
  doc["modo_seguro"] = modoSeguro;  doc["rssi"] = WiFi.RSSI();
  char out[200];
  serializeJson(doc, out);
  mqtt.publish(T_ESTADO, out, true);          // retenido: reemplaza al LWT
}

// ================= Defensa: falso "aire limpio" =================
// El nodo escucha su PROPIO tópico. Todo mensaje que diga ser "nodo3" y no tenga
// una firma válida y reciente se rechaza, se registra y se desmiente.
void revisarMensajeEnMiTopico(JsonDocument& doc) {
  if (String((const char*)(doc["id"] | "")) != NODO_ID) return;

  unsigned long ts = doc["ts"] | 0UL;
  unsigned long s  = doc["seq"] | 0UL;
  const char* sensor = doc["sensor"] | "";
  float valor      = doc["valor"] | -1.0f;
  const char* uni  = doc["unidad"] | "";
  const char* cal  = doc["calidad"] | "";
  String firma     = doc["firma"] | "";

  bool firmaOk = firma == hmacHex(textoCanonico(NODO_ID, ts, s, sensor, valor, uni, cal));
  bool reciente = s <= seq && s + 6 >= seq;
  if (firmaOk && reciente) return;            // es un mensaje propio de vuelta: normal

  const char* ataque = firmaOk ? "repeticion" : "suplantacion";
  String motivo = firmaOk ? "Firma válida pero mensaje viejo reenviado (seq " + String(s) + ", actual " + String(seq) + ")"
                          : "Firma HMAC inválida (seq " + String(s) + ")";

  // Coherencia con el sensor real, el viento y las fuentes (pozas y secadores)
  if (strcmp(sensor, "gas") == 0 && valor < UMBRAL_ALERTA && gasPpm >= UMBRAL_ALERTA) {
    if (!firmaOk) ataque = "aire_limpio_falso";
    motivo += ". Incoherente: dice " + String(valor, 0) + " ppm y el MQ-2 mide " + String(gasPpm, 0) +
              " ppm, viento " + vientoGrados + "°" + (haciaComunidad ? " hacia la comunidad" : "") +
              " (pozas: " + pozasTxt + ", secadores: " + secadTxt + ")";
  } else if (strcmp(sensor, "viento") == 0 && diferenciaAngular((int)valor, vientoGrados) > 20) {
    motivo += ". Incoherente: dice viento " + String((int)valor) + "° y el sensor marca " + vientoGrados + "°";
  }

  if (millis() - tUltimoEvento > 1000) {      // máx. 1 evento por segundo: evita inundar el broker
    tUltimoEvento = millis();
    publicarEventoSeguridad(ataque, "Mensaje rechazado y lectura real republicada", motivo,
                            (long)s, firma.length() ? firma : String("(sin firma)"));
  }
  // Desmentir al atacante con lecturas reales firmadas, máx. una vez cada 2 s.
  // (Sin este límite, dos copias del nodo abiertas a la vez se "desmienten" entre sí sin parar.)
  if (millis() - tUltimoDesmentido > 2000) { tUltimoDesmentido = millis(); publicarTodo(); }
}

// Solicitud del botón del dashboard: simula al atacante publicando en nuestro tópico.
// El mensaje inyectado NO lleva firma válida, así que pasa por la misma defensa de arriba.
void simularAtaque(const char* tipo) {
  if (strcmp(tipo, "aire_limpio_falso") == 0) {
    JsonDocument doc;
    doc["id"] = NODO_ID;  doc["ts"] = millis();  doc["seq"] = seq + 1;  doc["sensor"] = "gas";
    doc["valor"] = 0.0;  doc["unidad"] = "ppm";  doc["calidad"] = "ok";
    doc["firma"] = "0000000000000000000000000000000000000000000000000000000000000000";
    char out[320];
    serializeJson(doc, out);
    Serial.println("[SIM] Inyectando mensaje falso de aire limpio (sin firma válida)");
    mqtt.publish(T_DATOS, out);
  } else if (strcmp(tipo, "repeticion") == 0 && mensajeViejo.length()) {
    Serial.println("[SIM] Reenviando un mensaje viejo (repetición)");
    mqtt.publish(T_DATOS, mensajeViejo.c_str());
  }
}

// ================= MQTT =================
String resumenFuente(JsonDocument& doc) {
  return String((float)(doc["valor"] | 0.0f), 1) + " " + String((const char*)(doc["unidad"] | ""));
}

void alRecibir(char* topic, byte* payload, unsigned int len) {
  String t(topic);
  if (t == T_GW) {
    String p((char*)payload, len);
    if (p.indexOf("offline") < 0) ultimoGateway = millis();
    return;
  }
  JsonDocument doc;
  if (deserializeJson(doc, payload, len)) {
    if (t == T_DATOS) publicarEventoSeguridad("mensaje_invalido", "Ignorado", "JSON mal formado en el tópico del nodo 3", -1, "");
    return;
  }
  if (t == T_DATOS) { revisarMensajeEnMiTopico(doc); return; }
  if (t == T_POZAS || t == T_SECAD) {
    String sensor = doc["sensor"] | "";
    if (sensor.startsWith("gas")) (t == T_POZAS ? pozasTxt : secadTxt) = resumenFuente(doc);
    return;
  }
  if (t == T_SIM) {
    if (String((const char*)(doc["id"] | "")) == NODO_ID) simularAtaque(doc["ataque"] | "");
    return;
  }
  // T_CMD: el nodo 3 no tiene actuadores que el gateway deba mandar. Suscrito por contrato.
}

void conectar() {
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("[WiFi] sin conexión, reintentando...");
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);   // no bloquea: el LED sigue funcionando sin red
    return;
  }
  if (mqtt.connected()) return;
  String clientId = "upc-g1-nodo3-" + String((uint32_t)esp_random(), HEX);   // único por instancia
  if (mqtt.connect(clientId.c_str(), nullptr, nullptr, T_ESTADO, 1, true,
                   "{\"id\":\"nodo3\",\"estado\":\"offline\"}")) {          // LWT del contrato
    mqtt.subscribe(T_DATOS);
    mqtt.subscribe(T_CMD);
    mqtt.subscribe(T_GW);
    mqtt.subscribe(T_POZAS);
    mqtt.subscribe(T_SECAD);
    mqtt.subscribe(T_SIM);
    publicarEstado();
    Serial.println("[MQTT] conectado a " MQTT_HOST);
  } else {
    // -2 = no llega al broker (red/DNS), -4 = sin respuesta, 2 = id rechazado, 5 = no autorizado
    Serial.printf("[MQTT] no se pudo conectar, estado=%d (reintento en 5 s)\n", mqtt.state());
  }
}

// ================= Programa =================
void setup() {
  Serial.begin(115200);
  pinMode(PIN_LED, OUTPUT);
  analogReadResolution(12);
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  configTime(-5 * 3600, 0, "pool.ntp.org");   // hora de Perú solo para mostrar en eventos
  mqtt.setServer(MQTT_HOST, MQTT_PORT);
  mqtt.setCallback(alRecibir);
  mqtt.setBufferSize(1024);
  mqtt.setSocketTimeout(2);
  Serial.println("Nodo 3 Perímetro listo");
}

void loop() {
  unsigned long m = millis();

  // 1) Detectar y decidir SIEMPRE, haya o no red (edge)
  if (m - tLectura >= PERIODO_LECTURA) {
    tLectura = m;
    leerSensores();
    bool antes = modoSeguro;
    modoSeguro = ultimoGateway == 0 || (m - ultimoGateway > TIMEOUT_GATEWAY);
    if (modoSeguro != antes)
      publicarAlerta(modoSeguro ? "modo_seguro_on" : "modo_seguro_off",
                     modoSeguro ? "Sin latido del gateway por más de 60 s: umbral local más estricto"
                                : "Gateway de vuelta: se usa el umbral normal");
    decidirRiesgo();
    if (riesgo != riesgoPublicado) {
      publicarAlerta("riesgo_comunidad",
                     String("Riesgo ") + NOMBRE_RIESGO[riesgoPublicado] + " -> " + NOMBRE_RIESGO[riesgo] +
                     " (" + String(gasPpm, 0) + " ppm, viento " + vientoGrados + "°" +
                     (haciaComunidad ? " hacia la comunidad)" : " hacia otro lado)"));
      riesgoPublicado = riesgo;
    }
  }
  // 2) Actuar
  actualizarLed();

  // 3) Comunicar (si se puede)
  if (m - tReconexion >= 5000 && (WiFi.status() != WL_CONNECTED || !mqtt.connected())) {
    tReconexion = m;
    conectar();
  }
  mqtt.loop();
  if (m - tPublicar >= PERIODO_PUBLICAR) {
    tPublicar = m;
    publicarTodo();
    Serial.printf("%s gas=%.0f ppm [ADC %d] (%s) viento=%d° %s riesgo=%s%s\n", mqtt.connected() ? "[MQTT ok]" : "[SIN MQTT]", gasPpm, rawGas, calidad.c_str(), vientoGrados,
                  haciaComunidad ? "HACIA comunidad" : "otro lado", NOMBRE_RIESGO[riesgo],
                  modoSeguro ? " [modo seguro]" : "");
  }
  if (m - tLatido >= PERIODO_LATIDO) { tLatido = m; publicarEstado(); }
}
