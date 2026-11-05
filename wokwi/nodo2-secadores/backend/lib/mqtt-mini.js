'use strict';
// Cliente MQTT 3.1.1 mínimo, solo QoS 0; evita instalar dependencias para esta demo.
const net = require('node:net');
const EventEmitter = require('node:events');
function encString(s) { const b=Buffer.from(s,'utf8'); return Buffer.concat([Buffer.from([b.length>>8,b.length&255]),b]); }
function remLen(n) {const bytes=[]; do{let d=n%128;n=Math.floor(n/128);if(n)d|=128;bytes.push(d);}while(n);return Buffer.from(bytes);}
function packet(header,body){return Buffer.concat([Buffer.from([header]),remLen(body.length),body]);}
class MiniMqtt extends EventEmitter {
  constructor({host,port=1883,clientId,topics=[]}){
    super(); Object.assign(this,{host,port,clientId,topics});
    this.socket=null;this.pending=Buffer.alloc(0);this.connected=false;this.stopping=false;this.retry=null;this.ping=null;this.packetId=0;
  }
  start(){this.stopping=false;this.connect();}
  connect(){
    if(this.stopping)return;
    const s=net.createConnection({host:this.host,port:this.port});
    this.socket=s;this.pending=Buffer.alloc(0);
    s.setTimeout(20000);
    s.on('connect',()=>{
      const variable=Buffer.concat([encString('MQTT'),Buffer.from([4,2,0,30])]);
      s.write(packet(0x10,Buffer.concat([variable,encString(this.clientId)])));
    });
    s.on('data',d=>this.receive(d));
    s.on('timeout',()=>s.destroy(new Error('MQTT socket timeout')));
    s.on('error',e=>this.emit('notice','MQTT: '+e.message));
    s.on('close',()=>{
      if(this.socket!==s)return;
      this.connected=false;clearInterval(this.ping);this.emit('connection',false);
      if(!this.stopping)this.retry=setTimeout(()=>this.connect(),4000);
    });
  }
  receive(chunk){
    this.pending=Buffer.concat([this.pending,chunk]);
    while(this.pending.length>=2){
      let remaining=0,mult=1,n=1,b;
      do{
        if(this.pending.length<=n)return;
        b=this.pending[n++]; remaining+=(b&127)*mult;mult*=128;
        if(n>5){this.socket.destroy(new Error('Longitud MQTT inválida'));return;}
      }while(b&128);
      if(this.pending.length < n+remaining)return;
      const h=this.pending[0], body=this.pending.subarray(n,n+remaining);
      this.pending=this.pending.subarray(n+remaining);
      this.handle(h,body);
    }
  }
  handle(h,body){
    switch(h>>4){
      case 2: // CONNACK
        if(body.length<2||body[1]!==0){this.socket.destroy(new Error('CONNACK rechazado'));return;}
        this.connected=true;this.socket.setTimeout(0);
        this.emit('connection',true);
        for(const topic of this.topics)this.subscribe(topic);
        this.ping=setInterval(()=>{if(this.connected)this.socket.write(Buffer.from([0xc0,0]));},15000);
        break;
      case 3: { // PUBLISH QoS0 (broker entrega QoS0 por nuestras suscripciones)
        if(body.length<2)return;
        const size=body.readUInt16BE(0);if(body.length<size+2)return;
        const topic=body.subarray(2,2+size).toString('utf8');
        this.emit('message',topic,body.subarray(2+size).toString('utf8'));
        break;
      }
      case 9: // SUBACK
      case 13: // PINGRESP
        break;
    }
  }
  subscribe(topic){
    if(!this.connected)return false;
    const id=++this.packetId&0xffff||1;
    const head=Buffer.alloc(2);head.writeUInt16BE(id);
    return this.socket.write(packet(0x82,Buffer.concat([head,encString(topic),Buffer.from([0])])));
  }
  publish(topic,data){
    if(!this.connected)return false;
    return this.socket.write(packet(0x30,Buffer.concat([encString(topic),Buffer.from(String(data),'utf8')])));
  }
  stop(){this.stopping=true;clearTimeout(this.retry);clearInterval(this.ping);if(this.socket)this.socket.destroy();}
}
module.exports={MiniMqtt};
