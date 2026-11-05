#include <Arduino.h>
#include <WiFi.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>
#include <Preferences.h>
#include <time.h>
#include <mbedtls/md.h>
#include "secrets.h"

constexpr uint8_t PIN_RELE = 26;
constexpr uint8_t PIN_CONFIRMACION = 32;
constexpr uint8_t PIN_STEP = 18;
constexpr uint8_t PIN_DIR = 19;
constexpr uint8_t PIN_ENABLE = 23;

constexpr uint32_t LIMITE_CONFIRMACION_MS = 10000;
constexpr uint32_t LIMITE_GATEWAY_MS = 60000;
constexpr int64_t ATRASO_MAX_COMANDO_S = 120;
constexpr int64_t ADELANTO_MAX_COMANDO_S = 45;
constexpr bool PERMITIR_COMANDOS_SERIAL_DE_PRUEBA = false;
constexpr uint32_t REPORTE_MS = 2000;
constexpr uint32_t HEARTBEAT_MS = 30000;
constexpr uint32_t RECONEXION_MS = 5000;
constexpr uint32_t WIFI_REINTENTO_MS = 20000;
constexpr uint32_t WIFI_DIAGNOSTICO_MS = 5000;

const char* TOPIC_CMD = "upc/g1/nodo4/cmd";
const char* TOPIC_DATOS = "upc/g1/nodo4/datos";
const char* TOPIC_PAQUETE = "upc/g1/nodo4/paquete"; // estado completo para panel individual
const char* TOPIC_SEG = "upc/g1/seguridad/eventos";
const char* TOPIC_ESTADO = "upc/g1/nodo4/estado";
const char* TOPIC_ALERTA = "upc/g1/nodo4/alerta";

WiFiClient wifiClient;
PubSubClient mqtt(wifiClient);
Preferences preferencias;

bool lavadorEncendido = false;
bool confirmado = false;
bool fallaReportada = false;
bool modoSeguro = true; // Arranque preventivo: aun no hubo contacto D5 autenticado.
bool riesgoAlto = false;
uint8_t intensidad = 2;

uint32_t inicioEsperaConfirmacion = 0;
uint32_t ultimoContactoGateway = 0;
uint32_t ultimoReporte = 0;
uint32_t ultimoHeartbeat = 0;
uint32_t ultimoIntentoWifi = 0;
uint32_t ultimoDiagnosticoWifi = 0;
uint32_t ultimoIntentoMqtt = 0;
bool wifiIntentado = false;
bool wifiConectadoReportado = false;
bool mqttIntentado = false;
uint32_t ultimoPasoUs = 0;
uint64_t ultimoContadorControl = 0;
uint64_t ultimoContadorHeartbeat = 0;
uint32_t secuenciaReporte = 0;
uint32_t secuenciaGrupo = 0;
bool almacenamientoOK = false;
String idCliente;

const char* nombreIntensidad() {
  switch (intensidad) {
    case 1: return "BAJA";
    case 2: return "MEDIA";
    default: return "MAXIMA";
  }
}

uint32_t intervaloPasoUs() {
  switch (intensidad) {
    case 1: return 12000;
    case 2: return 6500;
    default: return 3500;
  }
}

uint32_t tiempoMensaje() {
  time_t t = time(nullptr);
  return t > 1600000000 ? (uint32_t)t : millis() / 1000UL;
}

String baseFirmaComando(const char* id, uint32_t ts, uint64_t seq,
                        const char* cmd, const char* valor) {
  return String(id) + "|" + String(ts) + "|" +
         String((unsigned long long)seq) + "|" + cmd + "|" + valor;
}

String firmaHmac(const String& texto, const char* clave) {
  unsigned char digest[32];
  const mbedtls_md_info_t* info = mbedtls_md_info_from_type(MBEDTLS_MD_SHA256);
  if (!info || !clave || strlen(clave) < 24 ||
      strncmp(clave, "REEMPLAZAR", 9) == 0 ||
      strncmp(clave, "G1_Gateway_Lab_", 15) == 0 ||
      strncmp(clave, "G1_Nodo4_Lab_", 13) == 0 ||
      mbedtls_md_hmac(info, (const unsigned char*)clave, strlen(clave),
                              (const unsigned char*)texto.c_str(), texto.length(), digest) != 0) {
    return "";
  }
  char hex[65];
  const char* chars = "0123456789abcdef";
  for (size_t i = 0; i < sizeof(digest); ++i) {
    hex[i * 2] = chars[digest[i] >> 4];
    hex[i * 2 + 1] = chars[digest[i] & 0x0f];
  }
  hex[64] = '\0';
  return String(hex);
}

bool firmaIgual(const String& recibida, const String& esperada) {
  if (recibida.length() != 64 || esperada.length() != 64) return false;
  uint8_t diferencia = 0;
  for (size_t i = 0; i < 64; ++i) {
    diferencia |= (uint8_t)(recibida[i] ^ esperada[i]);
  }
  return diferencia == 0;
}

bool publicarJson(const char* topico, JsonDocument& doc, bool retener = false) {
  if (!mqtt.connected()) return false;
  char salida[900];
  size_t n = serializeJson(doc, salida, sizeof(salida));
  if (n == 0 || n >= sizeof(salida) - 1) return false;
  return mqtt.publish(topico, salida, retener);
}

void publicarEvento(const char* tipo, const char* motivo, uint64_t contador = 0) {
  Serial.printf("[EVENTO] NODO=D4 TIPO=%s MOTIVO=%s SEQ=%llu\n",
                tipo, motivo, (unsigned long long)contador);
  StaticJsonDocument<640> doc;
  uint32_t ts = tiempoMensaje();
  doc["id"] = "D4";
  doc["ts"] = ts;
  doc["valor"] = tipo;
  doc["unidad"] = "evento";
  doc["calidad"] = "VALIDO";
  doc["tipo"] = tipo;
  doc["motivo"] = motivo;
  doc["seq"] = contador;
  String canonico = String("D4|") + ts + "|" + tipo + "|evento|VALIDO|" +
                    tipo + "|" + motivo + "|" + String((unsigned long long)contador);
  doc["firma"] = firmaHmac(canonico, CLAVE_NODO4);
  publicarJson(TOPIC_ALERTA, doc);
}

void publicarPaquete() {
  StaticJsonDocument<640> doc;
  uint32_t ts = tiempoMensaje();
  String valor = lavadorEncendido ? "ON" : "OFF";
  String nivel = nombreIntensidad();
  String confirmacion = confirmado ? "SI" : "NO";
  String seguro = modoSeguro ? "SI" : "NO";
  String riesgo = riesgoAlto ? "ALTO" : "NORMAL";
  ++secuenciaReporte;
  doc["id"] = "D4";
  doc["ts"] = ts;
  doc["valor"] = valor;
  doc["unidad"] = "estado_lavador";
  doc["calidad"] = "VALIDO";
  doc["intensidad"] = nivel;
  doc["confirmado"] = confirmacion;
  doc["seguro"] = seguro;
  doc["riesgo"] = riesgo;
  doc["seq"] = secuenciaReporte;
  String canonico = String("D4|") + ts + "|" + valor + "|estado_lavador|VALIDO|" +
                    nivel + "|" + confirmacion + "|" + seguro + "|" + riesgo + "|" + secuenciaReporte;
  doc["firma"] = firmaHmac(canonico, CLAVE_NODO4);
  publicarJson(TOPIC_PAQUETE, doc);
}

// Contrato MQTT de grupo: un mensaje independiente HMAC-SHA256 por sensor.
// La firma protege id|ts_millis|seq|sensor|valor(2)|unidad|calidad.
// NO confiar en los datos por el mero hecho de verlos en el dashboard.
void publicarSensorGrupo(const char* sensor, float valor, const char* unidad) {
  if (!mqtt.connected()) return;
  const uint32_t ts=millis();
  const uint32_t seq=++secuenciaGrupo;
  const char* calidad="ok";
  String canonico=String("nodo4|")+ts+"|"+seq+"|"+sensor+"|"+
                  String(valor,2)+"|"+unidad+"|"+calidad;
  String firma=firmaHmac(canonico,CLAVE_NODO4);
  if(firma.length()!=64)return;
  StaticJsonDocument<360> j;
  j["id"]="nodo4"; j["ts"]=ts; j["seq"]=seq; j["sensor"]=sensor;
  j["valor"]=serialized(String(valor,2));
  j["unidad"]=unidad; j["calidad"]=calidad; j["firma"]=firma;
  char out[400];
  if(serializeJson(j,out,sizeof(out))>0)mqtt.publish(TOPIC_DATOS,out);
}

void publicarDatos() {
  publicarPaquete();  // Firma original, para el panel local de Pedro
  publicarSensorGrupo("lavador",lavadorEncendido?1.0f:0.0f,"estado");
  publicarSensorGrupo("confirmacion",confirmado?1.0f:0.0f,"estado");
  publicarSensorGrupo("intensidad",(float)intensidad,"nivel");
  publicarSensorGrupo("modo_seguro",modoSeguro?1.0f:0.0f,"estado");
  publicarSensorGrupo("riesgo_alto",riesgoAlto?1.0f:0.0f,"estado");
}

void publicarHeartbeat() {
  StaticJsonDocument<448> doc;
  uint32_t ts = tiempoMensaje();
  doc["id"] = "D4";
  doc["ts"] = ts;
  doc["valor"] = "online";
  doc["estado"] = "online";
  doc["unidad"] = "disponibilidad";
  doc["calidad"] = "VALIDO";
  doc["firma"] = firmaHmac(String("D4|") + ts + "|online|disponibilidad|VALIDO", CLAVE_NODO4);
  if (publicarJson(TOPIC_ESTADO, doc, true)) {
    Serial.println("[MQTT] HEARTBEAT D4=online");
  }
}

void actualizarSalidas() {
  digitalWrite(PIN_RELE, lavadorEncendido ? HIGH : LOW);
  digitalWrite(PIN_ENABLE, lavadorEncendido ? LOW : HIGH);
}

void encenderLavador(const char* motivo) {
  if (!lavadorEncendido) {
    lavadorEncendido = true;
    confirmado = false;
    fallaReportada = false;
    inicioEsperaConfirmacion = millis();
    Serial.printf("[ACTUACION] LAVADOR=ENCENDIDO MOTIVO=%s\n", motivo);
    publicarEvento("ACTUACION_ON", motivo);
  }
  actualizarSalidas();
}

void apagarLavador() {
  if (lavadorEncendido) {
    lavadorEncendido = false;
    confirmado = false;
    fallaReportada = false;
    Serial.println("[ACTUACION] LAVADOR=APAGADO");
    publicarEvento("ACTUACION_OFF", "ORDEN_AUTORIZADA");
  }
  actualizarSalidas();
}

void imprimirEstado() {
  Serial.printf(
    "[ESTADO] NODO=D4 LAVADOR=%s CONFIRMADO=%s INTENSIDAD=%s SEGURO=%s RIESGO=%s CONTACTO_GATEWAY_HACE=%lus MQTT=%s\n",
    lavadorEncendido ? "ON" : "OFF", confirmado ? "SI" : "NO", nombreIntensidad(),
    modoSeguro ? "SI" : "NO", riesgoAlto ? "ALTO" : "NORMAL",
    (unsigned long)((millis() - ultimoContactoGateway) / 1000UL),
    mqtt.connected() ? "CONECTADO" : "DESCONECTADO");
}

void recibirContactoGateway() {
  ultimoContactoGateway = millis();
  if (modoSeguro) {
    modoSeguro = false;
    Serial.println("[GATEWAY] CONTACTO AUTENTICADO RESTABLECIDO; LAVADOR CONSERVA SU ESTADO");
    publicarEvento("GATEWAY_RESTABLECIDO", "COMANDO_FIRMADO_VALIDO");
  }
}

void comprobarModoSeguro() {
  if (!modoSeguro && millis() - ultimoContactoGateway > LIMITE_GATEWAY_MS) {
    modoSeguro = true;
    intensidad = 3;
    Serial.println("[SEGURIDAD] SIN CONTACTO GATEWAY >60s; ENCENDIDO PREVENTIVO");
    encenderLavador("PERDIDA_GATEWAY");
    publicarEvento("MODO_SEGURO", "SIN_CONTACTO_GATEWAY_60S");
  }
}

void comprobarConfirmacion() {
  uint32_t ahora = millis();
  bool entradaActiva = digitalRead(PIN_CONFIRMACION) == LOW;
  bool nuevaConfirmacion = lavadorEncendido && entradaActiva;
  if (nuevaConfirmacion != confirmado) {
    confirmado = nuevaConfirmacion;
    if (confirmado) {
      fallaReportada = false;
      Serial.println("[CONFIRMACION] LAVADOR=OPERANDO; REPORTAR AL GATEWAY");
      publicarEvento("CONFIRMACION_OK", "INTERRUPTOR_ACTIVO");
    } else if (lavadorEncendido) {
      inicioEsperaConfirmacion = ahora;
      fallaReportada = false;
      Serial.println("[CONFIRMACION] SENAL PERDIDA; INICIA NUEVA VERIFICACION");
      publicarEvento("CONFIRMACION_PERDIDA", "INTERRUPTOR_INACTIVO");
    }
  }
  if (lavadorEncendido && !confirmado && !fallaReportada &&
      ahora - inicioEsperaConfirmacion >= LIMITE_CONFIRMACION_MS) {
    fallaReportada = true;
    Serial.println("[ALARMA] FALLA_LAVADOR: SIN CONFIRMACION EN 10s");
    publicarEvento("FALLA_LAVADOR", "SIN_CONFIRMACION_10S");
    Serial.println("[GATEWAY] D5 DEBE ORDENAR VENTILACION MAXIMA A D1");
  }
}

void actualizarMotor() {
  if (!lavadorEncendido) return;
  uint32_t ahora = micros();
  if ((uint32_t)(ahora - ultimoPasoUs) >= intervaloPasoUs()) {
    ultimoPasoUs = ahora;
    digitalWrite(PIN_STEP, HIGH);
    delayMicroseconds(3);
    digitalWrite(PIN_STEP, LOW);
  }
}

void ejecutarOrden(const String& cmd) {
  if (cmd == "HB") {
    Serial.println("[GATEWAY] HEARTBEAT FIRMADO ACEPTADO");
  } else if (cmd == "ON") {
    encenderLavador("ORDEN_GATEWAY_FIRMADA");
  } else if (cmd == "OFF") {
    if (modoSeguro || riesgoAlto) {
      Serial.println("[RECHAZO] APAGADO BLOQUEADO POR REGLA DE SEGURIDAD");
      publicarEvento("APAGADO_BLOQUEADO", modoSeguro ? "MODO_SEGURO" : "RIESGO_ALTO");
    } else {
      apagarLavador();
    }
  } else if (cmd == "BAJO" || cmd == "MEDIO" || cmd == "MAX") {
    if (modoSeguro && cmd != "MAX") {
      Serial.println("[RECHAZO] NO REDUCIR INTENSIDAD EN MODO SEGURO");
      publicarEvento("INTENSIDAD_BLOQUEADA", "MODO_SEGURO");
    } else if (riesgoAlto && cmd != "MAX") {
      Serial.println("[RECHAZO] NO REDUCIR INTENSIDAD CON RIESGO ALTO");
      publicarEvento("INTENSIDAD_BLOQUEADA", "RIESGO_ALTO");
    } else {
      intensidad = cmd == "BAJO" ? 1 : cmd == "MEDIO" ? 2 : 3;
      Serial.printf("[MOTOR] INTENSIDAD=%s\n", nombreIntensidad());
    }
  } else if (cmd == "RIESGO_ON") {
    riesgoAlto = true;
    intensidad = 3;
    encenderLavador("RIESGO_ALTO_GATEWAY");
    publicarEvento("RIESGO_ALTO", "ORDEN_GATEWAY_FIRMADA");
  } else if (cmd == "RIESGO_OFF") {
    riesgoAlto = false;
    publicarEvento("RIESGO_NORMAL", "ORDEN_GATEWAY_FIRMADA");
  }
}

bool comandoPermitido(const String& cmd) {
  return cmd == "HB" || cmd == "ON" || cmd == "OFF" ||
         cmd == "BAJO" || cmd == "MEDIO" || cmd == "MAX" ||
         cmd == "RIESGO_ON" || cmd == "RIESGO_OFF";
}

void recibirMqtt(char* topic, byte* payload, unsigned int longitud) {
  if (strcmp(topic, TOPIC_CMD) != 0) return;
  if (longitud == 0 || longitud > 750) {
    publicarEvento("ORDEN_RECHAZADA", "TAMANO_INVALIDO");
    return;
  }
  char json[751];
  memcpy(json, payload, longitud);
  json[longitud] = '\0';
  StaticJsonDocument<768> doc;
  DeserializationError error = deserializeJson(doc, json);
  bool tsNumerico = doc["ts"].is<uint32_t>() || doc["ts"].is<int32_t>();
  bool seqNumerico = doc["seq"].is<uint64_t>() || doc["seq"].is<int64_t>() ||
                     doc["seq"].is<uint32_t>() || doc["seq"].is<int32_t>();
  if (error || !doc["id"].is<const char*>() || !doc["cmd"].is<const char*>() ||
      !doc["firma"].is<const char*>() || !tsNumerico ||
      !seqNumerico || !doc["valor"].is<const char*>()) {
    publicarEvento("ORDEN_RECHAZADA", "FORMATO_INVALIDO");
    return;
  }
  String id = doc["id"].as<String>();
  uint32_t ts = doc["ts"].as<uint32_t>();
  uint64_t seq = doc["seq"].as<uint64_t>();
  String cmd = doc["cmd"].as<String>();
  String valor = doc["valor"].as<String>();
  String firma = doc["firma"].as<String>();
  if (id != "D5" || !comandoPermitido(cmd) || ts == 0 || seq == 0 || valor.length() > 50) {
    publicarEvento("ORDEN_RECHAZADA", "ORIGEN_O_ORDEN_INVALIDA", seq);
    return;
  }
  // Sin hora NTP fiable nunca aceptar una orden remota, aunque esté firmada.
  // Evita aceptar una captura vieja con seq superior que fuerce un actuador.
  time_t utc = time(nullptr);
  if (utc < 1700000000LL ||
      (int64_t)utc-(int64_t)ts > ATRASO_MAX_COMANDO_S ||
      (int64_t)ts-(int64_t)utc > ADELANTO_MAX_COMANDO_S) {
    publicarEvento("ORDEN_RECHAZADA", "TS_FUERA_DE_VENTANA_O_SIN_NTP", seq);
    return;
  }
  String canonico = baseFirmaComando(id.c_str(), ts, seq, cmd.c_str(), valor.c_str());
  String esperada = firmaHmac(canonico, CLAVE_GATEWAY);
  if (!firmaIgual(firma, esperada)) {
    Serial.println("[CIBERSEGURIDAD] ORDEN FALSA RECHAZADA: HMAC INVALIDA");
    publicarEvento("HMAC_INVALIDA", "FIRMA_INVALIDA", seq);
    return;
  }
  bool esHeartbeat = (cmd == "HB");
  uint64_t ultimoAceptado = esHeartbeat ? ultimoContadorHeartbeat : ultimoContadorControl;
  if (seq <= ultimoAceptado) {
    Serial.printf("[CIBERSEGURIDAD] REPLAY RECHAZADO CMD=%s SEQ=%llu ULTIMO=%llu\n",
                  cmd.c_str(), (unsigned long long)seq, (unsigned long long)ultimoAceptado);
    publicarEvento("REPLAY_BLOQUEADO", "CONTADOR_REPETIDO_O_ANTERIOR", seq);
    return;
  }
  if(!almacenamientoOK) {
    publicarEvento("ORDEN_RECHAZADA","NVS_INDISPONIBLE",seq);
    return;
  }
  size_t bytes = preferencias.putULong64(esHeartbeat ? "seqHB" : "seqCtrl", seq);
  if (bytes == 0) {
    Serial.println("[SEGURIDAD] ERROR AL GUARDAR CONTADOR; ORDEN RECHAZADA");
    publicarEvento("ORDEN_RECHAZADA", "FALLO_PERSISTENCIA_CONTADOR", seq);
    return;
  }
  if (esHeartbeat) ultimoContadorHeartbeat = seq;
  else ultimoContadorControl = seq;
  recibirContactoGateway();
  Serial.printf("[MQTT] ORDEN AUTENTICADA CMD=%s SEQ=%llu\n", cmd.c_str(), (unsigned long long)seq);
  bool accionBloqueada =
      (cmd == "OFF" && (modoSeguro || riesgoAlto)) ||
      ((cmd == "BAJO" || cmd == "MEDIO") && (modoSeguro || riesgoAlto));
  ejecutarOrden(cmd);
  if (!esHeartbeat) {
    const char* resultado = accionBloqueada ? "COMANDO_BLOQUEADO" : "COMANDO_EJECUTADO";
    Serial.printf("[MQTT] RESULTADO CMD=%s ESTADO=%s LAVADOR=%s\n",
                  cmd.c_str(), resultado, lavadorEncendido ? "ON" : "OFF");
    publicarEvento(resultado, cmd.c_str(), seq);
  }
  publicarDatos();
}

void conectarRed() {
  uint32_t ahora = millis();

  if (WiFi.status() != WL_CONNECTED) {
    wifiConectadoReportado = false;

    if (!wifiIntentado || ahora - ultimoIntentoWifi >= WIFI_REINTENTO_MS) {
      wifiIntentado = true;
      ultimoIntentoWifi = ahora;
      WiFi.mode(WIFI_STA);
      WiFi.setAutoReconnect(true);
      WiFi.begin("Wokwi-GUEST", "", 6);
      Serial.printf("[RED] CONECTANDO WIFI Wokwi-GUEST, estado=%d\n", (int)WiFi.status());
    } else if (ahora - ultimoDiagnosticoWifi >= WIFI_DIAGNOSTICO_MS) {
      ultimoDiagnosticoWifi = ahora;
      Serial.printf("[RED] ESPERANDO WIFI estado=%d, intento_hace=%lus\n",
                    (int)WiFi.status(),
                    (unsigned long)((ahora - ultimoIntentoWifi) / 1000UL));
    }
    return;
  }

  if (!wifiConectadoReportado) {
    wifiConectadoReportado = true;
    Serial.printf("[RED] WIFI CONECTADO IP=%s\n", WiFi.localIP().toString().c_str());
  }

  if (mqtt.connected()) return;
  if (mqttIntentado && ahora - ultimoIntentoMqtt < RECONEXION_MS) return;

  mqttIntentado = true;
  ultimoIntentoMqtt = ahora;

  if (mqtt.connect(idCliente.c_str(), TOPIC_ESTADO, 1, true, "{\"id\":\"nodo4\",\"estado\":\"offline\"}")) {
    Serial.println("[MQTT] CONECTADO broker.hivemq.com:1883");
    if (mqtt.subscribe(TOPIC_CMD, 1)) {
      Serial.printf("[MQTT] SUBSCRITO=%s\n", TOPIC_CMD);
    } else {
      Serial.printf("[MQTT] ERROR SUBSCRIPCION=%s\n", TOPIC_CMD);
    }
    publicarHeartbeat();
    publicarDatos();
  } else {
    Serial.printf("[MQTT] FALLO CONEXION rc=%d\n", mqtt.state());
  }
}

void procesarComandoSerial(String comando) {
  comando.trim();
  comando.toUpperCase();
  if (comando.length() == 0) return;
  if (comando == "ESTADO") {
    imprimirEstado();
  } else if (comando == "SIN_GATEWAY") {
    ultimoContactoGateway = millis() - LIMITE_GATEWAY_MS - 1UL;
    comprobarModoSeguro();
  } else if (comando == "HB") {
    Serial.println("[LOCAL] HB manual NO autentica contacto, modo seguro no cambia");
  } else if (comando == "ON" || comando == "OFF" || comando == "BAJO" ||
             comando == "MEDIO" || comando == "MAX" ||
             comando == "RIESGO_ON" || comando == "RIESGO_OFF") {
    if (PERMITIR_COMANDOS_SERIAL_DE_PRUEBA) {
      Serial.printf("[LOCAL] COMANDO DE PRUEBA POR SERIAL: %s\n", comando.c_str());
      ejecutarOrden(comando);
      publicarDatos();
    } else {
      Serial.println("[LOCAL] Control serial bloqueado; use /cmd con HMAC");
    }
  } else if (comando == "AYUDA") {
    Serial.println("[LOCAL] ESTADO | ON | OFF | BAJO | MEDIO | MAX | RIESGO_ON | RIESGO_OFF | SIN_GATEWAY | HB");
    Serial.println("[MQTT] SOLO /cmd CON HMAC Y CONTADOR PUEDE SIMULAR ORDENES REMOTAS");
  } else {
    Serial.println("[LOCAL] COMANDO DESCONOCIDO");
  }
}

void setup() {
  Serial.begin(115200);
  Serial.setTimeout(20);
  pinMode(PIN_RELE, OUTPUT);
  pinMode(PIN_CONFIRMACION, INPUT_PULLUP);
  pinMode(PIN_STEP, OUTPUT);
  pinMode(PIN_DIR, OUTPUT);
  pinMode(PIN_ENABLE, OUTPUT);
  digitalWrite(PIN_STEP, LOW);
  digitalWrite(PIN_DIR, HIGH);
  actualizarSalidas();
  almacenamientoOK = preferencias.begin("g1d4-mqtt", false);
  if(!almacenamientoOK)Serial.println("[SEGURIDAD] NVS no disponible: comandos MQTT se rechazaran");
  if(almacenamientoOK){
    uint64_t contadorAnterior = preferencias.getULong64("ultimoSeq", 0);
    ultimoContadorControl = preferencias.getULong64("seqCtrl", contadorAnterior);
    ultimoContadorHeartbeat = preferencias.getULong64("seqHB", contadorAnterior);
  }
  ultimoContactoGateway = 0;  // Sin D5 auténtico, NUNCA asumir contacto al arrancar
  intensidad = 3;
  encenderLavador("ARRANQUE_PREVENTIVO_SIN_GATEWAY");
  ultimoReporte = millis();
  ultimoHeartbeat = millis();
  idCliente = "g1-pesca-d4-" + String((uint32_t)(ESP.getEfuseMac() & 0xffffffff), HEX);
  mqtt.setServer(MQTT_HOST, MQTT_PORT);
  mqtt.setCallback(recibirMqtt);
  mqtt.setKeepAlive(15);
  mqtt.setSocketTimeout(1);
  mqtt.setBufferSize(1024);
  configTime(0, 0, "pool.ntp.org", "time.google.com");
  Serial.println("[INICIO] G1 PESCA - NODO D4 - MQTT / HMAC / ANTI-REPLAY");
  Serial.printf("[SEGURIDAD] ULTIMO_CONTROL=%llu ULTIMO_HB=%llu\n",
                (unsigned long long)ultimoContadorControl,
                (unsigned long long)ultimoContadorHeartbeat);
  Serial.println("[LOCAL] AYUDA para pruebas manuales; comandos remotos requieren firma");
  imprimirEstado();
  conectarRed();
}

void loop() {
  if (Serial.available() > 0) procesarComandoSerial(Serial.readStringUntil('\n'));
  if (mqtt.connected()) mqtt.loop();
  conectarRed();
  comprobarModoSeguro();
  comprobarConfirmacion();
  actualizarMotor();
  uint32_t ahora = millis();
  if (ahora - ultimoReporte >= REPORTE_MS) {
    ultimoReporte = ahora;
    imprimirEstado();
    publicarDatos();
  }
  if (ahora - ultimoHeartbeat >= HEARTBEAT_MS) {
    ultimoHeartbeat = ahora;
    publicarHeartbeat();
  }
}
