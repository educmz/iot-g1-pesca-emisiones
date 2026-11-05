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

- En `backend/`: copiar `.env.example` a `.env`, usar mismo broker, MQTT_ROOT y clave de laboratorio que el `config.h` local. Poner `DEMO_MODE=0`. Ejecutar `node server.js`. Abrir http://127.0.0.1:8080.
- Verificar en el dashboard individual las tres lecturas después de `v/a/r`, su calidad y el estado.
- Abrir Ciberseguridad, pulsar **Simular suplantación** y verificar `HMAC_INVALIDO` / `RECHAZADO`.
- Ejecutar `node --test tests/*.test.js` en carpeta `backend/`.

## Integración

El ESP32 publica `upc/g1/nodo2/datos`, `alerta`, `estado`, con un JSON de tres lecturas y firma HMAC. El contrato del equipo propone **un mensaje por sensor**; debe aprobarse/adaptarse.

El backend local puede servir como puente opcional: poner `ENABLE_GROUP_ATTACKS=1` para recibir botón grupal desde `upc/g1/sim/ataque` y publicar rechazo en `upc/g1/seguridad/eventos`; `ENABLE_GROUP_FEED=1` publica datos validados y etiquetados individualmente en `upc/g1/nodo2/validado`. Requiere adaptación del `dashboard/js/app.js` por Eduardo. **El broker público no autentica ese nuevo tópico**: el gateway debe verificar la firma HMAC original antes de actuar.

No colocar claves en el HTML. No publicar `.env` ni `config.h` en GitHub.

## Advertencias

MQ-2 5 V a GPIO34 sin adaptación no es un montaje físico seguro si AOUT excede 3.3 V. El simulador Wokwi solo tiene soporte analógico limitado, especialmente para resistores, así que el divisor eléctrico debe ser validado en Cirkit como diseño de hardware y no asumirse simulado. El MQ-2 no mide H2S o NH3 directamente ni da ppm calibrados en este firmware.
