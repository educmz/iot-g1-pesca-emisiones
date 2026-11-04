'use strict';
const $ = id => document.getElementById(id);
let latest = null;
let eventsFilter = 'all';
let busy = false;
let toastTimer;
const SHORT = {BAJA:'BAJA',MEDIA:'MEDIA',MAXIMA:'MÁXIMA',ON:'ON',OFF:'OFF',SI:'SÍ',NO:'NO'};
const fmt = x => x == null ? '—' : (SHORT[String(x)] || String(x));
const age = x => x == null ? 'Sin datos' : (x < 2 ? 'Ahora mismo' : `${Math.round(x)} s`);
function toast(message, error=false) {
  const el = $('toast'); el.textContent=message;el.className='toast show'+(error?' error':'');
  clearTimeout(toastTimer);toastTimer=setTimeout(()=>el.className='toast',4400);
}
function metric(id, value, tone='', rail=0) {
  const el=$(id), card=el.closest('.metric-card');
  el.textContent=fmt(value);card.classList.remove('good','danger');if(tone)card.classList.add(tone);
  $(id+'-rail').style.width=rail+'%';
}
function notice(text, level='') { $('trust-text').textContent=text; $('trust-notice').className='notice '+level; }
function renderHeader(s) {
  const chip=$('connectivity');let label,klass;
  if(!s.mqtt_connected){label='Broker desconectado';klass='offline';}
  else if(s.freshness==='actual'){label='MQTT · Datos en vivo';klass='connected';}
  else {label='Sin datos recientes';klass='waiting';}
  chip.className='connection-chip '+klass;
  chip.querySelector('span:last-child').textContent=label;
  if(s.freshness==='actual') notice('Telemetría verificada con la clave HMAC de D4. El panel muestra lecturas recibidas del ESP32.', 'success');
  else if(s.freshness==='atrasado') notice('Los últimos datos firmados están atrasados. No supongas que el lavador mantiene su estado anterior.', 'warning');
  else if(!s.mqtt_connected) notice('Sin enlace MQTT. La actuación autónoma de D4 puede seguir funcionando, pero no hay supervisión en vivo.', 'warning');
  else notice('Esperando datos firmados válidos de D4. Comprueba las claves y la simulación en Wokwi.', 'warning');
}
function renderMetrics(s) {
  const fresh=s.freshness==='actual';
  const d=fresh?s.data:null;
  const on=d?.valor==='ON';
  metric('lavador',d?.valor||'--',on?'good':'',on?100:5);
  $('lavador-note').textContent=d?on?'Ordenado: encendido':'Ordenado: apagado':'Sin datos verificados recientes';
  metric('confirmacion',d?.confirmado||'--',d?.confirmado==='SI'?'good':(on?'danger':''),d?.confirmado==='SI'?100:5);
  metric('intensidad',d?.intensidad||'--','',d?{BAJA:33,MEDIA:66,MAXIMA:100}[d.intensidad]||0:0);
  metric('riesgo',d?.riesgo||'--',d?.riesgo==='ALTO'?'danger':d?.riesgo==='NORMAL'?'good':'',d?.riesgo==='ALTO'?100:5);
  $('seguro-label').textContent=d?'Modo seguro: '+fmt(d.seguro):'Modo seguro: desconocido';
  for(const btn of document.querySelectorAll('[data-cmd],[data-action]')){
    btn.disabled=busy||!s.mqtt_connected;
  }
  if(!d||d.riesgo==='ALTO'||d.seguro==='SI') $('off-button').disabled=true;
  for(const btn of document.querySelectorAll('.speed-button')) btn.classList.toggle('selected',d?.intensidad===({BAJO:'BAJA',MEDIO:'MEDIA',MAX:'MAXIMA'}[btn.dataset.cmd]));
}
function renderDiagnostics(s) {
  $('diag-mqtt').textContent=s.mqtt_connected?'CONECTADO':'DESCONECTADO';
  $('diag-age').textContent=age(s.data_age_s);
  $('diag-hb').textContent=age(s.node_heartbeat_age_s);
  $('diag-hb-enabled').textContent=s.gateway_heartbeat_enabled?'ACTIVO':'PAUSADO';
  $('diag-invalid').textContent=String(s.rejected_telemetry||0);
  $('diag-transport').textContent=s.transport||'--';
  for(const key of ['datos','estado','alerta','cmd']) $('topic-'+key).textContent=s.topics?.[key]||'--';
  const c=s.last_request;
  $('last-cmd').textContent=c?`${c.cmd} · ${c.tag}`:'Sin órdenes enviadas';
  $('last-cmd-state').textContent=c?({BROKER_ACK:'Entregado al broker · esperando D4',COMANDO_EJECUTADO:'Confirmado por D4',COMANDO_BLOQUEADO:'Bloqueado por D4'}[c.state]||c.state):'—';
  $('last-seq').textContent='SEQ: '+(c?.seq??'—');
}
function drawTrend(series) {
  const el=$('trend-line'),area=$('trend-area');
  const points=(series||[]).slice(-48);
  $('trend-count').textContent=points.length+' lecturas';
  if(!points.length){el.setAttribute('d','');area.setAttribute('d','');$('chart-empty').hidden=false;return;}
  $('chart-empty').hidden=true;
  const w=602, left=8,base=154;
  const toY=v=>154-v*46;
  const rows=points.map((d,i)=>{const level=d.lavador==='OFF'?0:({BAJA:1,MEDIA:2,MAXIMA:3}[d.intensidad]||0);return [left+(points.length>1?i/(points.length-1):.5)*w,toY(level)];});
  const path=rows.map((p,i)=>(i?'L':'M')+p[0].toFixed(1)+','+p[1].toFixed(1)).join(' ');
  el.setAttribute('d',path);
  area.setAttribute('d',path+` L${rows.at(-1)[0].toFixed(1)},${base} L${rows[0][0].toFixed(1)},${base} Z`);
}
function eventItem(e) {
  const node=document.createElement('div'); node.className='event-item '+e.category;
  const bullet=document.createElement('span');bullet.className='event-bullet';
  const body=document.createElement('div');body.className='event-body';
  const name=document.createElement('div');name.className='event-name';name.textContent=e.type;
  const txt=document.createElement('div');txt.className='event-text';txt.textContent=e.message;
  const timeEl=document.createElement('time');timeEl.className='event-time';
  try{timeEl.textContent=new Date(e.at*1000).toLocaleTimeString('es-PE',{hour:'2-digit',minute:'2-digit',second:'2-digit'});}catch{timeEl.textContent='--';}
  body.append(name,txt);node.append(bullet,body,timeEl);return node;
}
function renderEvents(s) {
  const filtered=(s.events||[]).filter(e=>eventsFilter==='all'||e.category===eventsFilter);
  const container=$('event-list');container.replaceChildren();
  if(!filtered.length){const empty=document.createElement('div');empty.className='empty-events';empty.textContent='No hay eventos para este filtro.';container.append(empty);}
  else filtered.forEach(e=>container.append(eventItem(e)));
  $('event-count').textContent=filtered.length+' eventos';
  $('security-count').textContent=(s.security_count||0)+' incidentes';
}
async function refresh() {
  try{
    const r=await fetch('/api/state',{cache:'no-store'});if(!r.ok)throw Error('HTTP '+r.status);
    latest=await r.json();
    renderHeader(latest);renderMetrics(latest);renderDiagnostics(latest);renderEvents(latest);drawTrend(latest.history);
  }catch(err){
    $('connectivity').className='connection-chip offline';
    $('connectivity').querySelector('span:last-child').textContent='Puente local sin respuesta';
    notice('No se puede contactar la aplicación Python local. Verifica que bridge.py continúa abierto.','warning');
    document.querySelectorAll('[data-cmd],[data-action]').forEach(b=>b.disabled=true);
  }
}
async function action(payload){
  if(busy)return;
  busy=true; document.querySelectorAll('[data-cmd],[data-action]').forEach(b=>b.disabled=true);
  try{
    const response=await fetch('/api/action',{
      method:'POST',headers:{'Content-Type':'application/json','X-Panel-Token':window.D4_PANEL_TOKEN},
      body:JSON.stringify(payload),cache:'no-store'
    });
    const doc=await response.json(); toast(doc.message||'Operación completada',!doc.ok);
  }catch(e){toast('No se pudo ejecutar la acción: '+e.message,true);}
  finally{busy=false;await refresh();}
}
document.addEventListener('DOMContentLoaded',()=>{
  if(new URLSearchParams(location.search).get('embed')==='1') document.body.classList.add('embed');
  const syncClock=()=>{$('clock').textContent=new Date().toLocaleTimeString('es-PE',{hour:'2-digit',minute:'2-digit',second:'2-digit'});};
  syncClock();setInterval(syncClock,1000);
  document.querySelectorAll('[data-cmd]').forEach(btn=>btn.addEventListener('click',()=>action({action:'cmd',cmd:btn.dataset.cmd})));
  document.querySelectorAll('[data-action]').forEach(btn=>btn.addEventListener('click',()=>action({action:btn.dataset.action})));
  document.querySelectorAll('[data-filter]').forEach(btn=>btn.addEventListener('click',()=>{
    eventsFilter=btn.dataset.filter;document.querySelectorAll('[data-filter]').forEach(e=>e.classList.toggle('selected',e===btn));if(latest)renderEvents(latest);
  }));
  refresh();setInterval(refresh,1400);
  const observed=new IntersectionObserver(entries=>{for(const entry of entries){if(entry.isIntersecting){const name=entry.target.id;document.querySelectorAll('.nav-item').forEach(a=>a.classList.toggle('active',a.hash==='#'+name));}}},{rootMargin:'-20% 0px -60% 0px'});
  ['operacion','seguridad','eventos','integracion'].forEach(id=>{if($(id))observed.observe($(id));});
});
