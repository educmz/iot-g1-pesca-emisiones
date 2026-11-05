/* Read-only MQTT/TCP control probe. No sensor readings, keys or actuator commands.
   node scripts/probe_mqtt.cjs [duration seconds] [output JSON]
   Measures host->EMQX only; never substitutes for Wokwi/ESP32 evidence. */
const net=require('node:net'),fs=require('node:fs'),crypto=require('node:crypto');
const duration=Number(process.argv[2]||120)*1000;
const result={source:'host TCP control, NOT Wokwi',host:'broker.emqx.io',port:1883,
  started:new Date().toISOString(),durationRequestedMs:duration,connack:false,
  suback:false,pingResponses:0,receivedByTopic:{},errors:[]};
const started=Date.now();let pending=Buffer.alloc(0),timer,ping,done=false;
const str=s=>{const b=Buffer.from(s),n=Buffer.alloc(2);n.writeUInt16BE(b.length);return Buffer.concat([n,b]);};
function packet(type,body){let n=body.length,bytes=[];do{let b=n%128;n=Math.floor(n/128);bytes.push(b|(n?128:0));}while(n);return Buffer.concat([Buffer.from([type,...bytes]),body]);}
const socket=net.connect({host:result.host,port:result.port});
socket.on('connect',()=>{result.tcpConnectedMs=Date.now()-started;socket.write(packet(0x10,Buffer.concat([str('MQTT'),Buffer.from([4,2,0,20]),str('g1-diag-'+crypto.randomBytes(8).toString('hex'))])));});
socket.on('data',data=>{
  pending=Buffer.concat([pending,data]);
  while(pending.length>=2){
    let i=1,len=0,m=1,b;
    do{if(i>=pending.length)return;b=pending[i++];len+=(b&127)*m;m*=128;if(i>5){result.errors.push('bad remaining length');socket.destroy();return;}}while(b&128);
    if(pending.length<i+len)return;
    const type=pending[0]>>4,body=pending.subarray(i,i+len);pending=pending.subarray(i+len);
    if(type===2){result.connack=body[1]===0;result.connackCode=body[1];result.sessionStartMs=Date.now()-started;if(result.connack)socket.write(packet(0x82,Buffer.concat([Buffer.from([0,1]),str('upc/g1/+/datos'),Buffer.from([0])])));}
    if(type===9)result.suback=body[2]!==128;
    if(type===13)result.pingResponses++;
    if(type===3 && body.length>=2){const topic=body.subarray(2,2+body.readUInt16BE()).toString();result.receivedByTopic[topic]=(result.receivedByTopic[topic]||0)+1;}
  }
});
function finish(reason){if(done)return;done=true;clearTimeout(timer);clearInterval(ping);result.finishReason=reason;result.elapsedMs=Date.now()-started;result.sessionDurationMs=result.sessionStartMs===undefined?0:result.elapsedMs-result.sessionStartMs;result.finished=new Date().toISOString();if(socket.writable)socket.end(Buffer.from([0xe0,0]));socket.destroy();const json=JSON.stringify(result,null,2);if(process.argv[3])fs.writeFileSync(process.argv[3],json+'\n');console.log(json);}
socket.on('error',e=>{result.errors.push(e.code||e.message);finish('socket error');});
socket.on('close',()=>finish('remote close'));
timer=setTimeout(()=>finish('requested duration reached'),duration);
ping=setInterval(()=>{if(result.connack && socket.writable)socket.write(Buffer.from([0xc0,0]));},10000);
