# Reglas de detección, actuación y seguridad

## Reglas obligatorias

- **R1: Edge primero.** Lectura crítica validada ⇒ alarma/ventilación o señalización local; sin dependencia de MQTT.
- **R2: Fog coordinado.** Confirmar riesgo con **≥2 sensores** de nodos pertinentes, promedio móvil y viento hacia la comunidad ⇒ ordenar lavador.
- **R3: Lazo cerrado.** Desde orden al lavador esperar **≤10 s** una confirmación válida; si no llega, evento `falla_lavador` y comando firmado de ventilación máxima a pozas.
- **R4: Modo seguro.** El lavador **se enciende por precaución** cuando pasa **más de 60 s** sin contacto del gateway, usando temporizador local.
- **R5: Visibilidad.** Mostrar medición, decisión, orden, actuación, confirmación y alertas en dashboard.
- **R6: Seguridad.** Órdenes remotas solo firmadas por gateway y con contador superior al último aceptado; jamás apagar el lavador si la condición sigue siendo crítica.

## Parámetros experimentales pendientes de acuerdo

| Parámetro | Valor | Estado |
|---|---|---|
| Umbral de gas (unidades simuladas MQ-2) | Por definir | No figura en el PDF |
| Salto máximo aceptable entre lecturas | Por definir | No figura en el PDF |
| Ventana de promedio móvil | Por definir | No figura en el PDF |
| Intervalo de reporte de sensores | 1–2 s | Según laboratorio |
| Heartbeat / LWT | 30 s / `offline` | Obligatorio |
| Tiempo para detectar falla de lavador | 10 s | Obligatorio |
| Pérdida de gateway → encendido preventivo | >60 s | Obligatorio |
| Intervalo publicación ThingSpeak | ≥15 s | Obligatorio |

**Prueba docente:** cambiar un parámetro y repetir el experimento. Guardar valor antes/después, timestamps y el resultado; no usar cifras inventadas.

## Alternativas de la comparación

- **A:** la regla de activación se determina en Edge por umbral fijo; para respetar la autenticación, el gateway puede transportar la decisión al lavador mediante un comando firmado **sin aplicar confirmación de dos sensores**. Medir toda la latencia de extremo a extremo.
- **B:** la decisión de activar el lavador se determina en Fog con dos sensores, promedio y viento.
- Las actuaciones locales de seguridad R1 y R4 **se mantienen** incluso durante pruebas de B.
