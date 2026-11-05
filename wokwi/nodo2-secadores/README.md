# Entrega Fabricio — Nodo 2 Secadores

**Rama:** `nodo2-fabricio`  
**Wokwi:** https://wokwi.com/projects/479809534640024577  
**Cirkit:** https://app.cirkitdesigner.com/project/8ba81817-a151-413c-afa1-c8969ae08a86

## Qué está probado / qué requiere evidencia

- Visto en capturas: Wokwi y MQTT REAL funcionando, lecturas firmadas aceptadas, LED verde y amarillo, ataque HMAC inválido rechazado en backend individual.
- Pendiente de confirmar: LED rojo visible simultáneo con ALERTA; control de gas relativo 15/45/85 inyectado; circuito Cirkit eléctrico; integración con dashboard y gateway; mediciones A/B y entrenamiento opcional C.

## Archivos

- `sketch.ino`, `diagram.json`, `libraries.txt` y `config.example.h`: copiar al proyecto Wokwi existente. Crear `config.h` local desde plantilla, sin subirlo al repositorio.
- `backend/`: verificador HMAC + panel HTML individual + botón de ataque + puente OPCIONAL al panel grupal.
- `ia/`: script de alternativa C; datos reales de las simulaciones necesarios antes de entrenar.
- `evidencias/`: instrucciones sobre las capturas de prueba que debes guardar.

## Prueba de tres colores SIN desactivar gas

1. En `sketch.ino`, mantener `GAS_WARN = 30.0, GAS_ALARM = 60.0`; copiar este archivo al proyecto de Wokwi y reiniciarlo.
2. En Wokwi, abrir **Serial Monitor** y enviar un carácter seguido de Enter: `v` (gas 15%), `a` (45%), `r` (85%) o `m` (volver a la lectura directa del MQ-2 simulado).
3. Mantener la temperatura del DHT22 por debajo de 40°C. Esperar 2 s tras cada comando.
4. Verificar los colores: `v` verde NORMAL, `a` amarillo ADVERTENCIA, `r` rojo ALERTA.
5. En la consola verificar la etiqueta `PRUEBA_INYECTADA`, así como `ADC MQ2 simulado=...`. Los valores de demostración NO son una medición del gas físico.
6. Con `m` debe volver la lectura original del sensor MQ-2 simulado y los umbrales de gas siguen activos. Guardar captura.

## Prueba MQTT + HMAC

- En `backend/`: copiar `.env.example` a `.env`, usar mismo broker, MQTT_ROOT y clave de laboratorio que el `config.h` local. Poner `DEMO_MODE=0`, establecer una HMAC_KEY privada válida y ejecutar `node server.js`. Abrir http://127.0.0.1:8080.
- Verificar en el dashboard individual las tres lecturas después de `v/a/r`, su calidad y el estado.
- Abrir Ciberseguridad, pulsar **Simular suplantación** y verificar `HMAC_INVALIDO` / `RECHAZADO`.
- Ejecutar `node --test tests/*.test.js` en carpeta `backend/`.

## Integración

Antes de la corrección, Nodo 2 publicaba un paquete agrupado en `/datos`. Se conserva en `/paquete` y ahora `/datos` contiene lecturas individuales compatibles con el dashboard grupal. Consulta la sección de integración actualizada.

El backend local puede servir como puente opcional: poner `ENABLE_GROUP_ATTACKS=1` para recibir botón grupal desde `upc/g1/sim/ataque` y publicar rechazo en `upc/g1/seguridad/eventos`; `ENABLE_GROUP_FEED=1` publica datos validados y etiquetados individualmente en `upc/g1/nodo2/validado`. Requiere adaptación del `dashboard/js/app.js` por Eduardo. **El broker público no autentica ese nuevo tópico**: el gateway debe verificar la firma HMAC original antes de actuar.

No colocar claves en el HTML. No publicar `.env` ni `config.h` en GitHub.

## Advertencias

MQ-2 5 V a GPIO34 sin adaptación no es un montaje físico seguro si AOUT excede 3.3 V. El simulador Wokwi solo tiene soporte analógico limitado, especialmente para resistores, así que el divisor eléctrico debe ser validado en Cirkit como diseño de hardware y no asumirse simulado. El MQ-2 no mide H2S o NH3 directamente ni da ppm calibrados en este firmware.


## Integración grupal actualizada (noviembre 2026)

El ESP32 publica **una lectura firmada por sensor** en `upc/g1/nodo2/datos`: `gas_relativo` (% ADC, no ppm), `gas_raw` (ADC), `temperatura` (C), `humedad` (%) y `estado_secadores` (0 NORMAL, 1 ADVERTENCIA, 2 ALERTA, 3 FALLA_SENSOR). `/alerta` publica un estado firmado y `/estado` utiliza JSON para heartbeat y Last Will.

La firma grupal usa HMAC-SHA256 hex sobre `id|ts_millis|seq|sensor|valor(2 decimales)|unidad|calidad`, como Nodo 3. El Gateway **debe verificar** la firma, pues el dashboard grupal solo presenta MQTT sin autenticar.

Para conservar el panel individual de Fabricio, también se publica el **paquete completo firmado HMAC v1** por `upc/g1/nodo2/paquete`. El backend individual verifica ese paquete y además analiza las firmas en `/datos` y `/alerta` para registrar mensajes inválidos. Un mensaje sin firma o de firma incorrecta no debe modificar el panel individual.

**Advertencias:** el anti-replay de telemetría grupal es secuencial durante la sesión del verificador; el reinicio debe resolverse conjuntamente con D5 para evitar rechazos legítimos o aceptar reenvíos viejos. La telemetría agrupada y por sensor tienen *serializaciones distintas*: no se deben mezclar. `config.h` y `.env` deben tener la misma clave privada de laboratorio; el backend en modo MQTT real rechaza claves por defecto o de ejemplo. El broker público no autentica los tópicos, y D5 aún no implementa validación de firmas.

Para las pruebas del botón de ataque grupal, configurar `ENABLE_GROUP_ATTACKS=1` (el backend de Fabricio debe estar ejecutándose en modo real). El intento se inyecta al paquete individual y el rechazo real es publicado por el verificador local en `upc/g1/seguridad/eventos`.

**Pendientes después del merge de archivos:** copiar el `sketch.ino` actualizado a Wokwi y comprobarlo allí, verificar las claves y firmas con D5, repetir las capturas de la nueva versión y completar la política anti-replay de reinicio. El merge de archivos **no significa que el sistema de cinco nodos esté validado**.
