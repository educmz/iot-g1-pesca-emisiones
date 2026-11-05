'use strict';
const http=require('node:http');
const fs=require('node:fs');
const path=require('node:path');
const crypto=require('node:crypto');
const {MiniMqtt}=require('./lib/mqtt-mini');
const {createVerifier,sign}=require('./lib/protocol');

function loadEnv(file){
  if(!fs.existsSync(file))return;
  for(const line of fs.readFileSync(file,'utf8').split(/\r?\n/)){
    const m=line.match(/^\s*([A-Z_][A-Z_0-9]*)\s*=\s*(.*)\s*$/);
    if(m && process.env[m[1]]===undefined)process.env[m[1]]=m[2].replace(/^(\"|')(.*)\1$/,'$2');
  }
}
loadEnv(path.join(__dirname,'.env'));
const SECRET=process.env.HMAC_KEY || 'CAMBIAR_POR_CLAVE_DE_PRUEBA_COMPARTIDA_202620';
const ROOT=process.env.MQTT_ROOT || 'upc/g1/nodo2';
const PORT=Number(process.env.PORT||8080);
const DEMO=process.env.DEMO_MODE==='1';
const ENABLE_GROUP_ATTACKS=process.env.ENABLE_GROUP_ATTACKS==='1';
const ENABLE_GROUP_FEED=process.env.ENABLE_GROUP_FEED==='1';
const GROUP_ATTACK_TOPIC='upc/g1/sim/ataque';
const GROUP_SECURITY_TOPIC='upc/g1/seguridad/eventos';
const VERIFIED_TOPIC=ROOT+'/validado';
let lastAttackMs=0;
const verify=createVerifier(SECRET);
const history=[];
const security=[];
const subscribers=new Set();
let last=null,brokerOnline=false,health='ESPERANDO_DATOS';

function broadcast(event,obj){
  const data=`event: ${event}\ndata: ${JSON.stringify(obj)}\n\n`;
  for(const client of subscribers){try{client.write(data);}catch{subscribers.delete(client);}}
}
function securityEvent(type,action,reason){
  const e={hora:new Date().toISOString(),nodo:'nodo2',tipo:type,accion:action,motivo:reason};
  security.unshift(e);if(security.length>200)security.pop();
  broadcast('security',e);
  if(ENABLE_GROUP_ATTACKS && !DEMO && mqtt.connected) {
    mqtt.publish(GROUP_SECURITY_TOPIC,JSON.stringify({hora:e.hora,nodo:e.nodo,ataque:e.tipo,accion:e.accion,motivo:e.motivo}));
  }
  return e;
}
function acceptMessage(payload,topic){
  const verdict=verify(payload);
  if(!verdict.ok){securityEvent('SUPLANTACION_O_MENSAJE_INVALIDO','RECHAZADO',verdict.reason);return verdict;}
  const m=verdict.message;
  last={...m,recibido:new Date().toISOString(),origen:DEMO?'SINTETICO_DEMO':'MQTT_FIRMADO',tipo:topic.endsWith('/alerta')?'alerta':'datos'};
  if(topic.endsWith('/datos')){
    history.push(last);if(history.length>6000)history.shift();
    if(ENABLE_GROUP_FEED && !DEMO && mqtt.connected){
      // Estos eventos vienen de un verificador local, PERO el broker público no impide suplantar este tópico.
      // La seguridad del gateway debe validar HMAC original y NO confiar solo en el nombre de un tópico.
      for(const [sensor,valor,unidad] of [
        ['temperatura',m.temperatura,'°C'],['humedad',m.humedad,'%'],['gas_relativo',m.gas,'%']
      ]){
        mqtt.publish(VERIFIED_TOPIC,JSON.stringify({id:'nodo2',sensor,valor,unidad,
          calidad:m.calidad,estado:m.estado,ts:m.ts,seq:m.seq,origen:'verificador_local_nodo2'}));
      }
    }
  }
  health=m.estado;
  broadcast('telemetry',last);
  return verdict;
}
const mqtt=new MiniMqtt({
  host:process.env.MQTT_BROKER||'broker.hivemq.com',
  port:Number(process.env.MQTT_PORT||1883),
  clientId:'g1-nodo2-verifier-'+crypto.randomBytes(4).toString('hex'),
  topics:[ROOT+'/datos', ROOT+'/alerta',ROOT+'/estado', ...(ENABLE_GROUP_ATTACKS?[GROUP_ATTACK_TOPIC]:[])]
});
mqtt.on('connection',yes=>{brokerOnline=yes;broadcast('network',{brokerOnline,mode:DEMO?'DEMO':'MQTT'});});
mqtt.on('notice',m=>console.log('[Broker]',m));
mqtt.on('message',(topic,payload)=>{
  if(topic===GROUP_ATTACK_TOPIC){
    // Solo una solicitud de laboratorio; ninguna orden de actuador se acepta desde este tópico.
    if(!ENABLE_GROUP_ATTACKS || DEMO || !mqtt.connected)return;
    let request;try{request=JSON.parse(payload);}catch{return;}
    if(request?.id!=='nodo2' || request?.ataque!=='suplantacion' || request?.origen!=='dashboard_simulacion')return;
    if(Date.now()-lastAttackMs<5000)return; // anti inundación de pruebas
    lastAttackMs=Date.now();
    simulateAttack();
    return;
  }
  if(topic===ROOT+'/estado'){
    // Estado sin firma: informativo, NO se usa para autorizar datos o actuación.
    broadcast('device',{estado:payload,autenticado:false});return;
  }
  if(topic===ROOT+'/datos'||topic===ROOT+'/alerta')acceptMessage(payload,topic);
});

let simTimer=null, demoSeq=0, demoTemp=29, demoGas=18;
const demoBoot='def0ac202620';
function newDemoPayload(){
  demoSeq++;
  const phase=demoSeq%45;
  if(phase>15&&phase<28)demoGas=Math.min(75,demoGas+5);
  else demoGas=Math.max(12,demoGas-4);
  demoTemp=29+Math.sin(demoSeq/7)*3;
  const gasRaw=Math.round(demoGas*4095/100);
  const m={id:'nodo2',boot:demoBoot,ts:Math.floor(Date.now()/1000),seq:demoSeq,
    temperatura:Number(demoTemp.toFixed(2)),humedad:61.0,gas_raw:gasRaw,
    gas:Number((gasRaw*100/4095).toFixed(2)),unidad:'gas_pct_simulado',calidad:'OK',
    estado:demoGas>=60?'ALERTA':demoGas>=30?'ADVERTENCIA':'NORMAL'};
  m.firma=sign(m,SECRET);return m;
}
function simulateAttack(){
  const fake={...newDemoPayload(),gas:99.0,gas_raw:4054,estado:'ALERTA',firma:'0'.repeat(64)};
  // Conserva id='nodo2' y falsifica la lectura sin la clave; pasa por MQTT en modo real.
  // En demo offline atraviesa el mismo verificador, SIN publicar a terceros.
  if(DEMO){acceptMessage(JSON.stringify(fake),ROOT+'/datos');return {published:false,simulatedLocally:true};}
  if(!mqtt.connected)return {error:'BROKER_DESCONECTADO'};
  mqtt.publish(ROOT+'/datos',JSON.stringify(fake));
  return {published:true,simulatedLocally:false};
}
function json(res,data,status=200){
  const body=JSON.stringify(data);
  res.writeHead(status,{'content-type':'application/json; charset=utf-8','cache-control':'no-store','x-content-type-options':'nosniff'});
  res.end(body);
}
function csvEscape(x){const s=String(x??'');return '"'+s.replaceAll('"','""')+'"';}
function exportCsv(){
  const columns=['recibido','id','boot','ts','seq','temperatura','humedad','gas_raw','gas','unidad','calidad','estado'];
  return [columns.join(','),...history.map(r=>columns.map(c=>csvEscape(r[c])).join(','))].join('\n')+'\n';
}
const html=fs.readFileSync(path.join(__dirname,'public/index.html'));
const server=http.createServer((req,res)=>{
  let pathname;
  try{pathname=new URL(req.url,'http://localhost').pathname;}catch{return json(res,{error:'URL_INVALIDA'},400);}
  if(req.method==='GET' && pathname==='/'){
    res.writeHead(200,{'content-type':'text/html; charset=utf-8','cache-control':'no-store','x-content-type-options':'nosniff'});return res.end(html);
  }
  if(req.method==='GET' && pathname==='/api/state')return json(res,{last,brokerOnline,health,mode:DEMO?'DEMO':'MQTT',security:security.slice(0,40),samples:history.length});
  if(req.method==='GET' && pathname==='/api/export.csv'){
    res.writeHead(200,{'content-type':'text/csv; charset=utf-8','content-disposition':'attachment; filename="nodo2-lecturas.csv"'});return res.end(exportCsv());
  }
  if(req.method==='GET' && pathname==='/api/events'){
    res.writeHead(200,{'content-type':'text/event-stream','cache-control':'no-cache, no-transform','connection':'keep-alive'});
    res.write(': conectado\n\n');subscribers.add(res);
    const ping=setInterval(()=>res.write(': ping\n\n'),25000);
    req.on('close',()=>{clearInterval(ping);subscribers.delete(res);});return;
  }
  if(req.method==='POST' && pathname==='/api/demo/attack'){
    // Protección local simple: no habilitar exposición de este endpoint en Internet.
    const origin=req.headers.origin;
    if(origin && !/^http:\/\/(localhost|127\.0\.0\.1)(:\d+)?$/.test(origin))return json(res,{error:'ORIGEN_NO_PERMITIDO'},403);
    const result=simulateAttack();
    return json(res,result,result.error?503:202);
  }
  return json(res,{error:'NO_ENCONTRADO'},404);
});

if(require.main===module){
  server.listen(PORT,'127.0.0.1',()=>{
    console.log(`Panel Nodo 2 en http://127.0.0.1:${PORT}/ - MODO ${DEMO?'DEMO (sintético)':'MQTT en vivo'}`);
  });
  if(!DEMO)mqtt.start();
  else {simTimer=setInterval(()=>acceptMessage(newDemoPayload(),ROOT+'/datos'),2000);}
}
function shutdown(){if(simTimer)clearInterval(simTimer);mqtt.stop();server.close();}
module.exports={server,shutdown,acceptMessage,simulateAttack,newDemoPayload};
