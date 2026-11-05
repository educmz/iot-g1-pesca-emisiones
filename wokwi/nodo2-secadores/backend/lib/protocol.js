'use strict';
const crypto = require('node:crypto');
const STATUS = new Set(['NORMAL','ADVERTENCIA','ALERTA','FALLA_SENSOR']);

function canonical(m) {
  return [
    'v1', m.id, m.boot, String(m.ts), String(m.seq),
    Number(m.temperatura).toFixed(2), Number(m.humedad).toFixed(2),
    String(m.gas_raw), Number(m.gas).toFixed(2), m.unidad, m.calidad, m.estado
  ].join('|');
}
function sign(message, key) {
  return crypto.createHmac('sha256', key).update(canonical(message), 'utf8').digest('hex');
}
function checkShape(m) {
  if (!m || Array.isArray(m) || typeof m !== 'object') return 'JSON_NO_VALIDO';
  if (m.id !== 'nodo2') return 'IDENTIFICADOR_INESPERADO';
  if (typeof m.boot !== 'string' || !/^[0-9a-f]{1,32}$/.test(m.boot)) return 'BOOT_INVALIDO';
  if (!Number.isSafeInteger(m.ts) || !Number.isSafeInteger(m.seq) || m.seq < 1) return 'SECUENCIA_O_TIEMPO_INVALIDO';
  for (const k of ['temperatura','humedad','gas']) {
    if (typeof m[k] !== 'number' || !Number.isFinite(m[k])) return 'LECTURA_NO_NUMERICA';
  }
  if (!Number.isInteger(m.gas_raw) || m.gas_raw < 0 || m.gas_raw > 4095) return 'GAS_RAW_INVALIDO';
  if (m.gas < 0 || m.gas > 100) return 'GAS_FUERA_RANGO';
  if (!STATUS.has(m.estado)) return 'ESTADO_INVALIDO';
  if (m.unidad !== 'gas_pct_simulado') return 'UNIDAD_INVALIDA';
  if (!['OK','SENSOR_ERROR','PRUEBA_INYECTADA'].includes(m.calidad)) return 'CALIDAD_INVALIDA';
  if (m.calidad !== 'SENSOR_ERROR' && (m.temperatura < -40 || m.temperatura > 125 || m.humedad < 0 || m.humedad > 100)) return 'DHT_FUERA_RANGO';
  if (m.calidad === 'SENSOR_ERROR' && (m.temperatura !== -99 || m.humedad !== -99 || m.estado !== 'FALLA_SENSOR')) return 'ERROR_SENSOR_INCOHERENTE';
  if (typeof m.firma !== 'string' || !/^[0-9a-f]{64}$/.test(m.firma)) return 'FIRMA_AUSENTE_O_INVALIDA';
  return null;
}
function createVerifier(secret, options={}) {
  if (typeof secret !== 'string' || secret.length < 24) throw new Error('Clave HMAC DEMO debe tener al menos 24 caracteres');
  const seen = new Map(); // último seq por boot, sirve para reinicios sin perder anti-replay de boots observados
  const nowFn = options.now || (() => Math.floor(Date.now()/1000));
  return function verify(payload) {
    let m;
    try { m = typeof payload === 'string' ? JSON.parse(payload) : payload; }
    catch { return {ok:false,reason:'JSON_NO_VALIDO'}; }
    const problem=checkShape(m);
    if (problem) return {ok:false,reason:problem};
    const age=nowFn()-m.ts;
    if (age > 120 || age < -45) return {ok:false,reason:'MENSAJE_FUERA_DE_VENTANA_TEMPORAL'};
    const expected=Buffer.from(sign(m,secret),'hex');
    const received=Buffer.from(m.firma,'hex');
    if (!crypto.timingSafeEqual(expected,received)) return {ok:false,reason:'HMAC_INVALIDO'};
    const old=seen.get(m.boot)||0;
    if (m.seq <= old) return {ok:false,reason:'REPETICION_DE_SECUENCIA'};
    seen.set(m.boot,m.seq);
    if(seen.size>1000) seen.delete(seen.keys().next().value);
    return {ok:true,message:m};
  };
}
module.exports={canonical,sign,checkShape,createVerifier};
