/*
 * ============================================================
 * G1 PESCA | NODO 5: GATEWAY FOG
 * Responsable: Eduardo
 * ============================================================
 * FUNCION:
 * - Centralizar las mediciones de los cuatro nodos ESP32.
 * - Verificar HMAC-SHA256 y secuencias para decidir en Fog.
 * ENTRADAS:
 * - Datos MQTT de los nodos 1, 2, 3 y 4.
 * - RTC DS1307 (GPIO21, 22), boton (GPIO25).
 * SALIDAS:
 * - Pantalla TFT ILI9341, NeoPixel (GPIO32).
 * - Comandos MQTT firmados, respaldo seguro de actuadores.
 * COMUNICACION:
 * - ESP32/WiFi y MQTT en broker de pruebas compartido.
 * - ThingSpeak opcional por HTTPS CA validada, en tarea aparte.
 * SEGURIDAD:
 * - HMAC por nodo, anti-replay y NVS.
 * - Promedio movil y validacion de fuentes independientes.
 * - Ante datos ausentes, no permitir orden OFF insegura.
 * LIMITACIONES:
 * - MQ-2 en Wokwi es simulado; NO es medicion certificada de H2S/NH3.
 * - La estabilidad MQTT aun se investiga. Ver docs/diagnostico-mqtt-2026-11-05.md.
 * - secrets.h contiene claves locales y NO debe versionarse.
 * ============================================================
 */
#include <Arduino.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/queue.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>
#include <Preferences.h>
#include <SPI.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_ILI9341.h>
#include <Adafruit_NeoPixel.h>
#include <RTClib.h>
#include <mbedtls/md.h>
#include <time.h>
#include <math.h>
#include "secrets.h"
#include "fog_logic.h"

constexpr uint8_t TFT_CS=5, TFT_DC=27, TFT_RST=26;
constexpr uint8_t BTN_PIN=25, LED_PIN=32, I2C_SDA=21, I2C_SCL=22;
constexpr uint32_t MQTT_RETRY_MS=15000, WIFI_RETRY_MS=45000;
constexpr uint32_t HEARTBEAT_MS=20000, D5_STATUS_MS=30000;
constexpr uint32_t FOG_MS=1000, SCREEN_MS=1000, SAMPLE_MAX_AGE_MS=12000;
constexpr uint32_t CONFIRM_WAIT_MS=10000, THINGSPEAK_MS=25000;
// Poner false SOLO si se necesita una prueba aislando MQTT de HTTPS.
constexpr bool ENABLE_THINGSPEAK=true;
constexpr uint32_t BUTTON_DEBOUNCE_MS=60, RELEASE_HOLD_MS=2500;
constexpr int SENSOR_HISTORY=3;

const char* MQTT_HOST="broker.emqx.io";
const uint16_t MQTT_PORT=1883;
const char* T_STATUS="upc/g1/nodo5/estado";
const char* T_ALERT="upc/g1/nodo5/alerta";
const char* T_SECURITY="upc/g1/seguridad/eventos";
const char* T_D4_ALERT="upc/g1/nodo4/alerta";

WiFiClient wifi;
PubSubClient mqtt(wifi);
Preferences prefs;
Adafruit_ILI9341 tft(TFT_CS,TFT_DC,TFT_RST);
Adafruit_NeoPixel pixel(1,LED_PIN,NEO_GRB+NEO_KHZ800);
RTC_DS1307 rtc;

// HTTPS usa una tarea separada: la tarea principal puede seguir
// ejecutando mqtt.loop() y verificando los mensajes HMAC.
// La cola almacena una copia de los campos al programar cada envio.
struct ThingSpeakRequest { char body[320]; uint32_t deadline; };
QueueHandle_t thingspeakQueue=nullptr;
bool thingspeakWorkerReady=false;

struct History {
  float samples[SENSOR_HISTORY]={0,0,0};
  uint8_t count=0, cursor=0;
  uint32_t at=0;
  void add(float v) {
    samples[cursor]=v;
    cursor=(cursor+1)%SENSOR_HISTORY;
    if(count<SENSOR_HISTORY)count++;
    at=millis();
  }
  bool fresh() const { return count>0 && (uint32_t)(millis()-at)<=SAMPLE_MAX_AGE_MS; }
  void invalidate() { count=0; at=0; }
  float avg() const {
    if(!count)return 0;
    float sum=0;for(int i=0;i<count;i++)sum+=samples[i];
    return sum/count;
  }
};
History n1Gas,n1Temp,n1Vent,n2Gas,n2Temp,n3Gas,n3Wind,n3Risk,n4Lavador,n4Confirm,n4Intensity;
uint64_t lastSeqByNode[4]={0,0,0,0};
uint64_t nextCommandSeq=0;
uint32_t latestData=0, lastWifi=0, lastMQTT=0,lastHB=0,lastStatus=0;
uint32_t lastFog=0,lastScreen=0,lastThingSpeak=0,lastOnCommand=0;
uint32_t d4UnconfirmedSince=0;
uint32_t lastButtonChange=0,buttonPressedAt=0;
bool prefsOK=false,rtcOK=false,buttonStable=HIGH,buttonRawLast=HIGH,holdHandled=false;
bool acknowledged=false,backupLatched=false;
bool backupPublished=false;
uint32_t backupAttemptAt=0, backupPublishedAt=0;
uint64_t lastSecuritySeq3=0;
uint32_t maxNvsUs=0, maxTftUs=0, maxLoopGapMs=0, lastLoopAt=0, lastDiagnostics=0;
uint32_t nodeReceivedAt[4]={0,0,0,0};
bool backupConfirmed() {
  return backupPublished && n1Vent.fresh() &&
    (int32_t)(n1Vent.at-backupPublishedAt)>=0 && n1Vent.avg()>=0.5f;
}
bool wifiWasConnected=false, mqttWasConnected=false;
int desiredAction=-1,previousLevel=-1;
unsigned long totalVerified=0,totalRejected=0,totalUntrustedEvents=0,totalTrustedSecurity=0;
String lastEvent="ARRANCANDO";
FogDecision current={1,0,false,false,false};
String lastD4EventSignatures[8];
uint8_t lastD4EventCursor=0;

const char* sensorKey(int node) {
  switch(node){
    case 0: return HMAC_KEY_NODO1;
    case 1: return HMAC_KEY_NODO2;
    case 2: return HMAC_KEY_NODO3;
    case 3: return HMAC_KEY_NODO4;
    default:return "";
  }
}
const char* seqStorage(int node) {
  switch(node){case 0:return "s1";case 1:return "s2";case 2:return "s3";default:return "s4";}
}
bool validKey(const char* key) {
  return key && strlen(key)>=24 &&
         strncmp(key,"REEMPLAZAR",9)!=0 &&
         strncmp(key,"CAMBIAR",7)!=0 &&
         strncmp(key,"G1_Gateway_Lab_",15)!=0 &&
         strncmp(key,"G1_Nodo4_Lab_",13)!=0;
}
String hmac256(const String& material,const char* key) {
  if(!validKey(key))return "";
  const mbedtls_md_info_t* info=mbedtls_md_info_from_type(MBEDTLS_MD_SHA256);
  uint8_t hash[32];
  if(!info || mbedtls_md_hmac(info,(const unsigned char*)key,strlen(key),
       (const unsigned char*)material.c_str(),material.length(),hash)!=0)return "";
  const char* hex="0123456789abcdef";
  char out[65];
  for(int i=0;i<32;i++){out[2*i]=hex[hash[i]>>4];out[2*i+1]=hex[hash[i]&15];}
  out[64]=0;
  return String(out);
}
bool sameSignature(const String& a,const String& b) {
  if(a.length()!=64 || b.length()!=64)return false;
  uint8_t diff=0;
  for(int i=0;i<64;i++)diff|=uint8_t(a[i]^b[i]);
  return diff==0;
}
String str64(uint64_t val) {
  char buf[24];
  snprintf(buf,sizeof(buf),"%llu",(unsigned long long)val);
  return String(buf);
}
bool clockReady() { return time(nullptr)>=1700000000LL; }
bool recentUnix(uint32_t ts) {
  time_t now=time(nullptr);
  return now>=1700000000LL &&
         (int64_t)now-(int64_t)ts<=120 &&
         (int64_t)ts-(int64_t)now<=45;
}
int nodeForTopic(const char* topic) {
  if(strncmp(topic,"upc/g1/nodo",11)!=0)return -1;
  int n=topic[11]-'1';
  if(n<0 || n>3 || strcmp(topic+12,"/datos")!=0)return -1;
  return n;
}
bool sensorExpected(int n,const String& s,const String& unit,float v) {
  if(!isfinite(v))return false;
  if(n==0) {
    if(s=="gas_raw")return unit=="adc" && v>=0 && v<=4095;
    if(s=="temperatura")return unit=="C" && v>=-127 && v<=125;
    if(s=="ventilacion")return unit=="estado" && (v==0 || v==1);
  } else if(n==1) {
    if(s=="gas_relativo")return unit=="%" && v>=0 && v<=100;
    if(s=="gas_raw")return unit=="adc" && v>=0 && v<=4095;
    if(s=="temperatura")return unit=="C" && v>=-99 && v<=125;
    if(s=="humedad")return unit=="%" && v>=-99 && v<=100;
    if(s=="estado_secadores")return unit=="nivel" && v>=0 && v<=3;
  } else if(n==2) {
    if(s=="gas")return unit=="ppm" && v>=0 && v<=1000000; // SIMULACION MQ-2
    if(s=="viento")return unit=="grados" && v>=0 && v<=359;
    if(s=="riesgo")return unit=="nivel" && v>=0 && v<=2;
  } else if(n==3) {
    if(s=="intensidad")return unit=="nivel" && v>=1 && v<=3;
    if(s=="lavador"||s=="confirmacion"||s=="modo_seguro"||s=="riesgo_alto")
      return unit=="estado" && (v==0 || v==1);
  }
  return false;
}
bool dataQualityOK(int n,const String& quality) {
  if(n==0 || n==2 || n==3)return quality=="ok";
  return quality=="OK" || quality=="PRUEBA_INYECTADA";
}
// Data received from public broker is NEVER used for Fog before HMAC validation.
void acceptData(const char* topic,byte* bytes,unsigned int length) {
  int n=nodeForTopic(topic);
  if(n<0 || length==0 || length>750)return;
  StaticJsonDocument<768> json;
  if(deserializeJson(json,bytes,length)) {totalRejected++;return;}
  if(!json["id"].is<const char*>() || !json["sensor"].is<const char*>() ||
     !json["unidad"].is<const char*>() || !json["calidad"].is<const char*>() ||
     !json["firma"].is<const char*>() ||
     !json["ts"].is<uint32_t>() ||
     !(json["seq"].is<uint32_t>()||json["seq"].is<uint64_t>()) ||
     !(json["valor"].is<double>()||json["valor"].is<float>()||
       json["valor"].is<int>()||json["valor"].is<long>())) {
    totalRejected++; return;
  }
  String id=json["id"].as<String>(),sensor=json["sensor"].as<String>();
  String unit=json["unidad"].as<String>(),quality=json["calidad"].as<String>();
  const String expectedId=String("nodo")+String(n+1);
  if(id!=expectedId) {totalRejected++;return;}
  uint32_t ts=json["ts"].as<uint32_t>();
  uint64_t seq=json["seq"].as<uint64_t>();
  float val=json["valor"].as<float>();
  if(seq==0 || !sensorExpected(n,sensor,unit,val) ||
     !json["firma"].is<const char*>() || !prefsOK) {
    totalRejected++;return;
  }
  String material=id+"|"+String(ts)+"|"+str64(seq)+"|"+sensor+"|"+
                  String(val,2)+"|"+unit+"|"+quality;
  if(!sameSignature(json["firma"].as<String>(),hmac256(material,sensorKey(n)))) {
    totalRejected++;
    Serial.printf("[HMAC] RECHAZADO %s / %s\n",id.c_str(),sensor.c_str());
    return;
  }
  if(seq<=lastSeqByNode[n]) {
    totalRejected++;
    if(totalRejected<=4 || totalRejected%20==0)
      Serial.printf("[REPLAY] %s received=%s persisted=%s sensor=%s\n",
        id.c_str(),str64(seq).c_str(),str64(lastSeqByNode[n]).c_str(),sensor.c_str());
    return;
  }
  // Write the accepted high-water mark before processing. Fail closed on NVS failure.
  uint32_t nvsStarted=micros();
  size_t written=prefs.putULong64(seqStorage(n),seq);
  uint32_t nvsElapsed=micros()-nvsStarted;
  if(nvsElapsed>maxNvsUs)maxNvsUs=nvsElapsed;
  if(written!=sizeof(uint64_t)) {totalRejected++;return;}
  nodeReceivedAt[n]=millis();
  lastSeqByNode[n]=seq; totalVerified++;
  if(totalVerified<=4 || (totalVerified%20)==0) {
    Serial.printf("[HMAC] ACEPTADO %s / %s seq=%s total=%lu\n",
        id.c_str(),sensor.c_str(),str64(seq).c_str(),totalVerified);
  }
  latestData=millis();

  // Invalid signed quality cancels any older apparently fresh sample.
  if(!dataQualityOK(n,quality)) {
    if(n==0 && sensor=="gas_raw")n1Gas.invalidate();
    if(n==0 && sensor=="temperatura")n1Temp.invalidate();
    if(n==0 && sensor=="ventilacion")n1Vent.invalidate();
    if(n==1 && sensor=="gas_relativo")n2Gas.invalidate();
    if(n==1 && sensor=="temperatura")n2Temp.invalidate();
    if(n==2 && sensor=="gas")n3Gas.invalidate();
    if(n==2 && sensor=="viento")n3Wind.invalidate();
    if(n==2 && sensor=="riesgo")n3Risk.invalidate();
    return;
  }
  if(n==0 && sensor=="gas_raw")n1Gas.add(val);
  if(n==0 && sensor=="temperatura")n1Temp.add(val);
  if(n==0 && sensor=="ventilacion")n1Vent.add(val);
  if(n==1 && sensor=="gas_relativo")n2Gas.add(val);
  if(n==1 && sensor=="temperatura")n2Temp.add(val);
  if(n==2 && sensor=="gas")n3Gas.add(val);
  if(n==2 && sensor=="viento")n3Wind.add(val);
  if(n==2 && sensor=="riesgo")n3Risk.add(val);
  if(n==3 && sensor=="lavador")n4Lavador.add(val);
  if(n==3 && sensor=="confirmacion")n4Confirm.add(val);
  if(n==3 && sensor=="intensidad")n4Intensity.add(val);
}

// Nodo 4 emits a separate signed event schema (D4, UNIX time, signed tipo/motivo).
// Do NOT interpret arbitrary public /alerta or /seguridad/eventos as an action request.
bool verifyD4Event(JsonDocument& j) {
  if(String(j["id"]|"")!="D4" || !j["ts"].is<uint32_t>() ||
     !j["tipo"].is<const char*>() || !j["motivo"].is<const char*>() ||
     !j["firma"].is<const char*>() ||
     !(j["seq"].is<uint32_t>()||j["seq"].is<uint64_t>()))return false;
  uint32_t ts=j["ts"].as<uint32_t>();
  if(!recentUnix(ts) || String(j["valor"]|"")!=String(j["tipo"]|"") ||
     String(j["unidad"]|"")!="evento" || String(j["calidad"]|"")!="VALIDO")return false;
  String typ=j["tipo"].as<String>(),why=j["motivo"].as<String>();
  uint64_t seq=j["seq"].as<uint64_t>();
  String material=String("D4|")+ts+"|"+typ+"|evento|VALIDO|"+typ+"|"+why+"|"+str64(seq);
  String signature=j["firma"].as<String>();
  if(!sameSignature(signature,hmac256(material,HMAC_KEY_NODO4)))return false;
  for(const auto& seen:lastD4EventSignatures)if(seen==signature)return false;
  lastD4EventSignatures[lastD4EventCursor]=signature;
  lastD4EventCursor=(lastD4EventCursor+1)%8;
  return true;
}

bool backupVentilation(const char* cause);
void acceptD4Alert(byte* bytes,unsigned int length) {
  if(!length||length>750)return;
  StaticJsonDocument<768> j;
  if(deserializeJson(j,bytes,length) || !verifyD4Event(j)) {
    totalRejected++;Serial.println("[D4] Evento no verificable, ignorado");return;
  }
  String type=j["tipo"].as<String>();
  if(type=="FALLA_LAVADOR") {
    lastEvent="FALLA_LAVADOR VERIFICADA";
    backupVentilation("FALLA_LAVADOR");
  } else if(type=="CONFIRMACION_OK") {
    lastEvent="D4 confirmacion firmada";
  } else if(type=="HMAC_INVALIDA"||type=="REPLAY_BLOQUEADO") {
    lastEvent=type;
  }
}
void acceptSecurityEvent(byte* bytes,unsigned int length) {
  if(length==0||length>750)return;
  StaticJsonDocument<768> j;
  if(deserializeJson(j,bytes,length)) {totalUntrustedEvents++;return;}
  if(!j["nodo"].is<const char*>() || !j["ataque"].is<const char*>() ||
     !j["motivo"].is<const char*>() || !j["ts"].is<uint32_t>() ||
     !j["firma"].is<const char*>() || String(j["accion"]|"")!="RECHAZADO") {
    totalUntrustedEvents++;return;
  }
  if(String(j["schema"]|"")=="SEC1" && String(j["nodo"]|"")=="nodo3") {
    if(!prefsOK || !j["seq"].is<uint32_t>() || !recentUnix(j["ts"].as<uint32_t>())) {
      totalUntrustedEvents++;return;
    }
    uint32_t sequence=j["seq"].as<uint32_t>();
    String material=String("SEC1|nodo3|")+j["ts"].as<uint32_t>()+"|"+sequence+"|"+
      j["ataque"].as<String>()+"|RECHAZADO|"+j["motivo"].as<String>();
    if(!sequence || sequence<=lastSecuritySeq3 ||
       !sameSignature(j["firma"].as<String>(),hmac256(material,HMAC_KEY_NODO3)) ||
       prefs.putULong64("sec3",sequence)!=sizeof(uint64_t)) {
      totalUntrustedEvents++;return;
    }
    lastSecuritySeq3=sequence;totalTrustedSecurity++;
    Serial.println("[SEGURIDAD VERIFICADA] nodo3 SEC1");
    return;
  }
  String node=j["nodo"].as<String>(),type=j["ataque"].as<String>();
  String reason=j["motivo"].as<String>(),signature=j["firma"].as<String>();
  uint32_t ts=j["ts"].as<uint32_t>();
  String material,keyType;
  const char* key="";
  if(node=="nodo1") {
    material=String("nodo1|")+ts+"|"+type+"|"+reason;
    key=HMAC_KEY_NODO1;
  } else if(node=="nodo4" &&
            (j["seq_rechazado"].is<uint64_t>()||j["seq_rechazado"].is<uint32_t>()) &&
            recentUnix(ts)) {
    uint64_t seq=j["seq_rechazado"].as<uint64_t>();
    material=String("D4|")+ts+"|"+type+"|evento|VALIDO|"+type+"|"+reason+"|"+str64(seq);
    key=HMAC_KEY_NODO4;
  } else {
    totalUntrustedEvents++;return; // D2 and D3 currently lack common signed event schema.
  }
  if(sameSignature(signature,hmac256(material,key))) {
    totalTrustedSecurity++;
    Serial.printf("[SEGURIDAD VERIFICADA] %s: %s\n",node.c_str(),type.c_str());
  } else totalUntrustedEvents++;
}
void onMQTT(char* topic,byte* payload,unsigned int length) {
  static uint32_t recibidos=0;
  recibidos++;
  if(recibidos<=10 || recibidos%20==0) {
    Serial.printf("[MQTT RX] Topico=%s bytes=%u total=%lu\n",
                  topic,length,(unsigned long)recibidos);
  }
  if(strcmp(topic,T_D4_ALERT)==0) {acceptD4Alert(payload,length);return;}
  if(strcmp(topic,T_SECURITY)==0) {acceptSecurityEvent(payload,length);return;}
  if(nodeForTopic(topic)>=0)acceptData(topic,payload,length);
}
FogInputs fogInputs() {
  return {n1Gas.fresh(),n2Gas.fresh(),n3Gas.fresh(),n3Wind.fresh(),n3Risk.fresh(),
          n1Gas.avg(),n2Gas.avg(),n3Gas.avg(),n3Wind.avg(),(int)roundf(n3Risk.avg()),
          n1Temp.fresh(),n2Temp.fresh(),n1Temp.avg(),n2Temp.avg()};
}
bool publishJson(const char* topic,JsonDocument& j,bool retain=false) {
  if(!mqtt.connected())return false;
  char payload[700];
  size_t n=serializeJson(j,payload,sizeof(payload));
  return n>0 && n<sizeof(payload)-1 && mqtt.publish(topic,payload,retain);
}
bool sendSigned(int dest,const char* cmd,const char* valor) {
  if(!mqtt.connected() || !clockReady() || !prefsOK ||
     !validKey(HMAC_KEY_GATEWAY))return false;
  uint32_t ts=(uint32_t)time(nullptr);
  uint64_t epochMillis=(uint64_t)ts*1000ULL;
  uint64_t seq=(nextCommandSeq>=epochMillis)?nextCommandSeq+1:epochMillis;
  // Persist BEFORE publish; one increasing counter across all destination nodes.
  if(prefs.putULong64("command",seq)==0)return false;
  nextCommandSeq=seq;
  String material=String("D5|")+ts+"|"+str64(seq)+"|"+cmd+"|"+valor;
  String signature=hmac256(material,HMAC_KEY_GATEWAY);
  if(signature.length()!=64)return false;
  StaticJsonDocument<512> j;
  j["id"]="D5";j["ts"]=ts;j["seq"]=seq;j["cmd"]=cmd;
  j["valor"]=valor;j["firma"]=signature;
  String topic=String("upc/g1/nodo")+dest+"/cmd";
  bool sent=publishJson(topic.c_str(),j);
  Serial.printf("[D5] %s -> nodo%d (%s) seq=%s\n",cmd,dest,
                sent?"MQTT enviado":"sin enviar",str64(seq).c_str());
  return sent;
}
bool backupVentilation(const char* cause) {
  if(!backupLatched) {
    backupLatched=true;
    if(prefsOK)prefs.putBool("backup",true);
  }
  lastEvent=String("VENT_MAX: ")+cause;
  if(backupConfirmed())return true;
  const uint32_t now=millis();
  if(backupAttemptAt && now-backupAttemptAt<5000UL)return false;
  backupAttemptAt=now;
  bool published=sendSigned(1,"VENT_MAX","1");
  if(published) {backupPublished=true;backupPublishedAt=millis();}
  Serial.printf("[RESPALDO] %s => VENT_MAX %s\n",cause,published?"enviado":"pendiente");
  return published;
}
void publishStatus() {
  StaticJsonDocument<272> j;
  j["id"]="nodo5"; j["ts"]=millis(); j["estado"]="online";
  j["fase"]="fog_hmac";
  j["riesgo"]=current.level;
  j["verificados"]=totalVerified;
  publishJson(T_STATUS,j,true);
}
void sendHeartbeats() {
  // N3 specifically requires valor="1"; N4 uses empty valor.
  sendSigned(3,"HB","1");
  sendSigned(4,"HB","");
}
void logFogChange(const FogDecision& d) {
  Serial.printf("[FOG] Nivel=%d fuentes=%d vientoComunidad=%d liberar=%d\n",
                d.level,d.elevatedSources,d.windToCommunity,d.safeToStop);
  StaticJsonDocument<384> event;
  event["id"]="nodo5";event["ts"]=millis();
  event["nivel"]=d.level;event["fuentes"]=d.elevatedSources;
  event["tipo"]="CAMBIO_RIESGO";
  String material=String("nodo5|")+String(millis())+"|"+d.level+"|"+d.elevatedSources;
  // Informational; consumers must not confuse this with telemetry signed by a sensor.
  event["nota"]="Decision basada en lecturas HMAC por sensor, sin ppm certificados";
  publishJson(T_ALERT,event);
}
void processFog(uint32_t now) {
  FogDecision d=evaluateFog(fogInputs());
  current=d;
  if(previousLevel!=d.level) {
    previousLevel=d.level;
    acknowledged=false;
    logFogChange(d);
  }
  if(!mqtt.connected() || !clockReady())return;
  int wanted=d.requireLavador?1:(d.safeToStop&&!backupLatched?0:-1);
  // No orden OFF when there are missing/stale sources or latched emergency.
  if(wanted==desiredAction)return;
  if(wanted==1) {
    if(!sendSigned(4,"RIESGO_ON",""))return;
    if(!sendSigned(4,"MAX",""))return;
    if(!sendSigned(4,"ON",""))return;
    lastOnCommand=now;
    desiredAction=1;
    lastEvent="D4 ON por corroboracion";
  } else if(wanted==0) {
    if(!sendSigned(4,"RIESGO_OFF",""))return;
    if(!sendSigned(4,"OFF",""))return;
    desiredAction=0;
    lastOnCommand=0;
    lastEvent="Riesgo normal, orden D4 OFF";
  }
}
void checkConfirmation(uint32_t now) {
  // D4 can auto-start its lavador BEFORE the Gateway sends an ON command.
  // Track a physically unconfirmed ON using authenticated sensor readings.
  bool on=n4Lavador.fresh() && n4Lavador.avg()>=0.5f;
  bool confirmed=n4Confirm.fresh() && n4Confirm.avg()>=0.5f;
  if(on && n4Confirm.fresh() && !confirmed) {
    if(!d4UnconfirmedSince)d4UnconfirmedSince=now;
    if(!backupLatched && now-d4UnconfirmedSince>=CONFIRM_WAIT_MS)
      backupVentilation("D4_ENCENDIDO_SIN_CONF_10S");
  } else d4UnconfirmedSince=0;
  if(!lastOnCommand || backupLatched)return;
  if((uint32_t)(now-lastOnCommand)<CONFIRM_WAIT_MS)return;
  bool recentlyConfirmed=confirmed && n4Confirm.at>=lastOnCommand;
  if(!recentlyConfirmed)backupVentilation("D4_SIN_CONFIRMACION_10S");
}
void handleButton(uint32_t now) {
  bool raw=digitalRead(BTN_PIN);
  if(raw!=buttonRawLast)lastButtonChange=now;
  if(now-lastButtonChange>BUTTON_DEBOUNCE_MS && raw!=buttonStable) {
    buttonStable=raw;
    if(buttonStable==LOW) {buttonPressedAt=now;holdHandled=false;}
    else if(!holdHandled) {acknowledged=true;Serial.println("[BOTON] Alerta reconocida");}
  }
  buttonRawLast=raw;
  if(buttonStable==LOW && !holdHandled &&
     now-buttonPressedAt>=RELEASE_HOLD_MS) {
    holdHandled=true;
    if(backupLatched && current.safeToStop &&
       n4Confirm.fresh() && n4Confirm.avg()>=0.5f &&
       sendSigned(1,"VENT_AUTO","0")) {
      backupLatched=false;backupPublished=false;backupAttemptAt=0;
      if(prefsOK)prefs.putBool("backup",false);
      lastEvent="Respaldo liberado con botón";
      desiredAction=-1;
    } else {
      Serial.println("[BOTON] Sin condiciones para liberar VENT_MAX");
    }
  }
}
void drawLed() {
  uint32_t color=current.level==0?pixel.Color(0,140,0):
      current.level==1?pixel.Color(160,100,0):pixel.Color(180,0,0);
  if(backupLatched)color=pixel.Color(150,0,150);
  pixel.setPixelColor(0,color);pixel.show();
}
void drawScreen() {
  tft.fillScreen(ILI9341_BLACK);
  tft.fillRect(0,0,320,30,ILI9341_NAVY);
  tft.setTextSize(2);tft.setTextColor(ILI9341_WHITE);
  tft.setCursor(9,7);tft.print("RESQ  |  FOG D5");
  tft.setCursor(8,38);
  tft.setTextColor(current.level==0?ILI9341_GREEN:
                   current.level==1?ILI9341_YELLOW:ILI9341_RED);
  tft.print(current.level==0?"NORMAL":current.level==1?"ALERTA":"CRITICO");
  tft.setTextSize(1);tft.setTextColor(ILI9341_WHITE);
  tft.setCursor(8,65);
  tft.print("WiFi:");tft.print(WiFi.status()==WL_CONNECTED?"OK":"OFF");
  tft.print(" MQTT:");tft.print(mqtt.connected()?"OK":"OFF");
  tft.print(" HMAC:");tft.print(totalVerified);
  tft.setCursor(8,82);tft.print("N1 ADC: ");
  if(n1Gas.fresh())tft.print(n1Gas.avg(),0);else tft.print("--");
  tft.setCursor(8,99);tft.print("N2 gas relativo: ");
  if(n2Gas.fresh()){tft.print(n2Gas.avg(),1);tft.print("%");}else tft.print("--");
  tft.setCursor(8,116);tft.print("N3 gas SIM: ");
  if(n3Gas.fresh()){tft.print(n3Gas.avg(),0);tft.print(" ppm");}else tft.print("--");
  tft.setCursor(8,133);tft.print("Viento: ");
  if(n3Wind.fresh()){tft.print(n3Wind.avg(),0);tft.print(" grados");}else tft.print("--");
  tft.setCursor(8,150);tft.print("D4 lavador / confirmacion: ");
  tft.print(n4Lavador.fresh()?(n4Lavador.avg()>0.5?"ON":"OFF"):"--");
  tft.print("/");
  tft.print(n4Confirm.fresh()?(n4Confirm.avg()>0.5?"SI":"NO"):"--");
  tft.setCursor(8,168);tft.setTextColor(backupLatched?ILI9341_RED:ILI9341_WHITE);
  if(backupLatched) {
    tft.print(backupConfirmed()?"VENT_MAX N1 CONFIRMADO":backupPublished?"VENT_MAX ESPERA ACTUACION":"VENT_MAX N1 PENDIENTE");
  } else tft.print("Respaldo: no activo");
  tft.setTextColor(ILI9341_WHITE);
  tft.setCursor(8,187);
  tft.print("Rechazados: ");tft.print(totalRejected);
  tft.print("  Seguridad: ");tft.print(totalTrustedSecurity);
  tft.setCursor(8,205);
  if(rtcOK) {
    DateTime d=rtc.now();char b[28];
    snprintf(b,sizeof(b),"%02d/%02d/%04d %02d:%02d:%02d",
             d.day(),d.month(),d.year(),d.hour(),d.minute(),d.second());
    tft.print(b);
  } else tft.print("RTC no disponible");
  tft.setCursor(8,222);
  tft.print(acknowledged?"Reconocida":"Corto: reconocer  Largo: liberar");
  drawLed();
}
void handleNetwork(uint32_t now) {
  wl_status_t wifiStatus=WiFi.status();
  if(wifiStatus!=WL_CONNECTED) {
    if(wifiWasConnected)Serial.println("[WiFi] Desconectado; esperando reconexion");
    wifiWasConnected=false;
    mqttWasConnected=false;
    // WiFi.begin() was started in setup(). Do not immediately restart an
    // ongoing association; this caused unstable repeated attempts in Wokwi.
    if((uint32_t)(now-lastWifi)>=WIFI_RETRY_MS) {
      lastWifi=now;
      Serial.printf("[WiFi] Esperando Wokwi-GUEST, estado=%d; reintentando\n",
                    int(wifiStatus));
      if(wifiStatus==WL_DISCONNECTED || wifiStatus==WL_CONNECTION_LOST ||
         wifiStatus==WL_CONNECT_FAILED || wifiStatus==WL_NO_SSID_AVAIL) {
        WiFi.reconnect();
      }
    }
    return;
  }
  if(!wifiWasConnected) {
    wifiWasConnected=true;
    Serial.print("[WiFi] Conectado, IP=");
    Serial.println(WiFi.localIP());
  }
  if(mqtt.connected()) {
    mqttWasConnected=true;
    return;
  }
  if(mqttWasConnected) {
    mqttWasConnected=false;
    Serial.printf("[MQTT] Conexion perdida: state=%d WiFi=%d uptime=%lu s; reintentando\n",
                  mqtt.state(),int(WiFi.status()),(unsigned long)(millis()/1000));
  }
  if(lastMQTT && (uint32_t)(now-lastMQTT)<MQTT_RETRY_MS)return;
  lastMQTT=now;
  Serial.printf("[MQTT] Conectando a %s:%u...\n",MQTT_HOST, MQTT_PORT);
  String cid="g1-d5-"+String((uint32_t)esp_random(),HEX);
  if(mqtt.connect(cid.c_str(),T_STATUS,1,true,
        "{\"id\":\"nodo5\",\"estado\":\"offline\"}")) {
    bool subDatos=mqtt.subscribe("upc/g1/+/datos");
    bool subD4=mqtt.subscribe(T_D4_ALERT);
    bool subSeg=mqtt.subscribe(T_SECURITY);
    mqttWasConnected=true;
    desiredAction=-1;lastHB=0;lastStatus=0;
    Serial.printf("[MQTT] Conectado y suscrito: datos=%d D4=%d seg=%d\n",
                  int(subDatos),int(subD4),int(subSeg));
    publishStatus();
    sendHeartbeats();
    if(backupLatched)backupVentilation("RESTAURAR_LATCHEO_NVS");
  } else {
    Serial.printf("[MQTT] fallo rc=%d; siguiente intento en %lu s\n",
                  mqtt.state(),(unsigned long)(MQTT_RETRY_MS/1000));
  }
}
// La comunicacion HTTPS transcurre en otra tarea para que no bloquee
// el loop MQTT. No se comparte WiFiClient de MQTT con el HTTPClient.
void thingspeakWorker(void* parameter) {
  ThingSpeakRequest req;
  for(;;) {
    if(xQueueReceive(thingspeakQueue,&req,portMAX_DELAY)!=pdTRUE)continue;
    if(WiFi.status()!=WL_CONNECTED || (int32_t)(millis()-req.deadline)>0) {
      Serial.println("[ThingSpeak] OMITIDO: sin WiFi o captura caducada");
      memset(req.body,0,sizeof(req.body));
      continue;
    }
    WiFiClientSecure secure;
    secure.setCACert(THINGSPEAK_ROOT_CA); // Nunca desactivar TLS.
    secure.setTimeout(5000);
    HTTPClient http;
    http.setTimeout(5000);
    if(!http.begin(secure,"https://api.thingspeak.com/update")) {
      Serial.println("[ThingSpeak] ERROR: inicializacion HTTPS");
      memset(req.body,0,sizeof(req.body));
      continue;
    }
    http.addHeader("Content-Type","application/x-www-form-urlencoded");
    Serial.println("[ThingSpeak] Enviando HTTPS en tarea independiente");
    uint32_t started=millis();
    int status=http.POST((uint8_t*)req.body,strlen(req.body));
    Serial.printf("[HTTPS PERF] elapsed_ms=%lu stack_free=%lu heap=%lu\n",
      (unsigned long)(millis()-started),(unsigned long)uxTaskGetStackHighWaterMark(nullptr),
      (unsigned long)ESP.getFreeHeap());
    if(status>0) {
      String respuesta=http.getString();
      respuesta.trim();
      long entrada=respuesta.toInt();
      if(status==200 && entrada>0) {
        Serial.printf("[ThingSpeak] GUARDADO HTTP 200 entry_id=%ld\n",entrada);
      } else {
        Serial.printf("[ThingSpeak] NO GUARDADO HTTP %d respuesta=%s\n",
                      status,respuesta.substring(0,64).c_str());
      }
    } else {
      Serial.printf("[ThingSpeak] FALLO HTTPS codigo=%d detalle=%s\n",
                    status,http.errorToString(status).c_str());
    }
    http.end();
    // El buffer req contiene la Write API Key: limpiarlo despues de usarlo.
    memset(req.body,0,sizeof(req.body));
  }
}

// Captura solo lecturas frescas/HMAC verificadas.
// La tarea MQTT nunca espera la respuesta de ThingSpeak.
void sendThingSpeak(uint32_t now) {
  if(!ENABLE_THINGSPEAK)return;
  if((uint32_t)(now-lastThingSpeak)<THINGSPEAK_MS)return;
  lastThingSpeak=now;

  if(WiFi.status()!=WL_CONNECTED) {
    Serial.println("[ThingSpeak] OMITIDO: WiFi desconectado");return;
  }
  if(!clockReady()) {
    Serial.println("[ThingSpeak] OMITIDO: reloj UTC pendiente");return;
  }
  if(strlen(THINGSPEAK_WRITE_API_KEY)<12 ||
     strncmp(THINGSPEAK_WRITE_API_KEY,"REEMPLAZAR",9)==0) {
    Serial.println("[ThingSpeak] OMITIDO: falta Write API Key");return;
  }
  if(strlen(THINGSPEAK_ROOT_CA)<200 ||
     strstr(THINGSPEAK_ROOT_CA,"-----BEGIN CERTIFICATE-----")==nullptr ||
     strstr(THINGSPEAK_ROOT_CA,"-----END CERTIFICATE-----")==nullptr) {
    Serial.println("[ThingSpeak] OMITIDO: falta certificado CA PEM");return;
  }
  if(!thingspeakWorkerReady || !thingspeakQueue) {
    Serial.println("[ThingSpeak] ERROR: tarea HTTPS no disponible");return;
  }
  String body=String("api_key=")+THINGSPEAK_WRITE_API_KEY+
    "&field1="+String(current.level)+
    "&field8="+String(backupLatched?1:0);
  int fields=2;
  if(n1Gas.fresh()) {body+="&field2="+String(n1Gas.avg(),0);fields++;}
  if(n2Gas.fresh()) {body+="&field3="+String(n2Gas.avg(),1);fields++;}
  if(n3Gas.fresh()) {body+="&field4="+String(n3Gas.avg(),0);fields++;}
  if(n3Wind.fresh()) {body+="&field5="+String(n3Wind.avg(),0);fields++;}
  if(n4Lavador.fresh()) {body+="&field6="+String(n4Lavador.avg(),0);fields++;}
  if(n4Confirm.fresh()) {body+="&field7="+String(n4Confirm.avg(),0);fields++;}
  ThingSpeakRequest req={};
  if(body.length()>=sizeof(req.body)) {
    Serial.println("[ThingSpeak] ERROR: datos demasiado largos");return;
  }
  req.deadline=millis()+SAMPLE_MAX_AGE_MS;
  const History* sources[]={&n1Gas,&n2Gas,&n3Gas,&n3Wind,&n4Lavador,&n4Confirm};
  for(const History* source:sources)if(source->fresh()) {
    uint32_t deadline=source->at+SAMPLE_MAX_AGE_MS;
    if((int32_t)(deadline-req.deadline)<0)req.deadline=deadline;
  }
  body.toCharArray(req.body,sizeof(req.body));
  if(xQueueOverwrite(thingspeakQueue,&req)==pdPASS) {
    Serial.printf("[ThingSpeak] Preparado: %d campos (WiFi=OK MQTT=%s)\n",
                  fields,mqtt.connected()?"OK":"OFF");
  } else {
    Serial.println("[ThingSpeak] ERROR: no se pudo encolar HTTPS");
  }
  memset(req.body,0,sizeof(req.body));
}
void setup() {
  Serial.begin(115200);
  pinMode(BTN_PIN,INPUT_PULLUP);
  Wire.begin(I2C_SDA,I2C_SCL);
  SPI.begin(18,19,23,TFT_CS);
  pixel.begin();pixel.setBrightness(80);pixel.clear();pixel.show();
  tft.begin();tft.setRotation(1);
  rtcOK=rtc.begin();

  prefsOK=prefs.begin("g1n5fog",false);
  if(prefsOK) {
    for(int i=0;i<4;i++)lastSeqByNode[i]=prefs.getULong64(seqStorage(i),0);
    lastSecuritySeq3=prefs.getULong64("sec3",0);
    nextCommandSeq=prefs.getULong64("command",0);
    backupLatched=prefs.getBool("backup",false);
  } else Serial.println("[SEGURIDAD] NVS no disponible; firma de datos bloqueada");
  for(int i=0;i<4;i++)if(!validKey(sensorKey(i)))
    Serial.printf("[CLAVES] Pendiente clave HMAC del Nodo %d\n",i+1);
  if(!validKey(HMAC_KEY_GATEWAY))
    Serial.println("[CLAVES] Faltan claves privadas D5 para emitir comandos");
  mqtt.setServer(MQTT_HOST,MQTT_PORT);
  mqtt.setCallback(onMQTT);
  if(!mqtt.setBufferSize(1024))Serial.println("[MQTT] ERROR: no se pudo reservar buffer 1024");
  // Pings mas frecuentes; mayor tolerancia a lecturas TCP lentas en Wokwi.
  mqtt.setKeepAlive(20);
  mqtt.setSocketTimeout(8);
  WiFi.mode(WIFI_STA);
  WiFi.setAutoReconnect(true);
  WiFi.begin(WIFI_SSID,WIFI_PASSWORD,6);
  lastWifi=millis();
  Serial.println("[WiFi] Buscando Wokwi-GUEST sin reiniciar el intento");
  configTime(0,0,"pool.ntp.org","time.google.com");
  if(ENABLE_THINGSPEAK) {
    thingspeakQueue=xQueueCreate(1,sizeof(ThingSpeakRequest));
    if(thingspeakQueue) {
      BaseType_t started=xTaskCreatePinnedToCore(
        thingspeakWorker,"ThingSpeakHTTPS",12288,nullptr,1,nullptr,0);
      thingspeakWorkerReady=(started==pdPASS);
    }
    Serial.printf("[ThingSpeak] Tarea HTTPS independiente: %s\n",
                  thingspeakWorkerReady?"OK":"ERROR");
  }
  Serial.println("[D5] Fog seguro: espera telemetria firmada; no usa datos falsos");
  drawScreen();
}
void loop() {
  uint32_t now=millis();
  if(lastLoopAt && now-lastLoopAt>maxLoopGapMs)maxLoopGapMs=now-lastLoopAt;
  lastLoopAt=now;
  handleNetwork(now);
  if(backupLatched && !backupConfirmed())backupVentilation("REINTENTO_PENDIENTE");
  if(mqtt.connected()) {
    bool mqttLoopOK=mqtt.loop();
    if(!mqttLoopOK) {
      Serial.printf("[MQTT] loop fallo state=%d WiFi=%d\n",
                    mqtt.state(),int(WiFi.status()));
    }
    if(mqtt.connected() && (!lastHB || now-lastHB>=HEARTBEAT_MS)) {
      lastHB=now;sendHeartbeats();
    }
    if(mqtt.connected() && (!lastStatus || now-lastStatus>=D5_STATUS_MS)) {
      lastStatus=now;publishStatus();
    }
  }
  if(!lastFog || now-lastFog>=FOG_MS) {
    lastFog=now;processFog(now);
    checkConfirmation(now);
  }
  handleButton(now);
  if(!lastScreen || now-lastScreen>=SCREEN_MS) {
    lastScreen=now;
    uint32_t started=micros();drawScreen();
    uint32_t elapsed=micros()-started;
    if(elapsed>maxTftUs)maxTftUs=elapsed;
  }
  sendThingSpeak(now);
  if(now-lastDiagnostics>=30000UL) {
    lastDiagnostics=now;
    Serial.printf("[PERF] loop_gap_ms=%lu nvs_us=%lu tft_us=%lu heap=%lu\n",
      (unsigned long)maxLoopGapMs,(unsigned long)maxNvsUs,(unsigned long)maxTftUs,
      (unsigned long)ESP.getFreeHeap());
    for(int n=0;n<4;n++)Serial.printf("[RX AGE] n%d received=%d age_ms=%lu seq=%s\n",
      n+1,nodeReceivedAt[n]!=0,(unsigned long)(now-nodeReceivedAt[n]),str64(lastSeqByNode[n]).c_str());
    maxLoopGapMs=maxNvsUs=maxTftUs=0;
  }
  delay(10);
}
