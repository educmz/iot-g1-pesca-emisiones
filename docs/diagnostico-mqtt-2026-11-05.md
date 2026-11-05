# Diagnóstico MQTT y ThingSpeak — G1 Pesca (5 de noviembre de 2026)

**Estado:** desconexiones MQTT sin resolver; la conexión HTTPS a ThingSpeak sí logró guardar registros. No confundir una conexión exitosa inicial con estabilidad.

## Enlaces y responsables

| Nodo | Responsable | Wokwi | Configuración |
|---|---|---|---|
| 1 — Pozas | Fernando | https://wokwi.com/projects/479819491217347585 | secrets.h: MQTT_HOST |
| 2 — Secadores | Fabricio | https://wokwi.com/projects/479809534640024577 | config.h: MQTT_BROKER, MQTT_ROOT |
| 3 — Perímetro | Gonzalo | https://wokwi.com/projects/479699850241480705 | secrets.h: MQTT_HOST |
| 4 — Lavador | Pedro | https://wokwi.com/projects/479710105894416385 | secrets.h: MQTT_HOST |
| 5 — Gateway | Eduardo | https://wokwi.com/projects/479700208383720449 | sketch.ino: MQTT_HOST; secrets.h: HMAC y ThingSpeak |

Los nodos pueden ejecutarse en laptops y redes diferentes. Wokwi NO se sincroniza con GitHub automáticamente: verificar la copia realmente guardada y ejecutada en cada enlace.

## Configuración del laboratorio

- Broker MQTT TCP de ESP32: broker.emqx.io:1883; dashboard WSS: wss://broker.emqx.io:8084/mqtt.
- Tópicos: upc/g1/nodo1/datos, upc/g1/nodo2/datos, upc/g1/nodo3/datos y upc/g1/nodo4/datos; D5 se suscribe a upc/g1/+/datos.
- Validación HMAC-SHA256 por nodo, anti-replay con máximo seq persistente en D5 y frescura de muestras de **12 segundos**.
- D5: reintento MQTT **15 s**, keepalive **20 s**, timeout de socket **8 s**, ThingSpeak **25 s** (sketch de diagnóstico incluido en main).
- Canal privado de ThingSpeak **3567916**, campos: 1 riesgo Fog, 2 gas N1 ADC, 3 gas N2 %, 4 gas N3 ppm **SIMULADO**, 5 viento grados, 6 lavador, 7 confirmación, 8 ventilación respaldo. Solo D5 escribe HTTPS con certificado CA verificado y Write API Key privada.
- HTTPS en tarea independiente (FreeRTOS); opción ENABLE_THINGSPEAK para diagnóstico. **No se ha demostrado estabilidad MQTT.**

## Evidencias del usuario

- WiFi conectado con IP 10.10.0.2; suscripción MQTT exitosa: datos=1 D4=1 seg=1.
- Desconexiones: "[MQTT] Conexion perdida: state=-3 WiFi=3 uptime=17 s" y a los 26 s; una ocurrió antes del primer envío HTTPS.
- ThingSpeak guardó registros reales: "GUARDADO HTTP 200 entry_id=1", 2, 3, 4 y 7 en diferentes ejecuciones. Con dos campos solamente se registraron fields 1 y 8.
- D5 recibió cuatro mensajes HMAC válidos del Nodo 1; en TFT: N1 ADC 3628, HMAC 4 y MQTT OFF. No demuestra lecturas constantes ni entrada Field 2 en ThingSpeak.
- N2/N3/N4 no están confirmados con recepciones frescas simultáneas de las cinco laptops.
- MQTT -3 en PubSubClient indica conexión perdida, mientras WiFi=3 significa WiFi conectado; no determina por sí solo qué componente cortó TCP.

## Plan de diagnóstico para Codex / Work

1. Leer main y las 5 URLs de Wokwi; contrastar el código realmente ejecutado vs repo y plantillas, sin revelar secretos.
2. Iniciar D5 solo con ENABLE_THINGSPEAK=false durante 120 s, recopilar conexión, state, heap, keepalive y frecuencia de mqtt.loop().
3. Iniciar N1 junto a D5 durante 120 s: confirmar MQTT RX, HMAC ACEPTADO, TFT N1 ADC fresco y cambios de seq; revisar publicador de N1.
4. Añadir N2, N3 y N4 secuencialmente. Medir intervalos, rechazos HMAC, restarts y Client IDs. No modificar todos los nodos a la vez.
5. Reactivar ThingSpeak con mismo broker y repetir: confirmar HTTPS 200 con cuerpo entry_id>0 y fields 2..7 provenientes de muestras frescas y firmadas.
6. Comparar broker público con otro broker bajo la misma carga (sin asumir mejoría), e investigar pasarela de red de Wokwi.
7. Comprobar desbordes/bloqueos, tareas FreeRTOS, heap, validación TLS, keepalive, tiempos de red, secuencia anti-replay en NVS, calidad/edad y unidad de sensores.
8. Conservar HMAC, anti-replay, fail-safe, actuación local y confirmación del lavador. Documentar evidencia y tests después de cada corrección.

**Seguridad:** no subir secrets.h, config.h, claves HMAC, passwords ni claves ThingSpeak; rotar claves expuestas. Nunca desactivar TLS ni firmar falsamente datos incompletos. El broker público TCP 1883 no da privacidad y no debe usarse para control industrial.
