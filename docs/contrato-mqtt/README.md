# Contrato MQTT G1 Pesca — borrador v1 (acuerdo antes de programar)

El laboratorio define la familia `upc/g1/nodo/datos`, `/alerta`, `/estado`, `/cmd`. Para identificar cada nodo, se propone reemplazar **`nodo`** por `nodo1` … `nodo5`:

| Tópico | Uso |
|---|---|
| `upc/g1/nodo1/datos` … `upc/g1/nodo5/datos` | Telemetría |
| `upc/g1/nodo1/alerta` … `upc/g1/nodo5/alerta` | Alertas |
| `upc/g1/nodo1/estado` … `upc/g1/nodo5/estado` | Online, heartbeat y LWT |
| `upc/g1/nodo1/cmd` … `upc/g1/nodo5/cmd` | Comandos hacia actuadores |
| `upc/g1/seguridad/eventos` | **Extensión propuesta:** registro validado de ataques para dashboard |
| `upc/g1/sim/ataque` | **Extensión propuesta:** solicitud de ataque desde el panel de pruebas |

Se publican lecturas individuales por **sensor** (campo extra), conservando los campos obligatorios `id, ts, valor, unidad, calidad, firma`. Ejemplo **ilustrativo de formato**, NO una lectura real ni firma válida:

```json
{"id":"nodo1","ts":125000,"sensor":"gas_raw","valor":680,"unidad":"adc","calidad":"ok","seq":42,"firma":"FIRMA_HMAC_REAL_CALCULADA_POR_EL_NODO"}
```

`ts`: milisegundos desde el arranque si el nodo no tiene RTC; el gateway agrega una hora de calendario obtenida de su RTC a los registros (separar `ts_origen` para no confundir). Campos adicionales `sensor` y `seq` se deben mantener uniformes en todos los nodos. **Firmas reales nunca se escriben a mano.**

### Estados

Heartbeat `cada 30 s`; configurar LWT retained con `{"id":"nodo1","estado":"offline"}`. Un mensaje de estado online al conectar debe reemplazarlo. **El LWT es generado por el broker, no es una prueba de firma de la aplicación.**

### Comandos y confirmación

Un comando **solo desde gateway** debe llevar: `id` (destinatario), `origen` (`nodo5`), `ts`, `comando`, `valor`, `seq`, `firma`. Ejemplo de estructura:

```json
{"id":"nodo4","origen":"nodo5","ts":130000,"comando":"lavador_on","valor":1,"seq":7,"firma":"FIRMA_GATEWAY"}
```

El nodo 4 verifica **firma + contador de secuencia creciente + regla de seguridad** y publica confirmación del **estado real observado por su interruptor**, no solo «orden recibida». El nodo 1 admite `ventilacion_max` firmada ante falla del lavador.

### HMAC y protección anti-replay

- Se propone HMAC-SHA256, clave distinta para cada nodo y otra para comandos del gateway, guardadas **fuera del repositorio**.
- Antes de implementar, acordar la serialización exacta de los campos y el método de codificación de `firma` (hex/base64) en todos los ESP32.
- No firmar JSON arbitrario sin canonización: diferencias de orden, decimal o espacios rompen la verificación.
- El receptor debe rechazar firmas inválidas, `seq` repetidos o menores y comandos que violen la seguridad; definir recuperación de contadores tras reinicio.
- La página HTML **nunca** almacena secretos HMAC. Los botones de ataque publican solicitudes de prueba, mientras los nodos/gateway registran el rechazo real. Un clic **no** es un ataque rechazado hasta recibir evento del firmware.

### Acceso y limitaciones

Broker público **EMQX** usado en la simulación actual: ESP32 `broker.emqx.io:1883`; dashboard WSS `wss://broker.emqx.io:8084/mqtt`. Las plantillas anteriores con HiveMQ se actualizaron. Verificar cada Wokwi, ya que no se sincroniza automáticamente con GitHub. Estado real y fallas: [diagnóstico](../diagnostico-mqtt-2026-11-05.md). El broker público **no asegura disponibilidad ni privacidad**. Los nodos deben actuar localmente sin WAN; el lavador mantiene el temporizador preventivo. Solo el gateway publica en ThingSpeak cada **15 s o más**.

**Pendiente de aprobación grupal:** convención exacta de firma, reloj, QoS, retained, contador tras reinicio y comandos. Documentar el acuerdo antes de integrar.
