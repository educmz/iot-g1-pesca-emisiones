/*
 * G1 Pesca / Nodo 1 Pozas (Fernando) - correccion colaborativa.
 * Wokwi: MQ-2 en ADC1 GPIO34; DS18B20 GPIO15; buzzer GPIO16; rele GPIO17.
 * Gas = cuentas ADC, NO ppm certificados de H2S/NH3.
 * Edge funciona sin red. Ordenes remotas: HMAC + seq persistente + fecha UTC.
 *
 * En Wokwi copiar secrets.example.h como secrets.h con claves de laboratorio.
 * Nunca publicar secrets.h ni claves reales.
 */
#include <Arduino.h>
#include <WiFi.h>
#include <PubSubClient.h>
#include <OneWire.h>
#include <DallasTemperature.h>
#include <ArduinoJson.h>
#include <Preferences.h>
#include <mbedtls/md.h>
#include <time.h>
#include <math.h>
#include "secrets.h"
// BEGIN PERSISTENT TELEMETRY SEQUENCE
// Reserve BEFORE use; a reboot burns the unused tail. Never wrap or fall back
// to a RAM-only counter. Erased NVS requires explicit secure reprovisioning.
#include <Preferences.h>
class TelemetrySequence {
  Preferences storage;
  uint32_t current=0, ceiling=0;
  bool initialized=false, healthy=false;
public:
  uint32_t next() {
    if(!initialized) {
      initialized=true;
      healthy=storage.begin("g1txseq",false);
      if(!healthy)return 0;
      current=ceiling=storage.getUInt("reserved",0);
    }
    if(!healthy)return 0;
    if(current==ceiling) {
      if(ceiling>UINT32_MAX-256U) {healthy=false;return 0;}
      uint32_t reserved=ceiling+256U;
      if(storage.putUInt("reserved",reserved)!=sizeof(uint32_t)) {
        healthy=false;return 0;
      }
      ceiling=reserved;
    }
    return ++current;
  }
};
TelemetrySequence telemetrySequence;
// END PERSISTENT TELEMETRY SEQUENCE

constexpr int PIN_GAS=34, PIN_TEMP=15, PIN_BUZZER=16, PIN_RELE=17;
constexpr int ADC_ALARMA=3820, ADC_SALTO_MAX=500;
constexpr float TEMP_ALARMA_C=45.0f;
constexpr uint32_t LECTURA_MS=2000, HEARTBEAT_MS=30000;
constexpr uint32_t WIFI_RETRY_MS=45000, MQTT_RETRY_MS=5000;

const char* TOPIC_DATOS="upc/g1/nodo1/datos";
const char* TOPIC_ALERTA="upc/g1/nodo1/alerta";
const char* TOPIC_ESTADO="upc/g1/nodo1/estado";
const char* TOPIC_CMD="upc/g1/nodo1/cmd";
const char* TOPIC_SEG="upc/g1/seguridad/eventos";

WiFiClient wifi;
PubSubClient mqtt(wifi);
OneWire oneWire(PIN_TEMP);
DallasTemperature tempSensor(&oneWire);
Preferences prefs;

bool nvsOK=false, ventilacionForzada=false, alertaLocal=true;
bool buzzerActivo=false, releActivo=false, primeraMedicion=true;
int gasAnterior=-1, gasADC=0;
float temperaturaC=-127.0f;
String calidadGas="sin_lectura", calidadTemp="sin_lectura";
uint32_t seqDatos=0;
uint64_t ultimoSeqCmd=0;
uint32_t ultimaLectura=0, ultimoWiFi=0, ultimoMQTT=0;
uint32_t ultimoHeartbeat=0, ultimoEvento=0;

String hexDigest(const uint8_t* b, size_t n) {
  const char* hexd="0123456789abcdef";
  String s; s.reserve(n*2);
  for (size_t i=0;i<n;i++) { s+=hexd[b[i]>>4]; s+=hexd[b[i]&15]; }
  return s;
}

String hmacSha256(const String& material, const char* key) {
  if (!key || strlen(key)<24) return "";
  const mbedtls_md_info_t* md=mbedtls_md_info_from_type(MBEDTLS_MD_SHA256);
  uint8_t digest[32];
  if (!md || mbedtls_md_hmac(md, (const unsigned char*)key,strlen(key),
                 (const unsigned char*)material.c_str(),material.length(),digest)!=0) return "";
  return hexDigest(digest,32);
}

bool firmaIgual(const String& a, const String& b) {
  if (a.length()!=64 || b.length()!=64) return false;
  uint8_t diff=0;
  for (int i=0;i<64;i++) diff |= uint8_t(a[i]^b[i]);
  return diff==0;
}

String u64str(uint64_t x) {
  char str[24]; snprintf(str,sizeof(str),"%llu",(unsigned long long)x);
  return String(str);
}

// Canonizacion de telemetria acordable con D3:
// id|ts_millis|seq|sensor|valor_con_2_decimales|unidad|calidad
String materialDato(uint32_t ts, uint32_t seq, const char* sensor,
                    float valor, const char* unidad, const char* calidad) {
  return String("nodo1|")+ts+"|"+seq+"|"+sensor+"|"+
         String(valor,2)+"|"+unidad+"|"+calidad;
}

void publicarDato(const char* topic,const char* sensor,float valor,
                  const char* unidad,const char* calidad,const char* tipo=nullptr) {
  if (!mqtt.connected()) return;
  seqDatos=telemetrySequence.next();
  if(!seqDatos)return;
  uint32_t ts=millis(), seq=seqDatos;
  String firma=hmacSha256(materialDato(ts,seq,sensor,valor,unidad,calidad),HMAC_KEY_NODO1);
  if (firma.length()!=64) {
    Serial.println("[HMAC] FALTA CLAVE NODO1: telemetria no publicada"); return;
  }
  StaticJsonDocument<512> j;
  j["id"]="nodo1"; j["ts"]=ts; j["seq"]=seq; j["sensor"]=sensor;
  j["valor"]=valor; j["unidad"]=unidad; j["calidad"]=calidad;
  if(tipo) j["tipo"]=tipo;
  j["firma"]=firma;
  char out[512]; serializeJson(j,out,sizeof(out));
  mqtt.publish(topic,out);
}

void heartbeat() {
  if (!mqtt.connected()) return;
  StaticJsonDocument<160> j;
  j["id"]="nodo1"; j["estado"]="online"; j["ts"]=millis();
  char out[160]; serializeJson(j,out,sizeof(out));
  mqtt.publish(TOPIC_ESTADO,out,true);
}

void eventoSeguridad(const char* tipo,const char* motivo) {
  uint32_t ahora=millis();
  if (ultimoEvento && ahora-ultimoEvento<2000) return;
  ultimoEvento=ahora;
  Serial.printf("[SEGURIDAD] %s: %s\n",tipo,motivo);
  if(!mqtt.connected()) return;
  StaticJsonDocument<448> j;
  j["hora"]=String("t+")+String(ahora/1000)+"s";
  j["nodo"]="nodo1"; j["ataque"]=tipo;
  j["accion"]="RECHAZADO"; j["motivo"]=motivo; j["ts"]=ahora;
  // El Gateway debe autenticar el evento antes de tomar decisiones.
  j["firma"]=hmacSha256(String("nodo1|")+ahora+"|"+tipo+"|"+motivo,HMAC_KEY_NODO1);
  char out[448]; serializeJson(j,out,sizeof(out));
  mqtt.publish(TOPIC_SEG,out);
}

void aplicarSalidas() {
  const bool ventilar=alertaLocal || ventilacionForzada;
  digitalWrite(PIN_RELE,ventilar?HIGH:LOW);
  if(buzzerActivo!=alertaLocal) {
    if(alertaLocal) tone(PIN_BUZZER,2000);
    else noTone(PIN_BUZZER);
    buzzerActivo=alertaLocal;
  }
  if(releActivo!=ventilar) {
    Serial.printf("[EDGE] Ventilacion %s (alarma=%d, orden=%d)\n",
                  ventilar?"ON":"OFF",alertaLocal,ventilacionForzada);
    releActivo=ventilar;
  }
}

void leerYDecidir() {
  gasADC=analogRead(PIN_GAS);
  temperaturaC=tempSensor.getTempCByIndex(0);
  tempSensor.requestTemperatures(); // Conversion asincrona, evita espera bloqueante

  bool rangoADC=gasADC>10 && gasADC<4090;
  bool salto=gasAnterior>=0 && abs(gasADC-gasAnterior)>ADC_SALTO_MAX;
  bool validoGas=rangoADC && !salto;
  bool validoTemp=isfinite(temperaturaC) &&
      temperaturaC!=DEVICE_DISCONNECTED_C &&
      temperaturaC>=-40.0f && temperaturaC<=125.0f;
  calidadGas=validoGas?"ok":"sospechosa";
  calidadTemp=validoTemp?"ok":"error_sensor";
  if(validoGas) gasAnterior=gasADC;

  // Fallos de sensor y lecturas anormales conservan actuacion preventiva.
  alertaLocal=!validoGas || !validoTemp ||
              gasADC>=ADC_ALARMA || temperaturaC>=TEMP_ALARMA_C;
  aplicarSalidas();
  Serial.printf("[EDGE] Gas ADC=%d [%s], temp=%.2f C [%s], alarma=%d\n",
                gasADC,calidadGas.c_str(),temperaturaC,
                calidadTemp.c_str(),alertaLocal);

  publicarDato(TOPIC_DATOS,"gas_raw",(float)gasADC,"adc",calidadGas.c_str());
  publicarDato(TOPIC_DATOS,"temperatura",temperaturaC,"C",calidadTemp.c_str());
  publicarDato(TOPIC_DATOS,"ventilacion",releActivo?1.0f:0.0f,"estado","ok");
  if(primeraMedicion || alertaLocal) {
    publicarDato(TOPIC_ALERTA,"riesgo_pozas",alertaLocal?1.0f:0.0f,
          "nivel",(validoGas && validoTemp)?"ok":"sospechosa",
          alertaLocal?"ALERTA_POZAS":"NORMAL");
  }
  primeraMedicion=false;
}

// Comando igual al formato de orden HMAC del Nodo 4:
// id=D5, ts=Unix segundos, seq=uint64, cmd, valor (string), firma.
// material: D5|ts|seq|cmd|valor
void recibirMQTT(char* topic,byte* payload,unsigned int len) {
  if(strcmp(topic,TOPIC_CMD)!=0) return;
  if(!len || len>600) {eventoSeguridad("orden_falsa","TAMANO_INVALIDO");return;}
  StaticJsonDocument<640> j;
  if(deserializeJson(j,payload,len)) {
    eventoSeguridad("orden_falsa","JSON_INVALIDO");return;
  }
  if(!j["id"].is<const char*>() || !j["cmd"].is<const char*>() ||
     !j["valor"].is<const char*>() || !j["firma"].is<const char*>() ||
     !j["ts"].is<uint32_t>() ||
     !(j["seq"].is<uint64_t>() || j["seq"].is<uint32_t>())) {
    eventoSeguridad("orden_falsa","CAMPOS_INVALIDOS");return;
  }
  String id=j["id"].as<String>(),cmd=j["cmd"].as<String>();
  String valor=j["valor"].as<String>(),firma=j["firma"].as<String>();
  uint32_t ts=j["ts"].as<uint32_t>();
  uint64_t seq=j["seq"].as<uint64_t>();

  if(id!="D5" ||
     !((cmd=="VENT_MAX"&&valor=="1")||(cmd=="VENT_AUTO"&&valor=="0"))) {
    eventoSeguridad("orden_falsa","COMANDO_O_IDENTIDAD_INVALIDA");return;
  }
  time_t utc=time(nullptr);
  if(utc<1700000000LL || (int64_t)utc-(int64_t)ts>120 ||
     (int64_t)ts-(int64_t)utc>45) {
    eventoSeguridad("orden_falsa","RELOJ_NO_SINCRONIZADO_O_MENSAJE_VENCIDO");return;
  }
  String canonico=id+"|"+String(ts)+"|"+u64str(seq)+"|"+cmd+"|"+valor;
  if(!firmaIgual(firma,hmacSha256(canonico,HMAC_KEY_GATEWAY))) {
    eventoSeguridad("suplantacion","HMAC_INVALIDO");return;
  }
  if(seq==0 || seq<=ultimoSeqCmd || !nvsOK) {
    eventoSeguridad("replay","SEQ_REPETIDO_O_NVS_INDISPONIBLE");return;
  }
  if(prefs.putULong64("lastSeq",seq)==0) {
    eventoSeguridad("orden_falsa","NO_PUDO_GUARDAR_SEQ");return;
  }
  ultimoSeqCmd=seq;
  if(cmd=="VENT_MAX") {
    ventilacionForzada=true;
    prefs.putBool("ventMax",true);
    Serial.println("[MQTT] VENT_MAX firmado ACEPTADO");
  } else {
    if(alertaLocal) {
      eventoSeguridad("orden_peligrosa","VENT_AUTO_BLOQUEADO_POR_RIESGO_LOCAL");
      return;
    }
    ventilacionForzada=false;
    prefs.putBool("ventMax",false);
    Serial.println("[MQTT] VENT_AUTO firmado ACEPTADO");
  }
  aplicarSalidas();
  publicarDato(TOPIC_DATOS,"ventilacion",releActivo?1.0f:0.0f,"estado","ok");
}

void mantenerMQTT() {
  uint32_t ahora=millis();
  if(WiFi.status()!=WL_CONNECTED) {
    if(!ultimoWiFi || ahora-ultimoWiFi>=WIFI_RETRY_MS) {
      ultimoWiFi=ahora;
      if(WiFi.status()!=WL_IDLE_STATUS)WiFi.reconnect();
      Serial.println("[WiFi] Reintento; seguridad Edge sigue activa");
    }
    return;
  }
  if(!mqtt.connected() && (!ultimoMQTT || ahora-ultimoMQTT>=MQTT_RETRY_MS)) {
    ultimoMQTT=ahora;
    String clientId="g1-n1-"+String((uint32_t)esp_random(),HEX);
    const char* lwt="{\"id\":\"nodo1\",\"estado\":\"offline\"}";
    if(mqtt.connect(clientId.c_str(),TOPIC_ESTADO,1,true,lwt)) {
      mqtt.subscribe(TOPIC_CMD,1);
      heartbeat(); ultimoHeartbeat=ahora;
      Serial.println("[MQTT] Conectado; esperando comandos HMAC");
    } else Serial.printf("[MQTT] Error=%d, reintentando luego\n",mqtt.state());
  }
}

void setup() {
  Serial.begin(115200);
  pinMode(PIN_GAS,INPUT);
  pinMode(PIN_RELE,OUTPUT);
  pinMode(PIN_BUZZER,OUTPUT);
  digitalWrite(PIN_RELE,HIGH); // Preventivo durante arranque
  tempSensor.begin();
  tempSensor.setResolution(10);
  tempSensor.setWaitForConversion(false);
  tempSensor.requestTemperatures();
  analogReadResolution(12);
  analogSetPinAttenuation(PIN_GAS,ADC_11db);

  nvsOK=prefs.begin("g1-nodo1",false);
  if(nvsOK) {
    ultimoSeqCmd=prefs.getULong64("lastSeq",0);
    ventilacionForzada=prefs.getBool("ventMax",false);
  }
  aplicarSalidas();

  mqtt.setServer(MQTT_HOST,MQTT_PORT);
  mqtt.setCallback(recibirMQTT);
  if(!mqtt.setBufferSize(1024))Serial.println("[MQTT] ERROR: no se pudo reservar buffer 1024");
  mqtt.setSocketTimeout(1);
  mqtt.setKeepAlive(30);
  WiFi.mode(WIFI_STA);
  WiFi.setAutoReconnect(true);
  WiFi.begin(WIFI_SSID,WIFI_PASSWORD,6);
  ultimoWiFi=millis();
  ultimoWiFi=millis();
  configTime(0,0,"pool.ntp.org","time.google.com");
  Serial.println("NODO1: MQ2 ADC1 + BUZZER + EDGE + HMAC");
}

void loop() {
  uint32_t ahora=millis();
  if(!ultimaLectura || ahora-ultimaLectura>=LECTURA_MS) {
    ultimaLectura=ahora; leerYDecidir();
  }
  mantenerMQTT();
  if(mqtt.connected()) {
    mqtt.loop();
    if(ahora-ultimoHeartbeat>=HEARTBEAT_MS) {
      ultimoHeartbeat=ahora; heartbeat();
    }
  }
  delay(10);
}
