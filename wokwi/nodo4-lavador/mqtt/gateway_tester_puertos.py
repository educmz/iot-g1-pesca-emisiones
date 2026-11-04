"""Cliente individual de laboratorio: representa al gateway D5 para probar el Nodo 4.

Uso: pip install paho-mqtt
     python gateway_tester.py

No usar con servicios industriales, claves reales ni datos sensibles.
"""
import hashlib
import hmac
import json
import socket
import threading
import time
from pathlib import Path

import paho.mqtt.client as mqtt

CONFIG_PATH = Path(__file__).with_name("config.local.json")
with CONFIG_PATH.open(encoding="utf-8") as f:
    CONFIG = json.load(f)

TOPIC_CMD = CONFIG["topic_cmd"]
CLAVE = CONFIG["clave_gateway"].encode("utf-8")
_lock = threading.Lock()
_ultimo_seq = 0
_ultimo_mensaje_valido = None
_enviar_hb = True
_activo = True
_conectado = threading.Event()


def generar_mensaje(cmd, valor="", firma_valida=True):
    global _ultimo_seq
    with _lock:
        _ultimo_seq = max(_ultimo_seq + 1, int(time.time() * 1000))
        seq = _ultimo_seq
    msg = {
        "id": "D5",
        "ts": int(time.time()),
        "seq": seq,
        "cmd": cmd.upper(),
        "valor": str(valor),
    }
    canonico = f'{msg["id"]}|{msg["ts"]}|{msg["seq"]}|{msg["cmd"]}|{msg["valor"]}'
    msg["firma"] = hmac.new(CLAVE, canonico.encode("utf-8"), hashlib.sha256).hexdigest()
    if not firma_valida:
        msg["firma"] = "0" * 64
    return msg


def publicar(msg, etiqueta):
    if not _conectado.is_set():
        print("[ERROR] MQTT desconectado; no se publico el mensaje")
        return False
    carga = json.dumps(msg, separators=(",", ":"), ensure_ascii=False)
    info = client.publish(TOPIC_CMD, carga, qos=0, retain=False)
    if info.rc != mqtt.MQTT_ERR_SUCCESS:
        print(f"[ERROR] Fallo MQTT rc={info.rc}")
        return False
    print(f'[ENVIO] {etiqueta} CMD={msg["cmd"]} SEQ={msg["seq"]}')
    return True


def enviar_valido(cmd):
    global _ultimo_mensaje_valido
    msg = generar_mensaje(cmd)
    ok = publicar(msg, "FIRMADO")
    if ok:
        with _lock:
            _ultimo_mensaje_valido = dict(msg)
    return ok


def on_connect(client_, userdata, flags, reason_code, properties):
    if reason_code == 0:
        _conectado.set()
        for topic in (CONFIG["topic_estado"], CONFIG["topic_datos"], CONFIG["topic_alerta"]):
            client_.subscribe(topic)
        print(f"[MQTT] Conectado. Escuchando las salidas de D4.")
    else:
        print(f"[MQTT] Conexion rechazada: {reason_code}")


def on_disconnect(client_, userdata, disconnect_flags, reason_code, properties):
    _conectado.clear()
    if _activo:
        print(f"[MQTT] Conexion interrumpida: {reason_code}")


def on_message(client_, userdata, msg):
    texto = msg.payload.decode("utf-8", errors="replace")
    try:
        doc = json.loads(texto)
        tipo = doc.get("tipo", "")
        resumen = f'id={doc.get("id")} valor={doc.get("valor")}'
        if tipo:
            resumen += f' tipo={tipo} motivo={doc.get("motivo")}'
        elif msg.topic.endswith("/datos"):
            resumen += f' confirmado={doc.get("confirmado")} intensidad={doc.get("intensidad")} seguro={doc.get("seguro")}'
    except json.JSONDecodeError:
        resumen = texto
    print(f"\n[RECIBIDO] {msg.topic}: {resumen}")


def tarea_heartbeat():
    global _enviar_hb
    while _activo:
        if _enviar_hb and _conectado.is_set():
            enviar_valido("HB")
        for _ in range(20):
            if not _activo:
                return
            time.sleep(1)


client = None


def preparar_cliente(modo):
    tipo = "websockets" if modo in ("WSS", "WS") else "tcp"
    nuevo = mqtt.Client(
        mqtt.CallbackAPIVersion.VERSION2,
        client_id=f"g1-pesca-d5-tester-{int(time.time() * 1000)}",
        transport=tipo,
    )
    nuevo.on_connect = on_connect
    nuevo.on_disconnect = on_disconnect
    nuevo.on_message = on_message
    if modo in ("TLS", "WSS"):
        nuevo.tls_set()
    if tipo == "websockets":
        nuevo.ws_set_options(path="/mqtt")
    return nuevo


def conectar_con_alternativas():
    global client
    host = CONFIG["host"]
    modos_por_puerto = {1883: "TCP", 8883: "TLS", 8884: "WSS", 8000: "WS"}
    puerto_config = int(CONFIG["port"])
    opciones = [(puerto_config, modos_por_puerto.get(puerto_config, "TCP"))]
    for alternativa in [(1883, "TCP"), (8883, "TLS"), (8884, "WSS"), (8000, "WS")]:
        if alternativa not in opciones:
            opciones.append(alternativa)

    for puerto, modo in opciones:
        print(f"[RED] Probando {host}:{puerto} ({modo})...")
        try:
            with socket.create_connection((host, puerto), timeout=5):
                pass
        except OSError as exc:
            print(f"[RED] Sin acceso al puerto {puerto}: {exc}")
            continue

        try:
            intento = preparar_cliente(modo)
            _conectado.clear()
            intento.connect(host, puerto, keepalive=30)
            client = intento
            intento.loop_start()
            if _conectado.wait(timeout=8):
                print(f"[RED] MQTT confirmado por {modo} puerto {puerto}.")
                return True
            print(f"[RED] Sin confirmacion MQTT en {modo} puerto {puerto}.")
            intento.disconnect()
            intento.loop_stop()
        except (OSError, ValueError, RuntimeError) as exc:
            print(f"[RED] Fallo de conexion por {modo} puerto {puerto}: {exc}")
    print("[ERROR] No se pudo acceder al broker desde esta PC por TCP/TLS/WebSocket.")
    print("[AYUDA] Prueba una red diferente o zona Wi-Fi del celular; conserva Wokwi sin cambios.")
    return False


def main():
    global _activo, _enviar_hb
    print("G1 Pesca | Gateway de pruebas D5 -> Nodo 4")
    print("Broker publico exclusivo para laboratorio. No introducir credenciales reales.")
    print(f'Broker: {CONFIG["host"]}:{CONFIG["port"]}  Topic: {TOPIC_CMD}')
    print("Comandos: on, off, bajo, medio, max, riesgo_on, riesgo_off, hb")
    print("Ataques: ataque_falso, ataque_replay, probar_bloqueo")
    print("Conectividad: pausar_hb, reanudar_hb, salir")
    if not conectar_con_alternativas():
        return
    threading.Thread(target=tarea_heartbeat, daemon=True).start()
    try:
        while True:
            try:
                entrada = input("\nD5> ").strip().lower()
            except EOFError:
                break
            if entrada in ("salir", "exit", "quit"):
                break
            if entrada in {"on", "off", "bajo", "medio", "max", "riesgo_on", "riesgo_off", "hb"}:
                enviar_valido(entrada.upper())
            elif entrada == "ataque_falso":
                publicar(generar_mensaje("OFF", firma_valida=False), "ATAQUE_HMAC_INVALIDA")
            elif entrada == "ataque_replay":
                with _lock:
                    ultimo = dict(_ultimo_mensaje_valido) if _ultimo_mensaje_valido else None
                if ultimo is None:
                    print("Envia primero una orden autentica (por ejemplo ON)")
                else:
                    publicar(ultimo, "ATAQUE_REPLAY_MISMO_SEQ")
            elif entrada == "probar_bloqueo":
                enviar_valido("RIESGO_ON")
                time.sleep(0.5)
                enviar_valido("OFF")
            elif entrada == "pausar_hb":
                _enviar_hb = False
                print("[GATEWAY] HB desactivado; espera >60 s de tiempo Wokwi para activar el modo seguro")
            elif entrada == "reanudar_hb":
                _enviar_hb = True
                enviar_valido("HB")
                print("[GATEWAY] HB reactivado")
            elif entrada:
                print("Comando no reconocido")
    finally:
        _activo = False
        client.disconnect()
        client.loop_stop()


if __name__ == "__main__":
    main()
