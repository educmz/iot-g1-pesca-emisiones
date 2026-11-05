# Integración ThingSpeak — G1 Pesca

**Estado al 05/11/2026:** D5 envió mediante HTTPS y ThingSpeak confirmó **HTTP 200 con entry_id>0** (varios registros). **No** está validada la subida de campos 2–7 bajo los cinco Wokwi simultáneos.

- Canal: **3567916**, privado: https://thingspeak.com/channels/3567916
- Solo **Gateway D5** escribe; Write API Key y certificado CA válidos se configuran en secrets.h privado y no se comparten.
- Intervalo del sketch de diagnóstico: **25 s**, mayor al mínimo académico de 15 s.
- Campo 1: Nivel riesgo Fog (0 normal, 1 alerta, 2 crítico).
- Campo 2: Gas Nodo 1 (ADC).
- Campo 3: Gas Nodo 2 (% relativo).
- Campo 4: Gas Nodo 3 (ppm simulado, NO calibrado).
- Campo 5: Dirección viento (grados).
- Campo 6: Estado lavador.
- Campo 7: Confirmación lavador.
- Campo 8: Ventilación de respaldo.

El Gateway transmite fields 1 y 8 siempre que exista WiFi, reloj válido, CA y API key. Solo agrega fields 2–7 si sus lecturas fueron **verificadas por HMAC** y se mantienen **frescas por 12 s**. No inventa ceros para datos ausentes.

HTTPS se envía desde una tarea FreeRTOS separada. Mensajes importantes del monitor: "[ThingSpeak] Preparado: N campos", "GUARDADO HTTP 200 entry_id=X", "NO GUARDADO" o "FALLO HTTPS". HTTP 200 con respuesta "0" NO confirma escritura.

**Incidencia histórica:** durante las pruebas anteriores, el Gateway sufrió desconexiones MQTT `state=-3` con WiFi conectado. El equipo informa que los cinco Wokwi ya funcionan; **falta subir capturas/logs de la prueba integrada y los campos 2–7** para dejar esa mejora validada documentalmente. Ver [diagnóstico anterior](../docs/diagnostico-mqtt-2026-11-05.md).

**Privacidad:** el canal es privado; el dashboard estático no puede incrustar sus gráficas sin una vía de lectura autorizada. Nunca publicar Write API Key, Read API Key privada ni HMAC en GitHub Pages.
