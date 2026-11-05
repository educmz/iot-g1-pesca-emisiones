'use strict';
const test=require('node:test');
const assert=require('node:assert/strict');
const net=require('node:net');
const {MiniMqtt}=require('../lib/mqtt-mini');
function encode(s){const b=Buffer.from(s);return Buffer.concat([Buffer.from([b.length>>8,b.length&255]),b]);}
test('MQTT cliente conecta, suscribe y recibe PUBLISH QoS 0 de broker local simulado',async()=>{
  let incoming=Buffer.alloc(0),topics=0;
  const fakeBroker=net.createServer(socket=>{
    socket.on('data',part=>{
      incoming=Buffer.concat([incoming,part]);
      while(incoming.length>=2){
        const kind=incoming[0],size=incoming[1];
        if(size>=128)throw new Error('test solo tramas pequeñas');
        if(incoming.length<2+size)break;
        const payload=incoming.subarray(2,2+size);incoming=incoming.subarray(2+size);
        if(kind===0x10)socket.write(Buffer.from([0x20,0x02,0,0]));
        else if(kind===0x82){
          topics++;
          socket.write(Buffer.from([0x90,0x03,payload[0],payload[1],0]));
          const topic='upc/g1/nodo2/datos',data='{"mock":true}';
          const body=Buffer.concat([encode(topic),Buffer.from(data)]);
          if(body.length<128)socket.write(Buffer.concat([Buffer.from([0x30,body.length]),body]));
        }
      }
    });
  });
  await new Promise(resolve=>fakeBroker.listen(0,'127.0.0.1',resolve));
  const client=new MiniMqtt({host:'127.0.0.1',port:fakeBroker.address().port,clientId:'test-g1',topics:['upc/g1/nodo2/datos']});
  try{
    const received=new Promise((resolve,reject)=>{
      const timer=setTimeout(()=>reject(new Error('No se recibió PUBLISH')),2000);
      client.on('message',(topic,payload)=>{clearTimeout(timer);resolve({topic,payload});});
    });
    client.start();
    const got=await received;
    assert.equal(topics,1);assert.equal(got.topic,'upc/g1/nodo2/datos');assert.equal(got.payload,'{"mock":true}');
    assert.equal(client.connected,true);
  }finally{
    client.stop();await new Promise(resolve=>fakeBroker.close(resolve));
  }
});
