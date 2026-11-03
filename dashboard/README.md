# Dashboard local — base de interfaz

Esta carpeta contiene un **esqueleto funcional de HTML/CSS/JS**, no una demostración del laboratorio terminada.

## Uso

1. Copia `js/config.example.js` a `js/config.js` y ajusta la URL WSS del broker **sin introducir secretos**.
2. Abre `index.html` en el navegador (o, para evitar restricciones del navegador, usa un servidor local: `python -m http.server 8000` desde el directorio `dashboard` y visita `http://localhost:8000`).
3. Comprueba el estado MQTT. Si el broker o la biblioteca externa no se carga, se mostrará desconectado.
4. Solo aparecerán valores si hay **mensajes reales** de Wokwi. La página no genera telemetría ficticia.
5. Los botones de ataques publican solicitudes de simulación a `upc/g1/sim/ataque`. **No garantizan rechazo**: cada nodo debe implementar la prueba y publicar un evento real en `upc/g1/seguridad/eventos`.
6. En Resultados, se presenta el canal ThingSpeak solo cuando hayas configurado su iframe en `config.js`. Debe contener resultados propios A/B (y C si se realiza).

## Pestañas exigidas

1. Problema (información fija del caso).
2. Solución (5 capas, nodos, captura Cirkit aún pendiente).
3. Operación en vivo (MQTT, 5 nodos).
4. Ciberseguridad (botones + registro real).
5. Resultados (ThingSpeak + comparaciones documentadas).

Los integrantes deben añadir contenido específico de su nodo; Eduardo integra y prueba. No incluir HMAC, passwords ni Write API keys en JS cliente. Si se usa ThingSpeak, poner solo información pública o clave de lectura según la configuración del canal.
