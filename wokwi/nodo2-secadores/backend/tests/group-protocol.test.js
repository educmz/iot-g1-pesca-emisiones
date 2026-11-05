'use strict';
const test=require('node:test');
const assert=require('node:assert/strict');
const {canonicalGroup,signGroup,createGroupVerifier}=require('../lib/group-protocol');
const KEY='CLAVE_PRIVADA_SOLO_TEST_NODO2_2026';
const message=()=>({id:'nodo2',ts:42000,seq:100,sensor:'gas_relativo',valor:42.35,unidad:'%',calidad:'OK'});
test('canónico coincide con contrato de sensor',()=>{
  const m=message();
  assert.equal(canonicalGroup(m),'nodo2|42000|100|gas_relativo|42.35|%|OK');
  assert.equal(signGroup(m,KEY),'38f994509e8b707dd2ecd0ee2d337d48525475540d53c8cc035095280bbcaebf');
});
test('HMAC válido admitido',()=>{
  const m=message();m.firma=signGroup(m,KEY);assert.equal(createGroupVerifier(KEY)(m).ok,true);
});
test('lectura alterada rechazada',()=>{
  const m=message();m.firma=signGroup(m,KEY);m.valor=99;
  assert.equal(createGroupVerifier(KEY)(m).reason,'HMAC_INVALIDO');
});
test('replay rechazado en misma sesión',()=>{
  const m=message();m.firma=signGroup(m,KEY);const v=createGroupVerifier(KEY);
  assert.equal(v(m).ok,true);assert.equal(v(m).reason,'REPETICION_DE_SECUENCIA');
});
test('no se permite ppm para gas relativo',()=>{
  const m=message();m.unidad='ppm';m.firma=signGroup(m,KEY);
  assert.equal(createGroupVerifier(KEY)(m).reason,'SENSOR_UNIDAD_INVALIDO');
});
test('humedad imposible rechazada',()=>{
  const m={...message(),sensor:'humedad',valor:150};m.firma=signGroup(m,KEY);
  assert.equal(createGroupVerifier(KEY)(m).reason,'HUMEDAD_INVALIDA');
});
test('redondeo decimal canónico',()=>{
  const m={...message(),valor:42.3};assert.equal(canonicalGroup(m),'nodo2|42000|100|gas_relativo|42.30|%|OK');
});
test('contador persistente mayor a uint32 conserva firma y bloquea replay',()=>{
  const v=createGroupVerifier(KEY);
  const a={...message(),seq:4294967297};a.firma=signGroup(a,KEY);
  assert.equal(v(JSON.parse(JSON.stringify(a))).ok,true);
  assert.equal(v(a).reason,'REPETICION_DE_SECUENCIA');
  const b={...message(),seq:4294971393};b.firma=signGroup(b,KEY);
  assert.equal(v(b).ok,true);
  assert.equal(v(a).reason,'REPETICION_DE_SECUENCIA');
});
