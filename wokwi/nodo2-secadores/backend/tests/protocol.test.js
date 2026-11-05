'use strict';
const test=require('node:test');
const assert=require('node:assert/strict');
const {sign,createVerifier,canonical}=require('../lib/protocol');
const KEY='CAMBIAR_POR_CLAVE_DE_PRUEBA_COMPARTIDA_202620';
const NOW=1793830000;
function message(seq=1){
  const m={id:'nodo2',boot:'ab0ff123',ts:NOW,seq,temperatura:35.5,humedad:60.0,
    gas_raw:1800,gas:43.96,unidad:'gas_pct_simulado',calidad:'OK',estado:'ADVERTENCIA'};
  m.firma=sign(m,KEY);return m;
}
test('firma HMAC válida es admitida',()=>{
  assert.equal(createVerifier(KEY,{now:()=>NOW})(message()).ok,true);
});
test('mismo JSON recompuesto produce misma firma',()=>{
  const m=message();assert.match(canonical(m),/^v1\|nodo2\|/);assert.equal(sign({...m},KEY),m.firma);
});
test('suplantación sin firma es rechazada',()=>{
  const m=message();m.firma='0'.repeat(64);
  assert.equal(createVerifier(KEY,{now:()=>NOW})(m).reason,'HMAC_INVALIDO');
});
test('lectura adulterada conservando la firma se rechaza',()=>{
  const m=message();m.gas=99;
  assert.equal(createVerifier(KEY,{now:()=>NOW})(m).reason,'HMAC_INVALIDO');
});
test('mensaje viejo es rechazado',()=>{
  const m=message();
  assert.equal(createVerifier(KEY,{now:()=>NOW+121})(m).reason,'MENSAJE_FUERA_DE_VENTANA_TEMPORAL');
});
test('repetición de seq se rechaza',()=>{
  const verify=createVerifier(KEY,{now:()=>NOW});const m=message();
  assert.equal(verify(m).ok,true);assert.equal(verify(m).reason,'REPETICION_DE_SECUENCIA');
});
test('lectura fuera de rango se rechaza',()=>{
  const m=message();m.humedad=122;m.firma=sign(m,KEY);
  assert.equal(createVerifier(KEY,{now:()=>NOW})(m).reason,'DHT_FUERA_RANGO');
});
test('identificador de otro nodo se rechaza',()=>{
  const m=message();m.id='nodo3';m.firma=sign(m,KEY);
  assert.equal(createVerifier(KEY,{now:()=>NOW})(m).reason,'IDENTIFICADOR_INESPERADO');
});
test('JSON malformado se rechaza',()=>{
  assert.equal(createVerifier(KEY,{now:()=>NOW})('{bad').reason,'JSON_NO_VALIDO');
});
test('falla sensor codificada correctamente',()=>{
  const m=message();m.temperatura=-99;m.humedad=-99;m.calidad='SENSOR_ERROR';m.estado='FALLA_SENSOR';m.firma=sign(m,KEY);
  assert.equal(createVerifier(KEY,{now:()=>NOW})(m).ok,true);
});

test('lectura académica inyectada mantiene HMAC válido y etiqueta de calidad',()=>{
  const m=message(10);m.gas_raw=614;m.gas=14.99;m.estado='NORMAL';m.calidad='PRUEBA_INYECTADA';m.firma=sign(m,KEY);
  const verdict=createVerifier(KEY,{now:()=>NOW})(m);assert.equal(verdict.ok,true);
  const manipulated={...m,gas:80};assert.equal(createVerifier(KEY,{now:()=>NOW})(manipulated).reason,'HMAC_INVALIDO');
});
