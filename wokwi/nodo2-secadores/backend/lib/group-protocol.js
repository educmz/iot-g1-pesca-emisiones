'use strict';
const crypto=require('node:crypto');
const SENSOR_UNITS=Object.freeze({gas_relativo:'%',gas_raw:'adc',temperatura:'C',humedad:'%',estado_secadores:'nivel'});
const QUALITIES=new Set(['OK','PRUEBA_INYECTADA','SENSOR_ERROR']);
function canonicalGroup(m){
  return [m.id,String(m.ts),String(m.seq),m.sensor,Number(m.valor).toFixed(2),m.unidad,m.calidad].join('|');
}
function signGroup(m,key){
  return crypto.createHmac('sha256',key).update(canonicalGroup(m),'utf8').digest('hex');
}
function checkGroup(m){
  if(!m||typeof m!=='object'||Array.isArray(m))return 'JSON_NO_VALIDO';
  if(m.id!=='nodo2')return 'IDENTIFICADOR_INVALIDO';
  if(!Number.isSafeInteger(m.ts)||m.ts<0||!Number.isSafeInteger(m.seq)||m.seq<1)return 'TS_SEQ_INVALIDO';
  if(!Object.hasOwn(SENSOR_UNITS,m.sensor)||SENSOR_UNITS[m.sensor]!==m.unidad)return 'SENSOR_UNIDAD_INVALIDO';
  if(typeof m.valor!=='number'||!Number.isFinite(m.valor))return 'VALOR_INVALIDO';
  if(!QUALITIES.has(m.calidad))return 'CALIDAD_INVALIDA';
  if(m.sensor==='gas_relativo'&&(m.valor<0||m.valor>100))return 'GAS_INVALIDO';
  if(m.sensor==='gas_raw'&&(m.valor<0||m.valor>4095||!Number.isInteger(m.valor)))return 'ADC_INVALIDO';
  if(m.sensor==='humedad'&&m.calidad!=='SENSOR_ERROR'&&(m.valor<0||m.valor>100))return 'HUMEDAD_INVALIDA';
  if(m.sensor==='temperatura'&&m.calidad!=='SENSOR_ERROR'&&(m.valor < -40||m.valor>125))return 'TEMP_INVALIDA';
  if(m.sensor==='estado_secadores'&&![0,1,2,3].includes(m.valor))return 'ESTADO_INVALIDO';
  if(typeof m.firma!=='string'||!/^[0-9a-f]{64}$/.test(m.firma))return 'FIRMA_INVALIDA';
  return null;
}
function createGroupVerifier(key){
  if(typeof key!=='string'||key.length<24)throw new Error('Clave HMAC insuficiente');
  const lastSeq=new Map();
  return function(payload){
    let m;
    try{m=typeof payload==='string'?JSON.parse(payload):payload;}catch{return {ok:false,reason:'JSON_NO_VALIDO'};}
    const problem=checkGroup(m);
    if(problem)return {ok:false,reason:problem};
    const expected=Buffer.from(signGroup(m,key),'hex');
    const got=Buffer.from(m.firma,'hex');
    if(!crypto.timingSafeEqual(expected,got))return {ok:false,reason:'HMAC_INVALIDO'};
    const old=lastSeq.get(m.sensor)||0;
    if(m.seq<=old)return {ok:false,reason:'REPETICION_DE_SECUENCIA'};
    lastSeq.set(m.sensor,m.seq);
    return {ok:true,message:m};
  };
}
module.exports={canonicalGroup,signGroup,checkGroup,createGroupVerifier};
