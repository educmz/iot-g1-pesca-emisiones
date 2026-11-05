"""Puente local MQTT/HTTP para el panel del Nodo 4 (laboratorio educativo).

Solo escucha en 127.0.0.1; las claves HMAC nunca se envian al navegador.
Se utiliza con el firmware Wokwi D4 V3 y el contrato firmado de G1 Pesca.
"""
from __future__ import annotations

import argparse
import hashlib
import hmac
import math
import json
import secrets
import socket
import ssl
import threading
import time
import uuid
from collections import deque
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
from pathlib import Path
from urllib.parse import urlparse

try:
    import paho.mqtt.client as mqtt
except ImportError:
    raise SystemExit('Falta paho-mqtt. Instala con: python -m pip install "paho-mqtt>=2,<3"')

ROOT = Path(__file__).resolve().parent
CONFIG_FILE = ROOT / 'config.local.json'
STATIC = {
    '/': ('panel/panel-nodo4.html', 'text/html; charset=utf-8'),
    '/index.html': ('panel/panel-nodo4.html', 'text/html; charset=utf-8'),
    '/styles.css': ('panel/styles.css', 'text/css; charset=utf-8'),
    '/app.js': ('panel/app.js', 'application/javascript; charset=utf-8'),
}
ALLOWED_CMDS = {'ON', 'OFF', 'BAJO', 'MEDIO', 'MAX', 'RIESGO_ON', 'RIESGO_OFF', 'HB'}
ACTUATION_CMDS = {'ON', 'OFF'}
PORT_MODES = {1883: 'TCP', 8883: 'TLS', 8884: 'WSS', 8000: 'WS'}


def load_config():
    if not CONFIG_FILE.exists():
        raise SystemExit('No existe config.local.json. Copia config.example.json, renombralo y coloca las mismas claves que Wokwi.')
    cfg = json.loads(CONFIG_FILE.read_text(encoding='utf-8'))
    required = ['host', 'port', 'topic_cmd', 'topic_datos', 'topic_estado', 'topic_alerta', 'clave_gateway', 'clave_nodo4']
    missing = [x for x in required if not isinstance(cfg.get(x), (str, int)) or not str(cfg.get(x)).strip()]
    if missing:
        raise SystemExit('Configuracion incompleta: ' + ', '.join(missing))
    for field in ('clave_gateway', 'clave_nodo4'):
        if any(word in cfg[field].upper() for word in ('REEMPLAZAR', 'CAMBIAR', 'EJEMPLO', 'PLACEHOLDER')):
            raise SystemExit('Sustituye el valor de ' + field + ' por la clave de pruebas usada en Wokwi.')
    for field in ('clave_gateway','clave_nodo4'):
        key = cfg[field]
        if (not isinstance(key, str) or len(key) < 24 or
                key.startswith(('G1_Gateway_Lab_', 'G1_Nodo4_Lab_'))):
            raise SystemExit('Clave de demostración publicada o insuficiente: cámbiala en config.local.json')
    cfg.setdefault('topic_paquete', cfg['topic_datos'].rsplit('/', 1)[0] + '/paquete')
    cfg['port'] = int(cfg['port'])
    return cfg


def canonical_command(doc):
    return '|'.join(str(doc[k]) for k in ('id', 'ts', 'seq', 'cmd', 'valor'))


def sign(text, key):
    return hmac.new(key.encode('utf-8'), text.encode('utf-8'), hashlib.sha256).hexdigest()


def sign_command(cmd, key, seq, ts=None, valor=''):
    doc = {'id': 'D5', 'ts': int(time.time()) if ts is None else int(ts), 'seq': int(seq), 'cmd': cmd, 'valor': str(valor)}
    doc['firma'] = sign(canonical_command(doc), key)
    return doc


def verify_group_node(doc, topic, config):
    """Valida la firma de cada sensor emitido por D4 en /datos."""
    if topic != config['topic_datos'] or not isinstance(doc, dict) or doc.get('id') != 'nodo4':
        return False
    allowed = {
        'lavador': ('estado', 0, 1),
        'confirmacion': ('estado', 0, 1),
        'intensidad': ('nivel', 1, 3),
        'modo_seguro': ('estado', 0, 1),
        'riesgo_alto': ('estado', 0, 1),
    }
    sensor = doc.get('sensor')
    if not isinstance(sensor, str) or sensor not in allowed:
        return False
    unit, lo, hi = allowed[sensor]
    valor = doc.get('valor')
    if (type(doc.get('ts')) is not int or doc['ts'] < 0 or
            type(doc.get('seq')) is not int or doc['seq'] < 1 or
            type(valor) not in (int, float) or not math.isfinite(valor) or
            int(valor) != valor or not lo <= valor <= hi or
            doc.get('calidad') != 'ok' or doc.get('unidad') != unit or
            not isinstance(doc.get('firma'), str)):
        return False
    canonical = '|'.join([
        'nodo4', str(doc['ts']), str(doc['seq']), sensor,
        f'{valor:.2f}', unit, 'ok'
    ])
    return hmac.compare_digest(sign(canonical, config['clave_nodo4']), doc['firma'])


def verify_node(doc, topic, config):
    """Comprueba HMAC tal como lo construye el sketch D4 V3."""
    if not isinstance(doc, dict) or doc.get('id') != 'D4':
        return False
    if type(doc.get('ts')) is not int or not isinstance(doc.get('firma'), str):
        return False
    ts = doc['ts']
    valor = doc.get('valor')
    unidad = doc.get('unidad')
    calidad = doc.get('calidad')
    if not all(isinstance(x, str) for x in (valor, unidad, calidad)):
        return False
    if topic == config['topic_paquete']:
        if not all(isinstance(doc.get(k), str) for k in ('intensidad', 'confirmado', 'seguro', 'riesgo')):
            return False
        if type(doc.get('seq')) is not int or doc['seq'] < 0:
            return False
        canonical = (f'D4|{ts}|{valor}|estado_lavador|VALIDO|{doc["intensidad"]}|'
                     f'{doc["confirmado"]}|{doc["seguro"]}|{doc["riesgo"]}|{doc["seq"]}')
        if unidad != 'estado_lavador' or calidad != 'VALIDO':
            return False
    elif topic == config['topic_alerta']:
        if not isinstance(doc.get('tipo'), str) or not isinstance(doc.get('motivo'), str):
            return False
        if type(doc.get('seq')) is not int or doc['seq'] < 0:
            return False
        canonical = f'D4|{ts}|{valor}|evento|VALIDO|{doc["tipo"]}|{doc["motivo"]}|{doc["seq"]}'
        if unidad != 'evento' or calidad != 'VALIDO' or valor != doc['tipo']:
            return False
    elif topic == config['topic_estado']:
        if valor != 'online' or unidad != 'disponibilidad' or calidad != 'VALIDO':
            return False
        canonical = f'D4|{ts}|online|disponibilidad|VALIDO'
    else:
        return False
    return hmac.compare_digest(sign(canonical, config['clave_nodo4']), doc['firma'])


class GatewayBridge:
    def __init__(self, config):
        self.cfg = config
        self.lock = threading.RLock()
        self.connected = threading.Event()
        self.stop_signal = threading.Event()
        self.client = None
        self.transport = ''
        self.connection_note = 'Iniciando MQTT...'
        self.connected_since = None
        self.data = None
        self.data_at = None
        self.node_heartbeat_at = None
        self.presence_hint = 'desconocido'
        self.events = deque(maxlen=150)
        self.security_events = 0
        self.rejected_telemetry = 0
        self.last_seq = 0
        self.group_seq = {}
        self.last_actuation = None
        self.command_results = {}
        self.last_request = None
        self.send_heartbeat = True
        self.recent_status = deque(maxlen=48)
        self._last_error_at = 0

    def add_event(self, etype, message, category='system', seq=None, timestamp=None):
        with self.lock:
            entry = {
                'id': uuid.uuid4().hex[:12],
                'at': time.time() if timestamp is None else timestamp,
                'type': str(etype)[:70],
                'message': str(message)[:250],
                'category': category,
                'seq': seq,
            }
            self.events.appendleft(entry)
            if category == 'security':
                self.security_events += 1

    def _on_connect(self, client, _userdata, _flags, reason_code, _properties):
        if reason_code == 0:
            self.connected.set()
            with self.lock:
                self.connection_note = f'MQTT conectado ({self.transport})'
                self.connected_since = time.time()
            for topic in (self.cfg['topic_datos'], self.cfg['topic_paquete'], self.cfg['topic_alerta'], self.cfg['topic_estado']):
                client.subscribe(topic, qos=1)
            self.add_event('MQTT', 'Broker conectado; suscrito a D4')
            print('[MQTT] Conectado. Escuchando D4.')
        else:
            with self.lock:
                self.connection_note = f'Broker rechazó conexión: {reason_code}'

    def _on_disconnect(self, _client, _userdata, _flags, reason_code, _properties):
        self.connected.clear()
        with self.lock:
            self.connection_note = f'MQTT desconectado ({reason_code})'
        if not self.stop_signal.is_set():
            self.add_event('MQTT', 'Conexión interrumpida; el cliente intentará recuperarla')

    def handle_payload(self, topic, payload):
        if topic == self.cfg['topic_estado'] and (
                payload == b'offline' or
                payload in (b'{"id":"nodo4","estado":"offline"}', b'{"estado":"offline"}')):
            with self.lock:
                self.presence_hint = 'offline_lwt_no_firmado'
            self.add_event('LWT', 'Se recibió aviso offline sin firma; es orientativo, no autenticado')
            return
        try:
            doc = json.loads(payload.decode('utf-8'))
            if topic == self.cfg['topic_datos']:
                if not verify_group_node(doc, topic, self.cfg):
                    raise ValueError('HMAC por sensor inválida')
                with self.lock:
                    sensor, seq = doc['sensor'], doc['seq']
                    if seq <= self.group_seq.get(sensor, 0):
                        raise ValueError('Replay en sensor del grupo')
                    self.group_seq[sensor] = seq
                return  # panel individual recibe el paquete completo /paquete
            if not verify_node(doc, topic, self.cfg):
                raise ValueError('HMAC del nodo inválida')
        except (UnicodeDecodeError, ValueError, TypeError, KeyError):
            with self.lock:
                self.rejected_telemetry += 1
                should_log = self.rejected_telemetry <= 3 or self.rejected_telemetry % 10 == 0
            if should_log:
                self.add_event('FIRMA', f'Telemetría descartada: firma o estructura inválida ({topic.rsplit("/",1)[-1]})', 'security')
            return
        now = time.time()
        if topic == self.cfg['topic_paquete']:
            if doc.get('valor') not in ('ON', 'OFF'):
                return
            with self.lock:
                self.data = {k: doc.get(k) for k in ('id', 'ts', 'valor', 'intensidad', 'confirmado', 'seguro', 'riesgo', 'seq', 'calidad')}
                self.data_at = now
                self.recent_status.append({'at': now, 'intensidad': doc.get('intensidad'), 'lavador': doc.get('valor')})
        elif topic == self.cfg['topic_estado']:
            with self.lock:
                self.node_heartbeat_at = now
                self.presence_hint = 'online_firmado'
        elif topic == self.cfg['topic_alerta']:
            typ = doc['tipo']
            reason = doc['motivo']
            seq = doc['seq']
            cat = ('security' if typ in ('HMAC_INVALIDA', 'REPLAY_BLOQUEADO', 'APAGADO_BLOQUEADO', 'ORDEN_RECHAZADA', 'INTENSIDAD_BLOQUEADA', 'COMANDO_BLOQUEADO') else
                   'warning' if typ in ('FALLA_LAVADOR', 'CONFIRMACION_PERDIDA', 'MODO_SEGURO') else 'system')
            self.add_event(typ, f'{reason} · SEQ {seq}', cat, seq=seq)
            if typ in ('COMANDO_EJECUTADO', 'COMANDO_BLOQUEADO'):
                with self.lock:
                    self.command_results[seq] = {'type': typ, 'cmd': reason, 'time': now}
                    if len(self.command_results) > 120:
                        oldest = next(iter(self.command_results))
                        del self.command_results[oldest]
                self.add_event('RESULTADO', f'{reason}: {typ}', 'system', seq)

    def _on_message(self, _client, _userdata, msg):
        self.handle_payload(msg.topic, msg.payload)

    def _new_client(self, mode):
        transport = 'websockets' if mode in ('WS', 'WSS') else 'tcp'
        new = mqtt.Client(mqtt.CallbackAPIVersion.VERSION2,
                          client_id='g1-d4-panel-' + uuid.uuid4().hex[:12], transport=transport)
        new.on_connect = self._on_connect
        new.on_disconnect = self._on_disconnect
        new.on_message = self._on_message
        if mode in ('TLS', 'WSS'):
            new.tls_set(cert_reqs=ssl.CERT_REQUIRED)
        if transport == 'websockets':
            new.ws_set_options(path='/mqtt')
        new.reconnect_delay_set(min_delay=2, max_delay=20)
        return new

    def mqtt_connect_worker(self):
        host = self.cfg['host']
        initial = (int(self.cfg['port']), PORT_MODES.get(int(self.cfg['port']), 'TCP'))
        modes = [initial] + [p for p in ((1883, 'TCP'), (8883, 'TLS'), (8884, 'WSS')) if p != initial]

        while not self.stop_signal.is_set():
            for port, mode in modes:
                if self.stop_signal.is_set():
                    return
                self.transport = f'{mode}:{port}'
                with self.lock:
                    self.connection_note = f'Conectando {host}:{port} ({mode})'
                print(f'[MQTT] Probando {host}:{port} por {mode}...', flush=True)
                candidate = None
                try:
                    with socket.create_connection((host, port), timeout=4):
                        pass
                    candidate = self._new_client(mode)
                    rc = candidate.connect(host, port, keepalive=30)
                    if rc != mqtt.MQTT_ERR_SUCCESS:
                        raise OSError(f'MQTT connect devolvio codigo {rc}')
                    self.client = candidate
                    self.connected.clear()
                    candidate.loop_start()
                    if self.connected.wait(9):
                        self.add_event('MQTT', f'Enlace establecido en {mode}:{port}')
                        print(f'[MQTT] Enlace activo por {mode}:{port}.', flush=True)
                        while not self.stop_signal.is_set():
                            if not self.connected.wait(5):
                                print('[MQTT] Se perdio el enlace; probando reconexion y otros puertos.', flush=True)
                                self.add_event('MQTT', 'Se perdio el enlace; recuperando conexion', 'warning')
                                break
                        if self.stop_signal.is_set():
                            return
                    else:
                        print(f'[MQTT] Sin confirmacion del broker en {mode}:{port}.', flush=True)
                        with self.lock:
                            self.connection_note = f'Sin respuesta MQTT en {mode}:{port}'
                except (OSError, RuntimeError, ValueError) as exc:
                    print(f'[MQTT] Fallo {mode}:{port}: {exc}', flush=True)
                    with self.lock:
                        self.connection_note = f'No conecta por {mode}:{port}: {exc}'
                finally:
                    if candidate is not None:
                        try:
                            candidate.disconnect()
                        except Exception:
                            pass
                        try:
                            candidate.loop_stop()
                        except Exception:
                            pass
                    if self.client is candidate:
                        self.client = None
                    self.connected.clear()
            if not self.stop_signal.is_set():
                print('[MQTT] Todos los puertos fallaron; nuevo intento en 8 segundos.', flush=True)
                self.stop_signal.wait(8)

    def send_command(self, cmd, *, valid_signature=True, tag='ORDEN'):
        cmd = str(cmd).upper()
        if cmd not in ALLOWED_CMDS:
            return {'ok': False, 'message': 'Comando no permitido'}
        if not self.connected.is_set() or self.client is None:
            return {'ok': False, 'message': 'No hay conexión MQTT con el broker'}
        with self.lock:
            self.last_seq = max(self.last_seq + 1, int(time.time() * 1000))
            seq = self.last_seq
        doc = sign_command(cmd, self.cfg['clave_gateway'], seq)
        if not valid_signature:
            doc['firma'] = '0' * 64
        return self._publish_command(doc, tag=tag, save_actuation=valid_signature and cmd in ACTUATION_CMDS)

    def _publish_command(self, doc, *, tag='ORDEN', save_actuation=False):
        if not self.connected.is_set() or self.client is None:
            return {'ok': False, 'message': 'MQTT desconectado'}
        payload = json.dumps(doc, ensure_ascii=False, separators=(',', ':'))
        pub = self.client.publish(self.cfg['topic_cmd'], payload, qos=1, retain=False)
        if pub.rc != mqtt.MQTT_ERR_SUCCESS:
            return {'ok': False, 'message': f'Error al publicar MQTT: {pub.rc}'}
        try:
            pub.wait_for_publish(timeout=5)
        except RuntimeError as exc:
            return {'ok': False, 'message': f'No se confirmó envío: {exc}'}
        if not pub.is_published():
            return {'ok': False, 'message': 'Sin confirmación de recepción del broker'}
        with self.lock:
            self.last_request = {'cmd': doc['cmd'], 'seq': doc['seq'], 'tag': tag, 'at': time.time(), 'state': 'BROKER_ACK'}
            if save_actuation:
                self.last_actuation = dict(doc)
        self.add_event('ENVÍO', f'{tag}: {doc["cmd"]} · broker ACK, falta respuesta D4', seq=doc['seq'])
        print(f'[ENVIO] {tag} {doc["cmd"]} SEQ={doc["seq"]}')
        return {'ok': True, 'message': 'Broker confirmó recepción; esperando aceptación de D4', 'seq': doc['seq'], 'cmd': doc['cmd']}

    def replay(self):
        with self.lock:
            last = dict(self.last_actuation) if self.last_actuation else None
            outcome = self.command_results.get(last['seq']) if last else None
        if not last:
            return {'ok': False, 'message': 'Primero ejecuta una orden ON u OFF auténtica desde este panel'}
        if outcome is None or outcome['type'] != 'COMANDO_EJECUTADO' or outcome['cmd'] != last['cmd']:
            return {'ok': False, 'message': 'La última orden ON/OFF aún no fue confirmada como ejecutada por D4'}
        return self._publish_command(last, tag='ATAQUE_REPLAY', save_actuation=False)

    def scenario_block(self):
        if not self.connected.is_set():
            return {'ok': False, 'message': 'MQTT desconectado'}
        threading.Thread(target=self._scenario_worker, daemon=True).start()
        return {'ok': True, 'message': 'Escenario iniciado: riesgo alto → OFF auténtico; observa los eventos'}

    def _scenario_worker(self):
        first = self.send_command('RIESGO_ON', tag='ESCENARIO')
        if not first['ok']:
            return
        seq = first['seq']
        deadline = time.monotonic() + 6
        while time.monotonic() < deadline and not self.stop_signal.is_set():
            with self.lock:
                outcome = self.command_results.get(seq)
            if outcome and outcome['type'] == 'COMANDO_EJECUTADO':
                self.send_command('OFF', tag='PRUEBA_OFF_CON_RIESGO')
                return
            time.sleep(0.1)
        self.add_event('PRUEBA', 'No llegó confirmación de RIESGO_ON; no se envió OFF', 'warning')

    def heartbeat_loop(self):
        while not self.stop_signal.is_set():
            if self.send_heartbeat and self.connected.is_set():
                self.send_command('HB', tag='HB')
            self.stop_signal.wait(20)

    def set_heartbeat(self, enabled):
        self.send_heartbeat = enabled
        if enabled:
            self.send_command('HB', tag='HB_REANUDADO')
        self.add_event('GATEWAY', 'Heartbeat activado' if enabled else 'Heartbeat pausado', 'system')
        return {'ok': True, 'message': 'Heartbeat ' + ('activo' if enabled else 'pausado')}

    def snapshot(self):
        now = time.time()
        with self.lock:
            data = dict(self.data) if self.data else None
            last_data_age = round(now - self.data_at, 1) if self.data_at is not None else None
            hb_age = round(now - self.node_heartbeat_at, 1) if self.node_heartbeat_at else None
            status = (
                'desconectado' if not self.connected.is_set() else
                'sin_datos' if last_data_age is None else
                'actual' if last_data_age < 8 else 'atrasado'
            )
            result = dict(self.last_request) if self.last_request else None
            if result and result['seq'] in self.command_results:
                result['state'] = self.command_results[result['seq']]['type']
            return {
                'mqtt_connected': self.connected.is_set(),
                'mqtt_note': self.connection_note,
                'transport': self.transport,
                'data': data,
                'data_age_s': last_data_age,
                'node_heartbeat_age_s': hb_age,
                'presence_hint': self.presence_hint,
                'freshness': status,
                'events': list(self.events)[:60],
                'security_count': self.security_events,
                'rejected_telemetry': self.rejected_telemetry,
                'last_request': result,
                'gateway_heartbeat_enabled': self.send_heartbeat,
                'history': list(self.recent_status),
                'topics': {k.removeprefix('topic_'): self.cfg[k] for k in ('topic_cmd', 'topic_datos', 'topic_paquete', 'topic_estado', 'topic_alerta')},
            }


class HTTPHandler(BaseHTTPRequestHandler):
    server_version = 'G1D4Panel/1.0'

    def _headers(self, code, content_type, size):
        self.send_response(code)
        self.send_header('Content-Type', content_type)
        self.send_header('Content-Length', str(size))
        self.send_header('Cache-Control', 'no-store')
        self.send_header('X-Content-Type-Options', 'nosniff')
        self.send_header('Referrer-Policy', 'no-referrer')
        self.end_headers()

    def _respond_json(self, doc, code=200):
        payload = json.dumps(doc, ensure_ascii=False, separators=(',', ':')).encode('utf-8')
        self._headers(code, 'application/json; charset=utf-8', len(payload))
        self.wfile.write(payload)

    def do_GET(self):
        path = urlparse(self.path).path
        if path == '/api/state':
            self._respond_json(self.server.bridge.snapshot())
            return
        if path in STATIC:
            filename, mime = STATIC[path]
            source = ROOT / filename
            data = source.read_bytes()
            if path in ('/', '/index.html'):
                data = data.replace(b'__PANEL_TOKEN__', self.server.csrf_token.encode())
            self._headers(200, mime, len(data))
            self.wfile.write(data)
            return
        self._respond_json({'error': 'Ruta no encontrada'}, 404)

    def do_POST(self):
        if urlparse(self.path).path != '/api/action':
            self._respond_json({'ok': False, 'message': 'Ruta no encontrada'}, 404)
            return
        origin = self.headers.get('Origin')
        allowed_origins = {f'http://127.0.0.1:{self.server.server_port}', f'http://localhost:{self.server.server_port}'}
        if origin and origin not in allowed_origins:
            self._respond_json({'ok': False, 'message': 'Origen no permitido'}, 403)
            return
        if self.headers.get('X-Panel-Token') != self.server.csrf_token:
            self._respond_json({'ok': False, 'message': 'Token de sesión inválido'}, 403)
            return
        if self.headers.get('Content-Type', '').split(';')[0].strip().lower() != 'application/json':
            self._respond_json({'ok': False, 'message': 'Se requiere application/json'}, 415)
            return
        try:
            n = int(self.headers.get('Content-Length', '0'))
            if n <= 0 or n > 4096:
                raise ValueError('Tamaño inválido')
            body = json.loads(self.rfile.read(n).decode('utf-8'))
            action = body.get('action')
            bridge = self.server.bridge
            if action == 'cmd' and str(body.get('cmd', '')).upper() in ALLOWED_CMDS:
                result = bridge.send_command(str(body['cmd']).upper())
            elif action == 'ataque_falso':
                result = bridge.send_command('OFF', valid_signature=False, tag='ATAQUE_HMAC_INVALIDA')
            elif action == 'ataque_replay':
                result = bridge.replay()
            elif action == 'probar_bloqueo':
                result = bridge.scenario_block()
            elif action == 'heartbeat_pausar':
                result = bridge.set_heartbeat(False)
            elif action == 'heartbeat_reanudar':
                result = bridge.set_heartbeat(True)
            else:
                result = {'ok': False, 'message': 'Acción no permitida'}
            self._respond_json(result, 200 if result['ok'] else 409)
        except (ValueError, UnicodeDecodeError, json.JSONDecodeError, AttributeError) as exc:
            self._respond_json({'ok': False, 'message': f'JSON inválido: {exc}'}, 400)

    def log_message(self, fmt, *args):
        if self.path.startswith('/api/state'):
            return
        super().log_message(fmt, *args)


def main():
    parser = argparse.ArgumentParser(description='Panel local del Nodo D4')
    parser.add_argument('--port', type=int, default=8765, help='Puerto HTTP local, predeterminado 8765')
    args = parser.parse_args()
    cfg = load_config()
    bridge = GatewayBridge(cfg)
    threading.Thread(target=bridge.mqtt_connect_worker, daemon=True).start()
    threading.Thread(target=bridge.heartbeat_loop, daemon=True).start()
    token = secrets.token_urlsafe(32)
    server = ThreadingHTTPServer(('127.0.0.1', args.port), HTTPHandler)
    server.bridge = bridge
    server.csrf_token = token
    print(f'[PANEL] Abre en tu navegador: http://127.0.0.1:{args.port}')
    print('[SEGURIDAD] Solo localhost; el HTML no contiene las claves de HMAC.')
    print('[NOTA] Detén gateway_tester_puertos.py antes de ejecutar el panel para evitar heartbeats duplicados.')
    try:
        server.serve_forever(poll_interval=0.2)
    except KeyboardInterrupt:
        print('\n[PANEL] Cerrando...')
    finally:
        bridge.stop_signal.set()
        if bridge.client is not None:
            bridge.client.disconnect()
            bridge.client.loop_stop()
        server.server_close()


if __name__ == '__main__':
    main()
