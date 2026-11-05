'use strict';
process.env.DEMO_MODE='1';
const test=require('node:test');
const assert=require('node:assert/strict');
const {server,shutdown,acceptMessage,simulateAttack,newDemoPayload}=require('../server');
function req(port,path,method='GET'){
  return fetch(`http://127.0.0.1:${port}${path}`,{method}).then(async r=>({status:r.status,body:r.headers.get('content-type')?.startsWith('application/json')?await r.json():await r.text()}));
}
test('dashboard sin red: lectura firmada y suplantación rechazada vía endpoint',async()=>{
  await new Promise(res=>server.listen(0,'127.0.0.1',res));
  try{
    const port=server.address().port;
    const first=newDemoPayload();
    const admitted=acceptMessage(first,'upc/g1/nodo2/paquete');
    assert.equal(admitted.ok,true);
    let r=await req(port,'/api/state');assert.equal(r.status,200);assert.equal(r.body.last.firma,first.firma);
    const savedSeq=r.body.last.seq;
    r=await req(port,'/api/demo/attack','POST');
    // DEMO_MODE=1: ataque simulado recorre el mismo verificador HMAC local.
    assert.equal(r.status,202);assert.equal(r.body.simulatedLocally,true);
    const fake={...newDemoPayload(),firma:'0'.repeat(64)};
    assert.equal(acceptMessage(fake,'upc/g1/nodo2/paquete').reason,'HMAC_INVALIDO');
    r=await req(port,'/api/state');assert.equal(r.body.last.seq,savedSeq);
    assert.equal(r.body.security[0].motivo,'HMAC_INVALIDO');
    r=await req(port,'/api/export.csv');assert.equal(r.status,200);assert.match(r.body,/nodo2/);
    r=await req(port,'/');assert.equal(r.status,200);assert.match(r.body,/Nodo 2/);
  }finally{await new Promise(resolve=>server.close(resolve));shutdown();}
});
