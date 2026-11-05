# Dashboard – Nodo 4: Lavador de gases

Panel de supervisión y control del lavador de gases de Pesquera Costa Norte S.A.C.

El dashboard permite visualizar el estado del lavador, la confirmación de funcionamiento, la intensidad del motor y el nivel de riesgo de la planta.

Utiliza MQTT para recibir datos del ESP32 simulado en Wokwi y enviar órdenes firmadas mediante HMAC-SHA256.

Incluye controles de encendido, apagado y regulación del motor, además de pruebas de ciberseguridad contra firmas inválidas, ataques Replay y órdenes de apagado durante riesgo alto.

Para ejecutarlo, se requiere Python, las dependencias de `requirements.txt` y un archivo `config.local.json` creado a partir de `config.example.json`. Las claves HMAC no deben publicarse en el repositorio.

El panel se inicia ejecutando `bridge.py` y se consulta desde `http://127.0.0.1:8765`.


### Cambios de integración

El puente sirve ahora el archivo real `panel/panel-nodo4.html`, escucha los datos individuales firmados en `/datos` y recibe el paquete completo para su panel desde `/paquete`. Verifica ambos formatos HMAC. El estado `offline` es un LWT sin firma y solo indica disponibilidad.

Para ejecutarlo se crea `config.local.json` copiando `config.example.json`, pero reemplazando **ambas** claves (D4 y D5) por valores privados nuevos de al menos 24 caracteres; los valores concretos que aparecían en Git anteriormente NO deben reutilizarse. El archivo local está ignorado por Git. Ejecutar la simulación corregida en Wokwi y verificar que sus claves coincidan con las del puente.
