# Dashboard G1 Pesca — GitHub Pages y modo local

El dashboard HTML/CSS/JS se publica automaticamente desde la carpeta `dashboard/` con GitHub Actions, cuando se fusionan cambios en `main` que afecten a `dashboard/**` o al workflow.

## Sitio publicado

- URL esperada: https://educmz.github.io/iot-g1-pesca-emisiones/
- Workflow: `.github/workflows/dashboard-pages.yml` (`Actions -> Publicar dashboard G1 Pesca en GitHub Pages`).
- Configuracion requerida: en `Settings -> Pages -> Build and deployment` seleccionar **GitHub Actions**.
- Tambien se puede iniciar el workflow manualmente desde GitHub Actions (`Run workflow`).

## Uso local

Abrir `dashboard/index.html` o servir la carpeta con `python -m http.server 8000` y entrar a http://localhost:8000.

El archivo `dashboard/js/config.public.js` incluye unicamente ajustes publicos:

- `mqttUrl: "wss://broker.emqx.io:8084/mqtt"`: WSS para navegadores. Los ESP32 de Wokwi usan el mismo broker, con TCP puerto 1883.
- `thingspeakEmbedUrl: ""`: por ahora desactivado porque el canal ThingSpeak del grupo (ID **3567916**) es **privado**. No incluir Write API Key, claves HMAC o claves de lectura privadas en HTML/JS.

El anterior `dashboard/js/config.example.js` se conserva como referencia de configuracion local. El sitio publicado usa `config.public.js`.

## Limitaciones y evidencias

1. El dashboard solo muestra datos que llegan mediante MQTT de los ESP32. No inventa lecturas; si no hay nodos activos, mostrara 'Sin datos'.
2. El dashboard **no verifica HMAC** en el navegador. Esa comprobacion se hace en el Gateway Fog. El panel no debe considerarse una fuente de confianza para actuacion.
3. Los botones de ataque publican peticiones de simulacion, pero los rechazos deben confirmarse con eventos reales.
4. El broker de pruebas es publico y puede desconectarse. La URL de Pages puede estar disponible incluso cuando MQTT esta OFF.
5. Falta sustituir las secciones de ejemplo con evidencias del equipo: captura general de Cirkit, resultados medidos A/B y graficas reales de ThingSpeak.
6. Para integrar ThingSpeak, mantener su **Write API Key solo en** `secrets.h` del Nodo 5 y autorizar expresamente cualquier visualizacion publica del canal.

## Diseño y gráficos en vivo

- Interfaz rediseñada con tema claro/oscuro (botón en la barra superior; respeta la preferencia del sistema).
- **Operación en vivo**: indicadores de decisión Fog (N5), gas del perímetro (N3), brújula de viento con el sector de la comunidad (90° ± 45°) y estado del lavador (N4); presencia de los 5 nodos (en línea si hubo mensaje en los últimos 35 s).
- Gráficos de series de tiempo (Chart.js 4.4.1 desde jsDelivr) con ventana de 1, 5 o 15 min: gas por zona (uno por nodo porque cada uno usa su propia unidad simulada, con la línea del umbral Fog), temperatura N1/N2, humedad N2, nivel de riesgo N5/N3 y actuadores (lavador, confirmación, ventilación).
- El eje de tiempo usa la **hora de recepción en el navegador**, porque los nodos envían `ts` como `millis()` desde su arranque.
- Botón **CSV de la sesión**: descarga los mensajes recibidos desde que se abrió la página (útil para `pruebas/`).
- Los gráficos solo guardan datos en memoria de la pestaña; al recargar la página empiezan vacíos. No hay datos de ejemplo ni modo demo.

## Pestañas

1. Problema
2. Solucion (arquitectura en cinco capas)
3. Operacion en vivo (MQTT de cinco nodos)
4. Ciberseguridad (pruebas y registro)
5. Resultados (ThingSpeak y comparacion A/B)
