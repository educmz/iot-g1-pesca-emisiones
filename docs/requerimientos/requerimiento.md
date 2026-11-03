# Requerimiento reescrito y verificable — G1 Pesca

Diseñar y validar por simulación un sistema IoT de **cinco nodos ESP32** que supervise indicadores de emisiones de gas en **pozas**, **secadores** y **perímetro** de una planta de harina de pescado, además de la dirección del viento y el estado de un **lavador**.

1. Cada nodo deberá ejecutar la actuación de seguridad local que le corresponda, sin esperar al gateway ni a internet.
2. El **gateway Fog** coordinará el lavador usando confirmación de **al menos dos sensores**, **promedio móvil** y dirección del viento hacia la comunidad.
3. Si el lavador no confirma que opera en **10 s**, el gateway registrará **«falla del lavador»** y ordenará **ventilación máxima** de las pozas.
4. Si el lavador pierde contacto con el gateway durante **más de 60 s**, deberá encenderse por precaución de modo autónomo.
5. La comunicación de integración será **MQTT** (HiveMQ público durante pruebas); los latidos se publicarán **cada 30 s** con LWT `offline`. Las órdenes remotas se verificarán con firma y contador.
6. El gateway enviará datos históricos a **ThingSpeak** con intervalo **no menor de 15 s**. El HTML local incluirá cinco pestañas y datos MQTT vivos en las pestañas de operación y seguridad.
7. La WAN costará como máximo **S/ 150 al mes**, no se usará **LoRa** y se considerará corrosión salina para un montaje futuro.
8. Se simularán y rechazarán cuatro tipos de ataques de la sección de ciberseguridad del laboratorio; los eventos mostrarán hora, nodo, ataque, acción y motivo.
9. Se medirán **tiempos de detección a actuación** y **falsas activaciones** con datos propios para comparar A (umbral fijo en Edge) y B (confirmación en Fog).
10. **Bonificación opcional C:** entrenar un modelo local de IA con datos propios, compararlo con A/B y mantener las protecciones críticas independientes del modelo y de internet.

**Verificación:** proyectos Wokwi por nodo, diagrama integrado Cirkit, logs/eventos MQTT, panel local, resultados registrados y demostración en vivo. Los umbrales numéricos no vienen definidos por el caso: deberán acordarse, documentarse y cambiarse en una prueba ante el docente.

> El MQ-2 simulado es un **proxy** de gas, no una medición específica calibrada de H₂S/NH₃.
