# G1 PESCA
## Sistema IoT de monitoreo y control de emisiones en una planta de harina de pescado

**Desarrollo de Soluciones IoT · Entrega del laboratorio en 12 pasos**  
**Caso académico:** Pesquera Costa Norte S.A.C., Chimbote, Perú (empresa ficticia)  
**Integrantes:** Fernando (N1), Fabricio (N2), Gonzalo (N3), Pedro (N4) y Eduardo (N5, integración)  
**Plataformas:** ESP32, Wokwi, Cirkit Designer, MQTT, dashboard HTML y ThingSpeak  
**Fecha de elaboración:** noviembre de 2026

**Enlaces para demostración:** [Dashboard del sistema](https://educmz.github.io/iot-g1-pesca-emisiones/) · [Circuito general en Cirkit](https://app.cirkitdesigner.com/project/373d1c97-d707-40e2-8416-f6e8a5aca5a3) · [ThingSpeak, canal 3567916](https://thingspeak.com/channels/3567916) *(canal privado)*.

**Resumen ejecutivo.** Se desarrolló un prototipo IoT distribuido de cinco nodos ESP32 que supervisa indicadores de gas y condiciones de operación en las pozas, secadores y perímetro de una pesquera. Cada nodo conserva acciones locales de seguridad (Edge), mientras un Gateway Fog valida la autenticidad de las lecturas, combina varias fuentes y coordina el lavador de gases con confirmación de funcionamiento. La operación se observa mediante TFT, dashboard HTML/MQTT e historial de ThingSpeak. El grupo informa que los cinco proyectos Wokwi ya funcionan en conjunto. Las capturas que acompañarán esta entrega muestran la ejecución y los resultados obtenidos. Por tratarse de sensores simulados, el sistema constituye una demostración académica y no un instrumento certificado para H₂S o NH₃.

---

## Diagnóstico previo. Análisis y corrección del requerimiento del cliente

### Requerimiento recibido

> «Queremos un sistema con inteligencia artificial en la nube que controle todos los olores de la planta en tiempo real, para que los vecinos ya no se quejen. Tiene que ser 100 % confiable, barato y funcionar aunque se caiga el internet. Usen LoRa, que nos dijeron que es lo mejor».

### Ocho defectos identificados

| N.º | Tipo de defecto | Problema identificado | Reformulación verificable |
|---|---|---|---|
| 1 | Contradicción | La nube requiere conectividad; a la vez se exige operar sin Internet. | La protección local Edge funciona sin Internet; Cloud es complementaria. |
| 2 | No verificable | «100 % confiable» no admite una prueba realista de cumplimiento. | Definir pruebas de recuperación, modos seguros y detección de fallos. |
| 3 | Ambigüedad temporal | «En tiempo real» no fija latencia máxima. | Objetivo de detección a actuación de 5 s y medición mediante marcas temporales. |
| 4 | Ambigüedad económica | «Barato» no expresa presupuesto. | WAN con límite de S/ 150 mensuales. |
| 5 | Solución incompatible | Se impone LoRa cuando la auditoría del caso lo prohíbe. | Utilizar WiFi e Internet/MQTT, sin LoRa. |
| 6 | Alcance indefinido | «Todos los olores» no define fuentes, variables ni puntos de control. | Vigilar pozas, secadores, perímetro y tratamiento de gases. |
| 7 | Resultado no garantizable | Ningún prototipo garantiza eliminar todas las quejas vecinales. | Minimizar tiempo de respuesta y falsas actuaciones; registrar incidentes. |
| 8 | Omisión de seguridad y control | No se pide confirmar los actuadores ni proteger órdenes remotas. | HMAC, antirreplay, confirmación física simulada y actuaciones de respaldo. |

### Requerimiento final del sistema

Diseñar y validar en simulación un sistema IoT de cinco nodos ESP32 para detectar oportunamente indicadores de emisiones, ejecutar acciones locales, combinar datos en un Gateway Fog, ordenar la activación del lavador cuando dos fuentes corroboren el riesgo, confirmar su funcionamiento, responder a fallos en modo seguro y registrar la operación en un dashboard e historial Cloud, sin utilizar LoRa y respetando el límite presupuestal de la WAN.

### Contexto y pregunta de investigación

La pesquera ficticia procesa hasta **1 200 toneladas diarias de anchoveta** y se ubica a **800 m** de una zona residencial en Chimbote. En el escenario del laboratorio, las mediciones manuales mensuales no capturan picos de emisiones que duran minutos u horas. Se identifican tres causas: permanencia prolongada de materia prima en pozas, accionamiento manual del lavador y ausencia de datos del viento. El caso señala **14 quejas vecinales**, **dos procesos de fiscalización** y un posible costo de paralización cercano a **USD 90 000 por día**. Estas cifras pertenecen al caso, no al resultado del prototipo.

**Pregunta:** ¿conviene decidir la activación del lavador mediante un umbral fijo en Edge o mediante corroboración de varios sensores y viento en Fog para reducir la demora de respuesta sin generar falsas activaciones?

---

## Paso 1. Requisitos, tiempo máximo de respuesta, energía y autonomía

### 1.1. Seis requisitos funcionales (RF)

| ID | Requisito | Criterio de verificación |
|---|---|---|
| RF1 | Cada nodo adquiere sus variables y publica telemetría identificada. | Lecturas por nodo con unidad, calidad, fecha relativa y secuencia. |
| RF2 | La protección Edge actúa aun sin Internet. | Alarma o salida local frente a condición crítica sin MQTT. |
| RF3 | El Gateway Fog integra dos fuentes independientes, promedio móvil y viento. | Orden de tratamiento ante corroboración de riesgo, con datos autenticados. |
| RF4 | El lavador confirma la actuación; si falla, se habilita respaldo. | Confirmación en 10 s; en su ausencia, alerta y ventilación máxima de pozas. |
| RF5 | El sistema protege mensajes y comandos. | Rechazo de HMAC inválido, Replay y apagado peligroso. |
| RF6 | Se registra y presenta la operación. | TFT, cinco pestañas HTML y ThingSpeak actualizado desde el Gateway. |

### 1.2. Seis requisitos no funcionales (RNF)

| ID | Requisito | Objetivo o restricción |
|---|---|---|
| RNF1 | Tiempo de detección a actuación. | **Máximo de referencia: 5 s** en el escenario de prueba; medirlo, no confundirlo con el plazo de confirmación. |
| RNF2 | Continuidad segura. | Activación preventiva de N4 tras **más de 60 s** sin contacto autenticado con N5. |
| RNF3 | Red y costos. | WAN **≤ S/ 150/mes**; no utilizar LoRa. |
| RNF4 | Periodicidad de comunicación. | Lecturas nominales cada **1–2 s**, heartbeat/estado cada **30 s** y LWT `offline`. |
| RNF5 | Integridad y disponibilidad de registros. | HMAC-SHA256, control de secuencia y escritura ThingSpeak separada al menos **15 s**. |
| RNF6 | Condiciones ambientales y mantenibilidad. | Considerar corrosión salina, conectores protegidos, identificación de circuitos y secretos fuera del repositorio. |

### 1.3. Suministro de energía y dimensionamiento de respaldo

La simulación representa el funcionamiento lógico; no sustituye una medición eléctrica con instrumentos. Para dimensionar un **futuro piloto físico**, se elaboró el siguiente **presupuesto conservador de diseño**. Los valores son **supuestos de ingeniería, no consumos medidos**: incorporan ESP32, periféricos y carga de actuación estimada, especialmente el motor N4.

| Nodo | Potencia máxima de diseño asumida | Carga dominante |
|---|---:|---|
| N1 · Pozas | 1,30 W | ESP32, relé de ventilación y buzzer. |
| N2 · Secadores | 0,95 W | ESP32, MQ-2/DHT22 y LED RGB. |
| N3 · Perímetro | 0,85 W | ESP32, sensores simulados y LED. |
| N4 · Lavador | 7,20 W | ESP32, relé, driver A4988 y motor. |
| N5 · Gateway | 1,50 W | ESP32, TFT, RTC y NeoPixel. |
| **Total considerado** | **11,80 W** | **Valor para estimar respaldo, no para seleccionar una fuente definitiva.** |

**Escenario de batería de respaldo:** 12 V × 7 Ah = **84 Wh nominales**. Si se considera **60 % de energía utilizable** y **85 % de eficiencia**, quedan 84 × 0,60 × 0,85 = **42,84 Wh útiles**. Autonomía **estimada del conjunto** = 42,84 Wh / 11,80 W ≈ **3,6 h**. Este cálculo muestra un dimensionamiento preliminar; su resultado real depende de cargas, corrientes de arranque, conversores, batería y condiciones físicas. La fuente de N4 debe admitir los picos del motor de forma independiente.

**Solución adoptada para el prototipo:** alimentación simulada por Wokwi y lógica de operación segura sin WAN. **Solución proyectada para hardware real:** fuentes DC reguladas, convertidor para cargas del motor, protección eléctrica y respaldo dimensionado con las corrientes definitivas.

## Paso 2. Tipología del sistema y arquitectura de cinco capas

Se seleccionó una **arquitectura distribuida de publicación/suscripción**: cuatro nodos de campo realizan lectura y acción local; un quinto nodo opera como Gateway Fog; el broker MQTT transporta telemetría, estados y órdenes; la nube almacena el historial. El control es de **lazo cerrado** porque la salida del lavador se comprueba mediante un interruptor independiente.

| Capa | Componentes y funciones | Resultado |
|---|---|---|
| **Device** | MQ-2, DS18B20, DHT22, potenciómetro de viento, interruptor, relés, buzzer, LED, motor A4988, TFT y NeoPixel. | Señales y actuación simulada. |
| **Embedded** | Firmware ESP32: adquisición, unidades, tiempos, MQTT y firma. | Datos y comandos con formato común. |
| **Edge** | Protección local ante gas, pérdida de señal y condiciones inseguras. | Actuación sin dependencia de respuestas remotas. |
| **Fog** | N5 valida lecturas y HMAC, mantiene promedio móvil, combina fuentes y envía órdenes firmadas. | Control coordinado y gestión de fallas. |
| **Cloud** | ThingSpeak y dashboard HTML. | Historial, consulta y exposición de resultados. |

**Secuencia de control:** medición → decisión Edge → mensaje firmado → validación Fog → orden firmada → relé/motor del lavador → confirmación mediante interruptor → estado en TFT y dashboard. Ante ausencia de confirmación, N5 ordena ventilación máxima a N1.

**Límite reconocido:** Edge permanece operativo sin Internet; la coordinación entre simuladores situados en redes distintas depende del broker público. Para una implementación industrial independiente de la WAN haría falta un broker local y red de planta, no incluidos en la simulación académica.

![Figura 01. Arquitectura IoT en cinco capas](evidencias-entrega/figura-01-arquitectura-cinco-capas.png)

*Figura 01. Arquitectura Device–Embedded–Edge–Fog–Cloud y relación entre los cinco nodos.*

## Paso 3. Nodos, incertidumbre de sensores y exactitud de actuadores

La **resolución** de una interfaz no equivale a la **exactitud** de una medición. Se documentan los valores de referencia de componentes reales donde existen fichas técnicas, y se distingue lo que Wokwi solo representa de manera simulada.

| Nodo / responsable | Entradas y señales | Resolución e incertidumbre de referencia | Salidas y exactitud |
|---|---|---|---|
| **N1 Fernando — Pozas** | MQ-2 analógico GPIO34; DS18B20 digital 1-Wire GPIO15. | ADC ESP32: **12 bits (0–4095 cuentas)**. DS18B20: hasta **12 bits (0,0625 °C)** y exactitud de ficha **±0,5 °C** entre −10 y 85 °C. Gas MQ-2 **sin calibración para H₂S/NH₃**. | Buzzer GPIO16 y relé GPIO17, salidas ON/OFF; retardo real de conmutación no medido. |
| **N2 Fabricio — Secadores** | MQ-2 analógico GPIO34; DHT22 digital GPIO15. | ADC **0–4095**; porcentaje de gas es **relativo**, no concentración certificada. DHT22: resolución típica **0,1 °C / 0,1 %HR**, exactitud de referencia **±0,5 °C / ±2 %HR** en condiciones especificadas. | LED RGB GPIO25/26/27: tres niveles de señalización; no hay variable de posición o velocidad que requiera calibración mecánica. |
| **N3 Gonzalo — Perímetro** | MQ-2 analógico GPIO34; potenciómetro analógico GPIO35 para dirección del viento. | ADC 12 bits; rumbo digital ideal **360/4095 ≈ 0,088° por cuenta** antes de redondeos. Esto **no representa exactitud de una veleta real**. «ppm» es simulación. | LED GPIO26: advertencia de riesgo, conmutación digital. |
| **N4 Pedro — Lavador** | Interruptor de confirmación digital GPIO32. | Señal binaria confirmado/no confirmado; no mide caudal real de lavado. | Relé GPIO26; motor con A4988 STEP18/DIR19/ENABLE23. Con motor nominal de 200 pasos/vuelta, **1,8° por paso completo**, sin tolerancia mecánica validada. |
| **N5 Eduardo — Fog** | RTC DS1307 por I²C GPIO21/22 y pulsador GPIO25. | Hora de referencia de simulación; deriva de reloj físico no caracterizada. | TFT ILI9341 por SPI y NeoPixel GPIO32, salidas visuales, no instrumentos industriales calibrados. |

**Criterio de incertidumbre del prototipo:** el ADC aporta discretización digital, pero la incertidumbre total incluye sensor, conversión, temperatura, calibración y montaje. Por eso no se reporta como «exactitud de H₂S» o «exactitud de NH₃» una cifra derivada de los números simulados. Los valores de ficha técnica no certifican los dispositivos virtuales.

![Figura 02. Circuitos de medición de pozas, secadores y perímetro](evidencias-entrega/figura-02-nodos-sensores.png)

*Figura 02. Entradas, GPIO y componentes de N1, N2 y N3.*

## Paso 4. Requisitos de intercambio, WAN elegida, costo y exclusión de LoRa

La solución del laboratorio utiliza **WiFi más MQTT** sobre Internet. En Wokwi los ESP32 se conectan al broker `broker.emqx.io` por el puerto **1883**; la web usa **WebSocket seguro** `wss://broker.emqx.io:8084/mqtt`; N5 publica en ThingSpeak mediante **HTTPS**. Los cinco simuladores pueden ejecutarse desde computadoras y redes distintas porque comparten el mismo broker.

| Medio | Función | Consideración |
|---|---|---|
| WiFi/WAN | Conectividad del simulador. | Acceso sencillo, costo bajo y disponibilidad variable. |
| MQTT | Telemetría, estados, alarmas y comandos. | Mensajes pequeños y comunicación desacoplada. |
| WSS | Datos en vivo al dashboard HTML. | Evita usar MQTT TCP directo desde navegador. |
| HTTPS | Historial de datos desde N5 a ThingSpeak. | Escritura exclusiva del Gateway y credenciales privadas. |
| **LoRa** | **No utilizado.** | Prohibido expresamente por auditoría del caso. |

**Presupuesto mensual de diseño WAN:** se adopta una asignación máxima de **S/ 150** para un eventual piloto, no una factura ni cotización comercial: conexión de datos **S/ 90**, reserva para conectividad alternativa **S/ 30** y margen operativo **S/ 30**. **Total presupuestado: S/ 150/mes**, cumpliendo el techo del caso. El broker público y el canal de pruebas se tratan como servicios didácticos y **no se les atribuye una disponibilidad comercial garantizada**. La contratación real deberá mantenerse dentro de esa asignación.

Por la **corrosión salina**, un montaje futuro emplearía cajas con grado de protección adecuado, prensaestopas, conectores protegidos, tratamiento de contactos, mantenimiento preventivo y separación de la electrónica respecto a zonas húmedas.

## Paso 5. Usuarios finales e información integrada que requiere cada uno

| Usuario | Información que consulta | Decisión respaldada |
|---|---|---|
| Operador de planta | Lecturas por zona, temperatura, viento, estado Fog, lavador y confirmación. | Reconocer alarmas y verificar actuación efectiva. |
| Responsable ambiental y SST | Evolución de picos, riesgo hacia comunidad y eventos de seguridad. | Evaluar incidentes y condiciones de exposición. |
| Mantenimiento | Estado del relé/motor, confirmación, pérdida de heartbeat, modos seguros. | Diagnosticar fallas y recuperar tratamiento de gases. |
| Jefatura de operación | Tendencia histórica, tiempos de respuesta y falsas activaciones. | Comparar enfoques A/B y justificar mejoras. |
| Docente / evaluador del laboratorio | Estado de cinco nodos, actuación, evidencia HMAC y comparación A/B. | Reproducir pruebas y verificar resultados. |

La información se presenta según su finalidad: **operación inmediata** en el dashboard/TFT, **trazabilidad** en MQTT/eventos y **historia** en ThingSpeak.

## Paso 6. Servicios prestados a cada usuario

| Servicio | Beneficiario | Ejecución y resultado |
|---|---|---|
| Detección y alarma local | Operador | N1 activa buzzer/ventilación; N2 señaliza por RGB; N3 indica riesgo en perímetro. |
| Evaluación integrada Fog | Operador, SST | N5 valida firmas y combina dos fuentes, promedio y viento. |
| Control del lavador | Operador, mantenimiento | N5 ordena a N4; N4 acciona relé/A4988 y publica confirmación independiente. |
| Ventilación de respaldo | Mantenimiento, SST | Sin confirmación en 10 s, N5 envía `VENT_MAX` firmado a N1. |
| Modo seguro autónomo | Mantenimiento | N4 se activa preventivamente cuando supera 60 s sin contacto válido. |
| Historial y tendencias | Ambiental, jefatura | ThingSpeak actualiza indicadores recibidos de N5. |
| Telemetría y alertas en tiempo real | Operador, docente | Dashboard HTML conectado directamente a MQTT. |
| Auditoría de ciberseguridad | SST, docente | Registra HMAC inválido, Replay, mensaje incoherente y orden prohibida. |

**Límite de confianza:** que un mensaje aparezca en el dashboard no lo hace auténtico. Solo el firmware del Gateway toma decisiones con datos cuya firma, secuencia y calidad fueron verificadas.

## Paso 7. Contrato de interfaz MQTT: tópicos, mensajes, órdenes, heartbeat y LWT

### 7.1. Tópicos de integración

| Tópico | Productor / consumidor | Propósito |
|---|---|---|
| `upc/g1/nodoX/datos` | N1–N4 → N5/dashboard. | Una lectura firmada por sensor. |
| `upc/g1/nodoX/alerta` | Nodo → Gateway/web. | Fallas y alertas relevantes. |
| `upc/g1/nodoX/estado` | Nodo → dashboard. | Estado `online`, heartbeat de estado y LWT `offline`. |
| `upc/g1/nodoX/cmd` | N5 → nodo destinatario. | Orden o heartbeat firmado. |
| `upc/g1/seguridad/eventos` | Nodos/Gateway → dashboard. | Registro de ataques y decisiones. |

La X se reemplaza por 1, 2, 3, 4 o 5 según el nodo. El Gateway escucha `upc/g1/+/datos`, `upc/g1/nodo4/alerta` y `upc/g1/seguridad/eventos`.

### 7.2. Estructura y autenticidad

**Telemetría (campos):** `id`, `ts`, `seq`, `sensor`, `valor`, `unidad`, `calidad`, `firma`. La firma HMAC-SHA256 se calcula en el emisor a partir del material canónico:

```text
id|ts_millis|seq|sensor|valor_con_2_decimales|unidad|calidad
```

**Ejemplo de estructura (no es una lectura real ni contiene una firma válida):**

```json
{"id":"nodo1","ts":125000,"seq":42,"sensor":"gas_raw","valor":680,"unidad":"adc","calidad":"ok","firma":"FIRMA_ILUSTRATIVA"}
```

**Comando del Gateway:** el nodo receptor comprueba origen, HMAC, tiempo y secuencia estrictamente creciente antes de ejecutar. El material canónico de firma es:

```text
D5|ts_unix_segundos|seq|cmd|valor
```

Ejemplos de comandos: `VENT_MAX`/`VENT_AUTO` hacia N1; `HB` hacia N3 y N4; `ON`, `OFF`, `MAX`, `RIESGO_ON`/`RIESGO_OFF` hacia N4. El heartbeat de N3 utiliza `valor="1"`, mientras el de N4 utiliza `valor=""`, conforme al contrato implementado.

**Latido y última voluntad:** la disponibilidad se declara por mensaje de estado cada **30 s**, con LWT `offline` gestionado por el broker. Los heartbeats **firmados de control** del Gateway pueden seguir un periodo interno distinto y no reemplazan la necesidad de estado. Se utilizan Client IDs exclusivos y resuscripción después de reconectar.

**Seguridad:** secretos HMAC por nodo, verificación antes de actuar y control anti-Replay de secuencias. Las claves privadas y la API Key de ThingSpeak se mantienen fuera del repositorio. El broker público se usa para el laboratorio y no ofrece privacidad ni disponibilidad de nivel industrial.

## Paso 8. Diagrama eléctrico e integrado en Cirkit Designer

**Proyecto general del equipo:** [Abrir circuito Cirkit de G1 Pesca](https://app.cirkitdesigner.com/project/373d1c97-d707-40e2-8416-f6e8a5aca5a3).

El diseño integrado contiene las secciones de los cinco nodos. Para la revisión del circuito se distinguen las **entradas** (MQ-2 ADC, DS18B20 1-Wire, DHT22 digital, potenciómetro ADC, interruptor y RTC I²C) de las **salidas** (buzzer, relés, LED RGB, LED de perímetro, A4988, TFT SPI y NeoPixel). Cada dispositivo dispone de alimentación y referencia de tierra según su necesidad, mientras las conexiones MQTT representan comunicación lógica de red y no cables entre ESP32.

| Nodo | Señales principales a identificar en Cirkit |
|---|---|
| N1 · Pozas | MQ-2 GPIO34 (ADC), DS18B20 GPIO15 (1-Wire), buzzer GPIO16, relé GPIO17. |
| N2 · Secadores | MQ-2 GPIO34 (ADC), DHT22 GPIO15, RGB GPIO25–27. |
| N3 · Perímetro | MQ-2 GPIO34 (ADC), viento GPIO35 (ADC), LED GPIO26. |
| N4 · Lavador | Confirmación GPIO32; relé GPIO26; motor STEP18/DIR19/ENABLE23. |
| N5 · Gateway | DS1307 I²C GPIO21/22, botón GPIO25, TFT SPI y NeoPixel GPIO32. |

![Figura 03. Diagrama general de los cinco nodos en Cirkit Designer](evidencias-entrega/figura-03-cirkit-general.png)

*Figura 03. Diseño eléctrico integrado; entradas, salidas, alimentaciones y tipos de señal de los cinco nodos.*

## Paso 9. Los cinco proyectos ESP32 ejecutables en Wokwi

Los proyectos se encuentran identificados por responsable y función. Los enlaces siguientes corresponden a las versiones compartidas por el equipo:

| Nodo y responsable | Parte del problema que resuelve | Enlace de ejecución |
|---|---|---|
| **N1 · Fernando: pozas** | Detecta gas/temperatura, activa sirena y ventilación. | [Abrir Wokwi N1](https://wokwi.com/projects/479864540006804481) |
| **N2 · Fabricio: secadores** | Detecta gas, temperatura y humedad; indica riesgo RGB. | [Abrir Wokwi N2](https://wokwi.com/projects/479864093839705089) |
| **N3 · Gonzalo: perímetro** | Combina gas y viento; alerta por riesgo hacia comunidad. | [Abrir Wokwi N3](https://wokwi.com/projects/479866498817689601) |
| **N4 · Pedro: lavador** | Controla relé/motor, intensidad y confirmación independiente. | [Abrir Wokwi N4](https://wokwi.com/projects/479867720782481409) |
| **N5 · Eduardo: Gateway Fog** | Integra y valida telemetría, envía órdenes, muestra TFT y actualiza ThingSpeak. | [Abrir Wokwi N5](https://wokwi.com/projects/479700208383720449) |

**Modo de demostración:** iniciar N5 y conectar gradualmente N1–N4; comprobar recepción MQTT, contadores HMAC y presentación de datos en la TFT/dashboard. Para mostrar una situación crítica, modificar los sensores virtuales y observar la respuesta del lavador y las confirmaciones. El grupo reportó que los cinco simuladores ya funcionan; las figuras adjuntas permiten reconocer el funcionamiento observado en esta versión.

![Figura 04. Cinco simuladores Wokwi](evidencias-entrega/figura-04-wokwi-cinco-nodos.png)

*Figura 04. Vista de los cinco circuitos Wokwi y sus consolas en operación.*

## Paso 10. Reglas de procesamiento Edge y Fog, lazo cerrado y modo seguro

### 10.1. Actuación local Edge

- **N1:** una condición crítica de gas/temperatura activa ventilación de pozas y buzzer sin esperar un comando MQTT.
- **N2:** el estado de gas, temperatura y humedad determina la señalización RGB de advertencia.
- **N3:** el gas y la dirección del viento definen la indicación de riesgo en el perímetro; mensajes falsos no liberan el modo seguro.
- **N4:** el lavador rechaza apagados inseguros y, tras perder contacto autenticado por más de 60 s, se activa preventivamente.

### 10.2. Decisión coordinada Fog

El Gateway admite únicamente valores de nodos reconocidos con HMAC válida, secuencia apropiada y calidad aceptable. Mantiene **promedio móvil de tres muestras** y comprueba **frescura temporal** (ventana de referencia: 12 s). Las fuentes se comparan en **sus unidades originales**; no se convierten arbitrariamente ADC, % relativo y ppm simulados.

| Condición del simulador | Parámetro documentado |
|---|---|
| Pozas N1 | Gas **≥3820 ADC** o temperatura **≥45 °C**. |
| Secadores N2 | Gas relativo **≥60 %** o temperatura **≥50 °C**. |
| Perímetro N3 | Gas simulado **≥1000 ppm** o riesgo nivel 2. |
| Confirmación de riesgo | Al menos **dos fuentes independientes** elevadas. |
| Viento hacia comunidad | Sector entre **45° y 135°**; eleva criticidad si hay corroboración. |
| Datos ausentes | Estado conservador; no emitir un apagado por falta de lectura. |

### 10.3. Lazo cerrado del lavador

1. Dos fuentes elevadas y el contexto del viento justifican la intervención Fog.
2. N5 firma una orden de encendido o intensidad hacia N4.
3. N4 acciona relé/motor y publica por separado el **estado de control** y la **confirmación del interruptor**.
4. Si la confirmación no aparece dentro de **10 s**, se registra `FALLA_LAVADOR`.
5. N5 remite `VENT_MAX` firmado a N1; la ventilación de respaldo permanece activa mientras la condición segura no esté restablecida.
6. N5 presenta el estado y los eventos en TFT/NeoPixel/dashboard y publica datos válidos en ThingSpeak.

La **confirmación del interruptor** es distinta de haber publicado una orden `ON`: evita declarar que el tratamiento funciona cuando solo se envió el comando.

![Figura 05. Lazo cerrado y confirmación del lavador](evidencias-entrega/figura-05-lazo-cerrado-lavador.png)

*Figura 05. Orden del Gateway, acción en N4, confirmación y ventilación máxima de respaldo en N1.*

## Paso 11. Comparación de alternativas A (Edge) y B (Fog)

### 11.1. Estrategias de decisión

| Criterio | **A — Umbral fijo en Edge** | **B — Corroboración en Fog** |
|---|---|---|
| Lugar de decisión | En el nodo al superar un umbral. | N5 verifica múltiples nodos. |
| Información | Principalmente señal local. | Dos fuentes, promedio móvil y viento. |
| Fortalezas de diseño | Menor recorrido de decisión; protección inmediata local. | Reduce dependencia de una lectura aislada y añade contexto. |
| Riesgo de diseño | Un pico espurio puede activar una respuesta innecesaria. | Una decisión corroborada puede tardar más o depender de MQTT. |
| Seguridad | Debe mantener alarma local y autenticación de órdenes. | Verifica HMAC, secuencias y estado seguro. |
| Uso recomendado | Alarmas y protección local de reacción rápida. | Coordinación del lavador a escala de planta. |

### 11.2. Medición experimental con datos propios

Se comparan A y B sobre **los mismos estímulos**: normalidad, pico de emisión simulado, lectura falsa, viento hacia/desde comunidad y fallo de confirmación. Para cada corrida se registran el instante del estímulo, de detección y de actuación **en el actuador**, no solamente la publicación de un comando.

- **Latencia detección–actuación:** `t_actuación - t_detección` (segundos).
- **Falsa activación:** encendido del lavador cuando la corrida no contiene un pico válido.
- **Pico no detectado:** pico simulado reconocido en el escenario sin actuación en el periodo de observación.
- **Sensibilidad:** se repite la prueba después de cambiar un umbral o una ventana de promedio y se compara su efecto.

![Figura 06. Resultados experimentales de la comparación A frente a B](evidencias-entrega/figura-06-resultados-experimentales-a-b.png)

*Figura 06. Tabla o gráfico elaborado con los datos de las corridas del grupo: latencia (s), falsas activaciones y respuesta al cambio de parámetro.*

### 11.3. Conclusión de diseño

**Se adopta una solución híbrida:** **Edge** conserva la respuesta local inmediata ante una condición crítica y **Fog** decide la coordinación del lavador con corroboración de dos fuentes, promedio y viento. Esto satisface la seguridad del caso mejor que trasladar toda la actuación a la nube. La comparación numérica se sustenta **únicamente** en los registros experimentales presentados en la Figura 06; no se atribuyen cifras de latencia ni reducciones porcentuales que no estén registradas.

**Alternativa C (IA):** ampliación opcional. Ningún modelo predictivo sustituye las protecciones críticas de Edge ni el modo seguro de N4.

## Paso 12. Dashboard de cinco pestañas, ThingSpeak y pantalla TFT

**Dashboard público:** [Abrir G1 Pesca](https://educmz.github.io/iot-g1-pesca-emisiones/).  
**Canal histórico:** [ThingSpeak 3567916](https://thingspeak.com/channels/3567916) *(acceso privado para el grupo)*.

### 12.1. Cinco pestañas de la página

| Pestaña | Contenido | Fuente |
|---|---|---|
| **1. Problema** | Contexto de la planta, causas, cifras y pregunta de investigación. | Contenido del caso. |
| **2. Solución** | Arquitectura en cinco capas, Cirkit y papel de cada nodo. | Documentación técnica del equipo. |
| **3. Operación en vivo** | Lecturas, estados, actuación, viento, alertas y confirmación del lavador. | MQTT WebSocket. |
| **4. Ciberseguridad** | Ataques iniciados desde web y eventos de rechazo con motivos. | MQTT y firmware. |
| **5. Resultados** | Comparación A/B, gráficos y consulta de histórico. | Datos propios y ThingSpeak. |

![Figura 07. Dashboard: operación en vivo de los cinco nodos](evidencias-entrega/figura-07-dashboard-operacion-en-vivo.png)

*Figura 07. Panel HTML con lecturas reales, estados y actuadores de los cinco nodos.*

### 12.2. Pantalla TFT del Gateway

N5 dispone de **TFT ILI9341**, **RTC DS1307**, pulsador de reconocimiento y **NeoPixel** como semáforo de estado de planta. Muestra el estado global, conectividad, lecturas autenticadas, condición del lavador y alertas. Cuando faltan datos válidos, muestra ausencia de lectura y no inventa valores cero.

![Figura 08. Pantalla TFT del Gateway Fog](evidencias-entrega/figura-08-tft-gateway-fog.png)

*Figura 08. Pantalla ILI9341 y NeoPixel de N5 mostrando el estado de operación y mediciones.*

### 12.3. Historial ThingSpeak

**Solo el Gateway N5** publica por HTTPS a intervalos nominales de **25 s** (superior al mínimo de **15 s** exigido). Las lecturas deben tener HMAC válida y cumplir la regla de frescura para aparecer en los campos de sensores. Una escritura se considera confirmada por un `entry_id` positivo, no solo por respuesta HTTP.

| Field | Indicador | Field | Indicador |
|---|---|---|---|
| 1 | Nivel de riesgo Fog (0 normal, 1 alerta, 2 crítico). | 5 | Dirección del viento N3 (grados). |
| 2 | Gas N1 (ADC). | 6 | Estado lavador N4 (0/1). |
| 3 | Gas relativo N2 (%). | 7 | Confirmación N4 (0/1). |
| 4 | Gas simulado N3 (ppm). | 8 | Ventilación de respaldo (0/1). |

![Figura 09. ThingSpeak: historial del canal 3567916](evidencias-entrega/figura-09-thingspeak-ocho-campos.png)

*Figura 09. Gráficos de ThingSpeak con valores históricos provenientes del Gateway y marcas temporales.*

---

## Ciberseguridad transversal: cuatro ataques y defensas

La seguridad se evalúa con ataques didácticos iniciados desde la pestaña **Ciberseguridad**. El evento debe contener **hora, nodo, tipo de ataque, acción y motivo**. Una pulsación del botón de ataque no cuenta como defensa hasta observar el rechazo efectivo en el firmware.

| Nodo | Ataque programado | Protección esperada |
|---|---|---|
| **N1 Pozas** | Lectura imposible o salto no plausible. | Validación de rango, salto máximo y autenticación. |
| **N2 Secadores** | Suplantar `nodo2` sin HMAC correcta. | Rechazar firma inválida; no usar esa lectura para decisiones. |
| **N3 Perímetro** | Publicar falsamente «aire limpio» con riesgo real simulado. | Verificar firma, Replay y coherencia con viento y otras fuentes. |
| **N4 Lavador** | Orden falsa de apagado o repetición de una orden firmada. | HMAC, secuencia estricta y bloqueo de OFF mientras el riesgo sea alto. |
| **N5 Gateway** | Consolidar los cuatro eventos. | Registrar decisiones, impedir acciones por mensajes no auténticos. |

**Resultado de seguridad que debe verse:** el lavador **permanece encendido** cuando recibe una orden falsa de apagado en una condición de riesgo, y la web muestra el motivo del rechazo.

![Figura 10. Eventos de ciberseguridad y rechazo de ataques](evidencias-entrega/figura-10-ciberseguridad-cuatro-ataques.png)

*Figura 10. Cuatro ataques, defensa, marca horaria, nodo, acción y motivo; demostración del lavador protegido.*

## Continuidad operativa y plan de contingencia

| Plan | Situación | Acción durante la demostración |
|---|---|---|
| **A** | MQTT WebSocket y ThingSpeak disponibles. | Mostrar el dashboard con lecturas en vivo, TFT y actualización cloud. |
| **B** | WebSocket bloqueado en el navegador. | Utilizar una red alternativa autorizada o consultar el histórico del canal por acceso privado del equipo, manteniendo la operación local. |
| **C** | WebSocket y Cloud no disponibles. | Ejecutar Wokwi N5, visualizar la TFT y mostrar el archivo de resultados locales con indicación de que es un registro histórico. |

La indisponibilidad de Internet **no** debe impedir las actuaciones de seguridad internas de los nodos. Los planes B y C son alternativas de demostración, no sustitutos de un enlace industrial redundante.

## Conclusiones

1. La solución separa **seguridad local Edge**, **coordinación Fog** e **histórico Cloud**, evitando depender de la nube para decisiones críticas.
2. La actuación no se considera completa hasta recibir una **confirmación independiente del lavador**, con ventana máxima de 10 s y respaldo por ventilación de pozas.
3. MQTT permite conectar los cinco simuladores, mientras HMAC-SHA256 y secuencias reducen el riesgo de usar mensajes falsificados o repetidos.
4. Los resultados ambientales corresponden a un **prototipo simulado**: el MQ-2 virtual y el potenciómetro no constituyen mediciones certificadas de H₂S, NH₃ ni viento real.
5. La estrategia final conserva **Edge para protección inmediata** y **Fog para decisión coordinada con dos fuentes y contexto del viento**; los datos propios A/B se presentan mediante la evidencia experimental indicada.

## Guion de demostración del grupo (10 minutos)

| Bloque | Participación | Contenido y pantalla |
|---|---|---|
| Apertura, **1 min 30 s** | Eduardo | Problema, cifras del caso y arquitectura en las pestañas 1 y 2. |
| Nodos, **25 s por integrante** | Fernando, Fabricio, Gonzalo, Pedro y Eduardo | Parte del problema, detección/actuación y ataque rechazado en pestañas 3 y 4. |
| Demostración integrada, **2 min 30 s** | Eduardo con el equipo | Pico simulado, dos fuentes, viento, orden al lavador, confirmación y respaldo. |
| Preguntas, **2 min** | Todos | Explicación de funcionamiento, decisiones y evidencias. |

La pauta del curso exige **intervención individual**. La contribución al proyecto se acredita mediante el historial auténtico de GitHub de cada integrante.

## Figuras incluidas en la entrega

Las figuras están referenciadas por **nombre de archivo definitivo**, de modo que al sustituir la imagen correspondiente se actualizan automáticamente sus apariciones en este README:

| N.º | Nombre de archivo | Contenido |
|---|---|---|
| 01 | `figura-01-arquitectura-cinco-capas.png` | Arquitectura y flujo de información. |
| 02 | `figura-02-nodos-sensores.png` | Circuitos N1–N3. |
| 03 | `figura-03-cirkit-general.png` | Captura del Cirkit general. |
| 04 | `figura-04-wokwi-cinco-nodos.png` | Los cinco simuladores funcionando. |
| 05 | `figura-05-lazo-cerrado-lavador.png` | Orden, actuación, confirmación y respaldo. |
| 06 | `figura-06-resultados-experimentales-a-b.png` | Mediciones de latencia y falsas activaciones de A y B. |
| 07 | `figura-07-dashboard-operacion-en-vivo.png` | Dashboard de operación. |
| 08 | `figura-08-tft-gateway-fog.png` | TFT y NeoPixel del Gateway. |
| 09 | `figura-09-thingspeak-ocho-campos.png` | Datos y gráficos históricos de ThingSpeak. |
| 10 | `figura-10-ciberseguridad-cuatro-ataques.png` | Eventos de los cuatro ataques. |

**Fuentes de especificaciones físicas utilizadas solo como referencia de diseño:** ficha DS18B20 de Analog Devices; especificación DHT22/AM2302 de Aosong; documentación Arduino-ESP32 para ADC. Las lecturas de Wokwi y las potencias estimadas no constituyen certificación metrológica ni eléctrica. La empresa y los resultados del contexto pertenecen al caso ficticio del laboratorio.