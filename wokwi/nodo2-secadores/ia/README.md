# Alternativa A y C (colaboración con el grupo)

## Alternativa A

La decisión local en `wokwi/sketch.ino` es el umbral fijo: el LED RGB cambia de estado inmediatamente en el propio nodo. Al comparar alternativas, **el lavador** lo controla el nodo 4/gateway: tu aporte es la lectura de gas del secador, su timestamp y el disparo de alerta. Registra el instante del pico, la respuesta y falsos positivos; usar la misma secuencia de inyecciones para A y B.

## Alternativa C - bonificación

1. Arrancar `backend/server.js` en modo MQTT real (no `DEMO_MODE=1`) y el Wokwi por varios minutos; crear picos y tramos normales.
2. Descargar `/api/export.csv` cuando ya tengas bastantes lecturas etiquetables. Se guardan solo los mensajes HMAC aceptados.
3. Ejecutar `python ia/entrenar_alternativa_c.py ruta/al/csv` con Python 3 estándar.
4. El programa crea `archivo.modelo.json` con coeficientes de un clasificador logístico pequeño y métricas sobre el último 25 % de la serie. Predice si la lectura relativa de gas superará el 60 % en las siguientes tres muestras.
5. Presentar métricas de C frente al umbral local A y reglas combinadas B en **datos nuevos**. No afirmar que C es superior si las métricas no lo demuestran.

**Limitaciones:** los datos del MQ-2 virtual no son H₂S ni NH₃, y una serie de un único escenario puede causar sobreajuste. La C es opcional (bonificación grupal), no debe sustituir la alarma edge.
