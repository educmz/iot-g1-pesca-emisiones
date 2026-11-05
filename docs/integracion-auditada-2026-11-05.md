# Integración G1: correcciones candidatas y evidencia del 5 de noviembre de 2026

**Estado: integración NO aprobada.** Rama basada en `a62bf51935354d7ce431917eb5b32ce159a2ecab` (PR #19, sobre #18). Los cambios de esta rama NO están guardados en Wokwi. No fusionar hasta validar los originales y las pruebas pendientes.

## Acceso y versiones observadas

Sesión Wokwi: Fabricio Fabian Quispe Barzola. Los cinco proyectos muestran «This project is owned by a different user» al intentar guardar. Se cancelaron las copias y se deshicieron las ediciones de prueba. No se creó un proyecto sustituto ni se modificaron circuitos o credenciales. Se conservaron los archivos leídos en la sesión de trabajo antes de cualquier edición; el Gateway también se descargó como ZIP privado. No se versionan estos respaldos con credenciales.

| Nodo | Original obligatorio | Evidencia de código | Corrección candidata en GitHub | Guardado original |
|---|---|---|---|---|
| D1 | https://wokwi.com/projects/479864540006804481 | Contador RAM; ventilación solo tras comando; WiFi ya tiene reintento de 45 s | Reserva NVS, publicación periódica de ventilación, reconexión controlada | No, permiso denegado |
| D2 | https://wokwi.com/projects/479864093839705089 | Lógica coincide con main, salvo comentarios; WiFi.begin repetido cada 5 s; contador RAM | Reserva NVS compartida con /paquete, WiFi.begin solo al arranque | No, permiso denegado |
| D3 | https://wokwi.com/projects/479866498817689601 | Lógica coincide con main, salvo comentarios; contador RAM; eventos de seguridad sin firma | Reserva NVS, eventos SEC1 firmados, serialización limitada, reconexión WiFi | No, permiso denegado |
| D4 | https://wokwi.com/projects/479867720782481409 | Lógica coincide con main, salvo comentarios; secuenciaGrupo RAM | Reserva NVS de telemetría, comprobación de tamaño JSON | No, permiso denegado |
| D5 | https://wokwi.com/projects/479700208383720449 | Versión anterior a main: HTTPS síncrono, 20 s, keepalive 30 s/socket 4 s | Se conserva main: tarea HTTPS, 25 s, keepalive 20 s/socket 8 s; métricas NVS/TFT/loop, reintentos de respaldo, receptor SEC1 | No, permiso denegado |

Las cuatro claves HMAC de los emisores coinciden con las cuatro claves del Gateway. También coinciden las claves de comandos D1/D3/D4 con D5. Valores no registrados. Los cinco usan broker.emqx.io; se conserva el broker y los tópicos. Los Client IDs de nodos usan prefijo distinto y componente aleatorio. No se ha observado una colisión.

El `secrets.h` del Gateway guardado no contiene una Write API Key operativa ni una CA operativa para ThingSpeak. Los HTTP 200 comunicados por el usuario pertenecen a otra configuración/versión; no se reprodujeron con este original. No se altera la privacidad del canal 3567916.

## Diagnóstico: causas separadas

1. **TCP/MQTT:** D5 conectó, se suscribió y mantuvo heartbeats durante 2:07.236 de tiempo simulado sin caída registrada. Posteriormente se observó una desconexión y reconexión exitosa. No se conoce aún el componente que cerró TCP. Esto no prueba estabilidad de cinco minutos ni que HTTPS sea la causa. D1 mostró «WiFi Error: Connection to IoT Gateway wss://netwi.wokwi.com/ failed» mientras seguía leyendo sensores y actuando localmente. Es evidencia de fallo de la pasarela en esa ejecución, no prueba universal de la causa de D5. Sin captura de paquetes no se atribuye el cierre al broker.
2. **Lecturas:** los cuatro contadores de telemetría se reinician en RAM; D5 conserva máximos NVS y rechaza valores menores o iguales. La incompatibilidad está confirmada por código; su reproducción dinámica con reinicios queda pendiente. D1 no publicaba ventilación periódica. D3 publicaba eventos de seguridad no autenticados, que D5 no podía aceptar como verificados. Los datos expiran a los 12 s y no se cambia ese límite sin mediciones suficientes.

Fuente sobre la pasarela, no evidencia de una causa específica: https://docs.wokwi.com/guides/esp32-wifi (Wokwi clasifica la estabilidad de su gateway público como media).

## Contratos y alcance

- Telemetría sin cambios: `id|ts_millis|seq|sensor|valor(2)|unidad|calidad`.
- Comandos sin cambios: `D5|ts_unix_segundos|seq|cmd|valor`. HB D3 usa `"1"`; HB D4 usa `""`.
- Nuevo evento D3: `schema=SEC1`, `nodo=nodo3`, `ts` Unix segundos, `seq` entero, `ataque`, `accion=RECHAZADO`, `motivo`, `firma`. Material: `SEC1|nodo3|ts|seq|ataque|RECHAZADO|motivo`. D5 comprueba ventana temporal, HMAC y máximo persistente separado `sec3`, antes de contar el evento. No toma decisiones Fog a partir de eventos sin autenticar.
- Reserva anticipada de 256 secuencias uint32 en NVS: antes del primer uso se escribe el extremo superior; tras reinicio se salta el bloque reservado. Fallo de apertura/escritura o agotamiento bloquea publicación; Edge sigue activo. Se mantiene uint32 para compatibilidad con los paneles individuales.
- **Limitación de migración y borrado de NVS:** esto NO crea sesiones autenticadas ni soluciona pérdida completa de NVS. Si D5 conserva un máximo anterior superior al nuevo emisor, seguirá rechazando valores. No borrar ni rebajar automáticamente su máximo. Antes de desplegar se necesita aprovisionar un piso superior al máximo verificado, conservando NVS, o implementar y probar un protocolo de desafío de nueva sesión firmado. La rama no se considera solución completa de reinicios Wokwi con flash borrada.
- D5 mantiene persistencia por cada lectura aceptada; solo se instrumenta su duración. No se agrupan escrituras del receptor, porque perder el máximo tras un corte debilitaría anti-Replay.
- D5 conserva TFT completa hasta medir su costo. El respaldo requerido permanece enclavado; se separa envío publicado de confirmación posterior autenticada. Reintentos limitados a 5 s con secuencias nuevas. `publish()` no equivale a confirmación de actuación.
- HTTPS conserva cola por copia y cliente separado. Se descartan capturas que ya caducaron al comenzar el envío y se mide duración/stack/heap; el envío por Internet no garantiza tiempo de llegada. No se sustituyen sensores ausentes por cero.
- Dashboard: usa mensajes reales recibidos, marca caducidad a 12 s y declara que el navegador no valida HMAC. Nunca se ponen claves en JavaScript. No se afirma autenticación de extremo a extremo de un dashboard público sin backend verificador.

## Validación y pendientes

| Prueba | Resultado |
|---|---|
| A: D5 solo, sin HTTPS operativo | Ejecutada con original: 2:07.236 simulados sin desconexión registrada; una caída y reconexión observadas posteriormente |
| B: D5 + D1 | Intentada; D1 compila y ejecuta Edge, pero falla su conexión a pasarela; recepción HMAC conjunta pendiente |
| C, D, E | Pendientes de validar con firmware corregido |
| F: cinco nodos sin ThingSpeak, 5 min | Pendiente |
| G: cinco nodos con ThingSpeak, 5 min | Pendiente; faltan configuración operativa en original y permisos |
| Reinicio y corte de Internet | Reserva probada en C++ con NVS simulado; ensayo ESP32/Wokwi pendiente |
| Fog nativo | 9 escenarios aprobados; fog_logic.h intacto |
| Backend D2 | 20 pruebas aprobadas |
| Puente D4 | 8 pruebas aprobadas |
| Reserva de secuencias | Frontera del bloque, reinicio, fallo de escritura, fallo de apertura y overflow aprobados; implementación idéntica en D1–D4 |
| Compilación local ESP32 | Bloqueada: arduino-cli no alcanza los índices por la red del entorno; se prepara CI para D1/D2 y se conservan los de D3/D4/D5 |
| Acciones físicas, TFT con cuatro nodos, ocho campos ThingSpeak | No verificadas |

No debe fusionarse esta rama como integración concluida. Se necesita acceso de edición con el propietario de los originales y la configuración privada operativa de ThingSpeak, además de resolver y probar migración/pérdida de NVS y repetir las pruebas graduales. No compartir claves por tickets, PR o documentación pública.
