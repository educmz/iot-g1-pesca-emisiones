# Nodo 3 — Perímetro

**Responsable:** Wilder Gonzalo Aliaga Urbina

## Componentes
MQ-2 (analógica), potenciómetro (viento simulado), LED indicador.

## Reglas y tareas
Convertir el potenciómetro a dirección de viento simulada y enviarla al gateway.

## Ataque y defensa
Mensaje falso «aire limpio»; rechazar por firma y coherencia con pozas/secadores/viento.

## Registro de entrega
- **Enlace Wokwi:** https://wokwi.com/projects/479699850241480705
- **Enlace Cirkit (sección de nodo):** https://app.cirkitdesigner.com/project/37ff5f4e-1d1d-4788-b2f4-8eea0eca768c
- **Publicación MQTT:** upc/g1/nodo3/datos (gas, viento, riesgo), alerta, estado; eventos en upc/g1/seguridad/eventos
- **Actuación comprobada:** LED fijo con gas ≥ 5000 ppm simulados y viento 45–135° (ver evidencias/02-riesgo-alto.png)
- **Ataque rechazado y registrado:** aire limpio falso rechazado por firma HMAC y coherencia (ver evidencias/03-ataque-rechazado.png)

Guía técnica, cableado y pruebas: [LEEME.md](LEEME.md) · Evidencias: [evidencias/](evidencias/)

## Revisión de seguridad antes de integrar (noviembre 2026)

- Firma de telemetría por sensor: HMAC-SHA256 sobre id|ts_millis|seq|sensor|valor(2)|unidad|calidad, compatible con el formato de nodos 1 y 2.
- Repetición: el ESP32 consume una sola vez el eco exacto de cada publicación propia; todo reenvío posterior, incluso firmado y reciente, se rechaza.
- Gateway: los mensajes públicos de nodo5/estado son orientativos y **nunca** pueden sacar al Nodo 3 del modo seguro.
- Heartbeat autenticado: D5 deberá enviar un JSON al tópico upc/g1/nodo3/cmd con id=D5, cmd=HB, valor="1", ts UTC en segundos, seq creciente y firma HMAC de D5|ts|seq|HB|1. Se usa la clave privada HMAC_KEY_GATEWAY y se persiste la secuencia en NVS.
- **Pendiente Gateway:** mientras D5 no publique estos latidos firmados, el Nodo 3 permanecerá en modo seguro, con umbral preventivo estricto.
- Lectura sospechosa o fuera de rango: LED fijo y riesgo ALTO de manera conservadora. El contexto no firmado de otros nodos se identifica como no autenticado.
- El panel HTML muestra mensajes MQTT, **no valida las firmas** (eso corresponde a firmware y Gateway).

### Pruebas pendientes de integración

Se conserva la evidencia previa. Repetir en Wokwi las pruebas de firma inválida, replay reciente, seguridad por fallo de sensor, los tres niveles de LED y heartbeat firmado de D5. El workflow de GitHub intenta compilar ESP32, pero **no sustituye la simulación física ni prueba la conexión con el Gateway real**.

### Circuito físico

El diagrama Wokwi contiene un MQ-2 simulado conectado a 3V3. Para montar un MQ-2 físico habitual, alimentar según el módulo (normalmente 5 V) y adaptar AOUT: divisor 10 kΩ en serie y 15 kΩ a tierra produce aproximadamente 3 V para una señal de 5 V. No copiar directamente el cableado simulado a hardware físico sin esa adaptación y revisión eléctrica. El diseño alojado en Cirkit debe ser actualizado por Gonzalo.
