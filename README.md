# G1 Pesca — Monitoreo y control de emisiones de una planta de harina de pescado

**Desarrollo de Soluciones IoT | Entrega en 12 pasos y diagnóstico previo**  
**Caso:** Pesquera Costa Norte S.A.C. (ficticia), Chimbote · **Equipo:** Fernando, Fabricio, Gonzalo, Pedro y Eduardo  
**Tecnologías:** ESP32, Wokwi, Cirkit Designer, WiFi/MQTT, Edge/Fog, ThingSpeak y dashboard HTML.

> **Actualización del equipo (05/11/2026):** el grupo informa que los cinco nodos ya funcionan de manera integrada. El README todavía necesita **incorporar capturas y registros de esa prueba final** y reflejar las correcciones del fallo MQTT `state=-3`, que corresponde a las pruebas anteriores. Ya se registró el **enlace al Cirkit general**, pero faltan su captura exportada y validación eléctrica documentada. La comparación A/B y algunas métricas cuantitativas siguen pendientes en el repositorio. No se inventan resultados.

**Enlaces rápidos:** [Cirkit general](https://app.cirkitdesigner.com/project/373d1c97-d707-40e2-8416-f6e8a5aca5a3) · [Dashboard](https://educmz.github.io/iot-g1-pesca-emisiones/) · [N1](https://wokwi.com/projects/479864540006804481) · [N2](https://wokwi.com/projects/479864093839705089) · [N3](https://wokwi.com/projects/479866498817689601) · [N4](https://wokwi.com/projects/479867720782481409) · [N5 Gateway](https://wokwi.com/projects/479700208383720449) · [ThingSpeak privado](https://thingspeak.com/channels/3567916)

## Índice de entrega

- [Diagnóstico previo del requerimiento](#diagnóstico-previo-del-requerimiento-original).
- [Paso 1. Requisitos, tiempo máximo y energía](#paso-1-requisitos-del-sistema-tiempo-máximo-y-energía).
- [Paso 2. Tipología y cinco capas](#paso-2-tipología-y-arquitectura-en-cinco-capas).
- [Paso 3. Nodos, incertidumbre y exactitud](#paso-3-nodos-incertidumbre-de-sensores-y-exactitud-de-actuadores).
- [Paso 4. WAN, presupuesto y LoRa](#paso-4-requisitos-de-intercambio-wan-presupuesto-y-lora).
- [Paso 5. Usuarios finales](#paso-5-usuarios-finales-e-información-necesaria).
- [Paso 6. Servicios por usuario](#paso-6-servicios-por-usuario).
- [Paso 7. Contrato de interfaz MQTT](#paso-7-contrato-de-interfaz-mqtt).
- [Paso 8. Diagrama Cirkit](#paso-8-diagrama-en-cirkit-designer).
- [Paso 9. Los cinco Wokwi](#paso-9-proyectos-wokwi-de-los-cinco-esp32).
- [Paso 10. Edge, Fog y lazo cerrado](#paso-10-procesamiento-edge-fog-y-lazo-cerrado).
- [Paso 11. Comparación A y B](#paso-11-comparación-de-alternativas-a-y-b).
- [Paso 12. Dashboard, ThingSpeak y TFT](#paso-12-dashboard-html-thingspeak-y-pantalla-tft).
- [Ciberseguridad transversal](#ciberseguridad-transversal) · [Pendientes de evidencia](#estado-de-la-entrega-y-pendientes) · [Exposición](#exposición-y-contribuciones).

---

## Diagnóstico previo del requerimiento original

El cliente solicitó controlar «todos los olores» mediante IA en la nube, con confiabilidad del 100 %, costo bajo, respuesta en tiempo real, funcionamiento sin Internet y uso de LoRa. El grupo detectó **ocho defectos**:

| N.º | Defecto y tipo | Corrección necesaria |
|---|---|---|
| 1 | Contradicción: nube y autonomía sin Internet. | Distinguir actuaciones Edge locales de Cloud. |
| 2 | Imposibilidad de comprobar «100 % confiable». | Definir criterios realistas y pruebas repetibles. |
| 3 | «Tiempo real» sin límite. | Definir latencias máximas y medirlas. |
| 4 | «Barato» sin presupuesto. | Aplicar límite WAN de **S/ 150/mes** del caso. |
| 5 | LoRa impuesto y a la vez prohibido. | Respetar prohibición de auditoría y seleccionar otra WAN. |
| 6 | «Todos los olores» sin alcance. | Delimitar pozas, secadores, perímetro y variables observables. |
| 7 | Promesa de eliminar quejas vecinales. | Definir control de riesgos, no garantías sociales absolutas. |
| 8 | Sin autenticación ni lazo cerrado. | Incorporar HMAC, antirreplay, confirmación y modo seguro. |

**Requerimiento reformulado:** diseñar y simular cinco nodos ESP32 para supervisar gas, condiciones ambientales, viento y estado de un lavador; actuar localmente ante riesgo, coordinar decisiones con fuentes independientes en Fog, confirmar la operación del lavador y registrar información en la nube sin depender de ella para las protecciones locales. [Diagnóstico completo](docs/requerimientos/diagnostico.md) · [Requerimiento verificable](docs/requerimientos/requerimiento.md).

**Contexto académico:** la pesquera ficticia procesa hasta **1 200 t/día**, está a **800 m** de una zona residencial y mide emisiones manualmente solo cada mes. El caso reporta **14 quejas vecinales**, **dos procesos de fiscalización** y una eventual paralización de **USD 90 000/día**. Los datos describen el caso, **no son resultados del prototipo**. Las emisiones consideradas incluyen H₂S, amoníaco y olores, pero el MQ-2 de Wokwi **no mide estos gases con exactitud certificada**.

**Pregunta de investigación:** ¿en qué capa (Edge o Fog) y con qué estrategia conviene decidir la activación del lavador para minimizar la demora ante picos de emisión sin falsas alarmas que interrumpan la operación?

## Paso 1. Requisitos del sistema, tiempo máximo y energía

| ID | Requisito funcional o restricción | Verificación |
|---|---|---|
| RF1 | Nodos detectan y actúan localmente sin Internet. | Probar alarmas y modo seguro con MQTT desactivado. |
| RF2 | Gateway toma decisión Fog con ≥2 fuentes independientes, promedio y viento. | Pruebas de `fog_logic.h` y escena integrada. |
| RF3 | Lavador confirma su actuación o se emite `FALLA_LAVADOR` y `VENT_MAX`. | Límite del caso: **10 s** sin confirmación. |
| RF4 | N4 enciende preventivamente sin heartbeat de D5. | Límite del caso: **más de 60 s**. |
| RF5 | Dashboard muestra detección, decisión, actuación y confirmación. | MQTT real, TFT y evidencia del lavador. |
| RF6 | Solo N5 escribe en ThingSpeak. | Publicaciones separadas **≥15 s**; firmware actual: **25 s**. |
| RF7 | Órdenes autenticadas y ataques rechazados. | HMAC-SHA256, secuencias crecientes y logs. |
| RNF1 | Respetar WAN ≤ S/150 al mes y prohibición de LoRa. | Presupuesto WAN documentado; actualmente pendiente de cotización. |
| RNF2 | Diseñar para entorno con corrosión salina. | Selección física de encapsulado/conectores pendiente. |

**Time-delay máximo:** el documento del caso **no define una latencia integral numérica**. Como objetivo **propuesto para medir y aprobar por el equipo**, usar **≤3 s** desde detección local hasta salida Edge y **≤5 s** desde corroboración Fog hasta publicación de orden. No son tiempos comprobados. Los valores de 10 s (confirmación) y >60 s (ausencia de heartbeat) **sí son obligatorios del caso**.

**Suministro, respaldo y autonomía:** el entorno Wokwi simula alimentación; **no mide consumo de energía real**. Un montaje futuro necesita fuente regulada, reserva de energía y adaptación de potencia para relés/motor, además de protección contra corrosión. **Pendiente** cuantificar consumos típicos y máximos de cada nodo, potencia del motor, capacidad de respaldo y autonomía. Dimensionamiento: `autonomía (h) ≈ capacidad útil (Ah) / corriente promedio (A)`, corrigiendo por eficiencia y cargas reales. No equiparar funcionamiento sin Internet con funcionamiento sin energía.

## Paso 2. Tipología y arquitectura en cinco capas

**Tipología:** sistema distribuido de nodos sensores/actuadores con decisiones Edge, comunicación MQTT por publicación/suscripción, coordinación central Fog y almacenamiento Cloud. La **topología lógica** es de nodos asociados a un broker MQTT; el Gateway se suscribe y envía órdenes seguras.

| Capa | Implementación | Responsable |
|---|---|---|
| Device | Sensores, interruptor, relés, buzzer, motor A4988, LED, NeoPixel, TFT. | Dueño de cada nodo. |
| Embedded | Firmware ESP32: muestreo, validación y MQTT. | Cada integrante. |
| Edge | Alarmas locales, ventilación y modo seguro sin esperar a Fog. | N1–N4. |
| Fog | Gateway N5, dos fuentes, promedio, viento, comandos y confirmación. | Eduardo. |
| Cloud | Histórico ThingSpeak y dashboard de presentación. | Equipo. |

**Lazo cerrado:** medir → detectar → verificar → decidir → ordenar → accionar → leer confirmación → mostrar evento. Si falla confirmación, D5 solicita respaldo a N1.

**Límite importante:** con Wokwi ejecutándose en cinco laptops y un broker público, **la coordinación MQTT entre equipos depende de Internet**. El caso exige autonomía de detección/actuación local; no se ha implementado un broker local industrial para mantener toda la coordinación Fog cuando caiga la WAN. [Arquitectura](docs/arquitectura/README.md).

## Paso 3. Nodos, incertidumbre de sensores y exactitud de actuadores

La calibración física no puede inferirse de Wokwi. La incertidumbre de un MQ-2 simulado **no es una incertidumbre certificada** de H₂S/NH₃; los «ppm» de N3 son valores simulados, no una lectura ambiental validada.

| Nodo / responsable | Entrada y señal | Incertidumbre / limitación | Actuación y exactitud disponible |
|---|---|---|---|
| **N1 Fernando — Pozas** | MQ-2 ADC GPIO34; DS18B20 1-Wire GPIO15. | Gas sin calibración física; error térmico no contrastado. | Buzzer GPIO16, relé ventilación GPIO17: conmutación binaria simulada, sin tiempo real medido. |
| **N2 Fabricio — Secadores** | MQ-2 ADC GPIO34; DHT22 digital GPIO15 (T y humedad). | Gas relativo % derivado de simulación; incertidumbre de T/H no medida. | RGB GPIO25–27: indicación de niveles de riesgo, sin calibración colorimétrica. |
| **N3 Gonzalo — Perímetro** | MQ-2 ADC GPIO34; potenciómetro GPIO35 para viento. | Gas en ppm simulados y rumbo de viento emulado, no veleta calibrada. | LED GPIO26: estado de riesgo simulado. |
| **N4 Pedro — Lavador** | Interruptor de confirmación GPIO32, digital. | Estado binario; no mide caudal ni eficiencia real de lavado. | Relé GPIO26, A4988 STEP18/DIR19/ENABLE23 y motor. El documento de N4 describe 200 pasos/vuelta (1,8° nominal por paso), sin error mecánico verificado. |
| **N5 Eduardo — Gateway** | Pulsador GPIO25 digital; RTC DS1307 I²C GPIO21/22. | Precisión RTC y rebote físico sin ensayo de calibración. | TFT ILI9341 SPI y NeoPixel GPIO32: indicación visual, no actuación industrial. |

**Pendiente de evidencia cuantitativa:** incluir por sensor rango, resolución, incertidumbre numérica sustentada y método de calibración; por actuador, tiempo, error y capacidad real. No inventar porcentajes de precisión. Evidencia individual: [N1](wokwi/nodo1-pozas/README.md) · [N2](wokwi/nodo2-secadores/README.md) · [N3](wokwi/nodo3-perimetro/README.md) · [N4](wokwi/nodo4-lavador/README.md) · [N5](wokwi/nodo5-gateway/README.md).

## Paso 4. Requisitos de intercambio, WAN, presupuesto y LoRa

| Requisito | Decisión | Justificación / límite |
|---|---|---|
| WAN | WiFi/Internet de los equipos Wokwi. | Prototipo distribuido sin misma LAN. |
| Mensajería | MQTT TCP `broker.emqx.io:1883`. | Liviano, tópicos por nodo, comandos y estado. Broker público compartido **sin SLA**. |
| Dashboard | MQTT WSS `wss://broker.emqx.io:8084/mqtt`. | Navegador recibe mensajes, no autentica HMAC. |
| Histórico | HTTPS hacia `api.thingspeak.com` solo desde N5. | Cloud no decide seguridad crítica. |
| Costo WAN | **≤S/150/mes**. | **Pendiente** cotizar acceso a red, conectividad y servicios bajo un escenario físico definido. No declarar costo cero sin evidencia. |
| LoRa | **No utilizar**. | Prohibición explícita de la auditoría en el caso. |
| Entorno salino | Protección/encapsulado adecuados en despliegue físico. | Wokwi no certifica resistencia ambiental. |

[Contrato de red](docs/contrato-mqtt/README.md) · [Diagnóstico de cortes MQTT](docs/diagnostico-mqtt-2026-11-05.md).

## Paso 5. Usuarios finales e información necesaria

Los perfiles siguientes se proponen a partir del caso; **no hay evidencia de entrevistas formales al cliente**.

| Usuario | Datos necesarios | Finalidad |
|---|---|---|
| Operador de planta | Gas, temperatura, riesgo, viento, lavador y confirmación. | Detectar anomalías y supervisar actuación. |
| Área ambiental / SST | Alertas, historia, picos y dirección del viento. | Investigar riesgo de emisiones hacia la comunidad. |
| Mantenimiento | Fallas, relés, motor, confirmación y pérdida de Gateway. | Diagnosticar y recuperar equipo de lavado. |
| Jefatura de operación | Tendencias, tiempos de respuesta y falsas activaciones. | Elegir estrategia A/B y valorar continuidad. |

## Paso 6. Servicios por usuario

| Servicio | Destinatario | Capa y resultado |
|---|---|---|
| Detección y alarma local | Operador | Edge N1/N2/N3: sirena, ventilación y baliza. |
| Detección de riesgo hacia comunidad | Ambiental | Edge N3 + Fog N5: gas y viento. |
| Coordinación del lavador | Operador / mantenimiento | Fog N5 → Edge N4: comando firmado, actuación y confirmación. |
| Ventilación máxima de respaldo | Mantenimiento / SST | Fog N5 → Edge N1: `VENT_MAX` firmado y estado observado. |
| Histórico y gráficos | Ambiental / jefatura | ThingSpeak, escritura solo desde Gateway. |
| Monitoreo y registro de seguridad | Todos según rol | HTML + eventos MQTT; HMAC se verifica en firmware. |

## Paso 7. Contrato de interfaz MQTT

**Broker de laboratorio:** `broker.emqx.io:1883`. Los cinco nodos necesitan **Client IDs distintos** y resuscripción tras cada reconexión.

| Tópico | Contenido |
|---|---|
| `upc/g1/nodo1/datos` a `upc/g1/nodo4/datos` | Telemetría HMAC individual por sensor. |
| `upc/g1/nodoX/alerta` | Alerta de nodo; D5 verifica las críticas de N4. |
| `upc/g1/nodoX/estado` | Latido, disponibilidad y LWT `offline`. |
| `upc/g1/nodoX/cmd` | Órdenes al actuador. |
| `upc/g1/seguridad/eventos` | Registro de ataques (compatibilidad HMAC aún pendiente entre todos). |
| `upc/g1/sim/ataque` | Solicitudes didácticas de ataque desde web. |

**Campos de telemetría:** `id`, `ts`, `seq`, `sensor`, `valor` numérico, `unidad`, `calidad`, `firma`. Canonización para HMAC-SHA256:

`id|ts_millis|seq|sensor|valor(2 decimales)|unidad|calidad`

**Ejemplo SOLO ilustrativo, firma NO válida:**

```json
{"id":"nodo1","ts":125000,"seq":42,"sensor":"gas_raw","valor":680,"unidad":"adc","calidad":"ok","firma":"FIRMA_ILUSTRATIVA_NO_VALIDA"}
```

**Firma de comandos D5:** `D5|ts_unix_segundos|seq|cmd|valor`. Los sensores usan `ts` relativo al inicio (`millis`), no segundos Unix. D3 acepta heartbeat firmado `HB` valor `"1"`; D4 usa `HB` valor `""`. HMAC y secuencia anti-Replay son obligatorios.

**Latido y LWT:** la rúbrica del caso indica **30 s y offline**. El firmware D5 documentado emite heartbeat de comando firmado cada 20 s y publica estado cada 30 s; no confundir ambas funciones.

**Limitación abierta:** D5 almacena en NVS el máximo `seq`; los contadores de D1–D4 pueden reiniciarse, provocando rechazos válidos por Replay. Resolver con persistencia/sesiones autenticadas, **no desactivar seguridad**. Claves solo en archivos locales ignorados por Git.

[Documento del contrato](docs/contrato-mqtt/README.md).

## Paso 8. Diagrama en Cirkit Designer

**[Ver circuito integrado del equipo en Cirkit Designer](https://app.cirkitdesigner.com/project/373d1c97-d707-40e2-8416-f6e8a5aca5a3)**.

Cada integrante aporta su diseño. **Se exige un diagrama general de los cinco nodos** con sensores/actuadores, GPIO, fuente y tipos de señal (ADC, GPIO, 1-Wire, I²C, SPI).

| Nodo | Diagrama individual disponible |
|---|---|
| N1 | [Cirkit Fernando](https://app.cirkitdesigner.com/project/75988b2c-c401-4ed8-ae5e-e7d56159e44f) |
| N2 | [Cirkit Fabricio](https://app.cirkitdesigner.com/project/8ba81817-a151-413c-afa1-c8969ae08a86) |
| N3 | [Cirkit Gonzalo](https://app.cirkitdesigner.com/project/37ff5f4e-1d1d-4788-b2f4-8eea0eca768c) |
| N4 | [Cirkit Pedro](https://app.cirkitdesigner.com/project/4663bde6-8941-4af3-a259-1a44f4c8d95b) |
| N5 | [Circuito documentado en Wokwi](wokwi/nodo5-gateway/diagram.json); ubicación Cirkit por confirmar. |

**Diagrama general compartido por el equipo:** [Abrir diseño integrado en Cirkit Designer](https://app.cirkitdesigner.com/project/373d1c97-d707-40e2-8416-f6e8a5aca5a3). **Enlace registrado.** Aún falta añadir a GitHub una **captura exportada del diagrama** y documentar la **revisión de señales, GPIO, alimentación y concordancia con Wokwi**. No se ha podido inspeccionar gráficamente el contenido del proyecto desde este acceso. [Carpeta general](cirkit/diagrama-general/README.md).

## Paso 9. Proyectos Wokwi de los cinco ESP32

**Enlaces vigentes proporcionados por el equipo el 05/11/2026:**

| Integrante | Parte del problema | Wokwi actual | Código en GitHub |
|---|---|---|---|
| Fernando — N1 Pozas | Emisión de materia prima; sirena y ventilación. | [Abrir N1](https://wokwi.com/projects/479864540006804481) | [Firmware](wokwi/nodo1-pozas/wifi-scan/wifi-scan.ino) |
| Fabricio — N2 Secadores | Gas, temperatura y humedad; RGB. | [Abrir N2](https://wokwi.com/projects/479864093839705089) | [Firmware](wokwi/nodo2-secadores/sketch.ino) |
| Gonzalo — N3 Perímetro | Gas, viento y comunidad; LED. | [Abrir N3](https://wokwi.com/projects/479866498817689601) | [Firmware](wokwi/nodo3-perimetro/sketch.ino) |
| Pedro — N4 Lavador | Relé, control A4988 y confirmación. | [Abrir N4](https://wokwi.com/projects/479867720782481409) | [Firmware](wokwi/nodo4-lavador/sketch.ino) |
| Eduardo — N5 Gateway | HMAC, decisión Fog, TFT, NeoPixel y ThingSpeak. | [Abrir N5](https://wokwi.com/projects/479700208383720449) | [Firmware](wokwi/nodo5-gateway/sketch.ino) |

**GitHub no sincroniza Wokwi automáticamente**: verificar versiones realmente guardadas, `diagram.json`, bibliotecas y configuración privada antes de exponer. [Carpeta Wokwi](wokwi/).

## Paso 10. Procesamiento Edge, Fog y lazo cerrado

**Edge:** N1 ventila y alarma en su origen; N2 y N3 señalizan peligro; N4 controla lavador y mantiene estado seguro sin depender de MQTT. **Fog:** D5 valida `id`, HMAC, `seq`, calidad, unidades y frescura (versión documentada: **12 s**); mantiene promedio móvil de **tres muestras** y compara fuentes independientes y viento.

| Variable / regla | Umbral documentado del simulador (no calibración física) |
|---|---|
| N1, pozas | gas ADC ≥3820 o T ≥45 °C. |
| N2, secadores | gas relativo ≥60 % o T ≥50 °C. |
| N3, perímetro | gas ppm simulados ≥1000 o riesgo nivel 2. |
| Corroboración | ≥2 fuentes independientes elevadas antes de ordenar lavador. |
| Viento hacia comunidad | 45°–135°; con ≥2 fuentes elevadas, estado crítico. |
| Lazo cerrado | Orden N4 → interruptor de confirmación → D5. |
| Fallo | Sin confirmación tras 10 s: alerta y `VENT_MAX` a N1. |
| Sin Gateway | N4 activa protección cuando supera 60 s sin contacto válido. |

**Secuencia que debe documentarse con evidencias:** pico en pozas + segunda fuente + viento a comunidad → D5 decide → orden firmada a D4 → relé/motor actúa → confirmación → visualización; negar confirmación → respaldo N1. Si faltan datos recientes, **no ordenar apagado inseguro**. El equipo informa que la integración ya funciona, pero aún se deben subir las evidencias de esa ejecución.

[Reglas](docs/reglas/README.md) · [Lógica y pruebas Fog](wokwi/nodo5-gateway/).

## Paso 11. Comparación de alternativas A y B

| | **A — Edge** | **B — Fog** |
|---|---|---|
| Dónde decide | En nodo, por umbral fijo. | En Gateway, con dos fuentes, promedio móvil y viento. |
| Hipótesis a verificar | Reacción rápida, posible susceptibilidad a alarmas aisladas. | Corroboración puede reducir falsas alarmas, con costo de latencia. |
| Tiempo detección → actuación | **PENDIENTE de medir (s)**. | **PENDIENTE de medir (s)**. |
| Falsas activaciones | **PENDIENTE de contar**. | **PENDIENTE de contar**. |
| Picos no detectados | **PENDIENTE**. | **PENDIENTE**. |
| Cambio de parámetro ante docente | **PENDIENTE**. | **PENDIENTE**. |

**Procedimiento:** repetir en A/B los mismos escenarios de normalidad, pico verdadero simulado, lectura falsa, viento hacia/desde comunidad y falta de confirmación. Guardar tiempos de inicio, detección y **actuación física simulada** (no solo publicación de orden). Calcular `latencia_s = (t_actuacion_ms - t_deteccion_ms) / 1000`. Falsa activación = lavador ON sin pico real. El docente pedirá cambiar un parámetro y repetir.

**No afirmar que A o B gana sin mediciones propias.** Guardar corridas en [CSV plantilla](pruebas/plantilla-resultados.csv), logs y evidencias en [pruebas/](pruebas/). **C — modelo IA propio — es opcional**, no reemplaza reglas críticas.

## Paso 12. Dashboard HTML, ThingSpeak y pantalla TFT

**Sitio publicado:** [Dashboard G1 Pesca](https://educmz.github.io/iot-g1-pesca-emisiones/) · [Código HTML](dashboard/) · [Configuración pública](dashboard/js/config.public.js)

| Pestaña | Qué muestra | Fuente |
|---|---|---|
| **1. Problema** | Contexto, cifras académicas y reto de investigación. | HTML. |
| **2. Solución** | Cinco capas, nodos y captura real de Cirkit cuando esté validada. | HTML/evidencias. |
| **3. Operación en vivo** | Lecturas, alertas, órdenes, lavador y confirmaciones. | MQTT. |
| **4. Ciberseguridad** | Botones de ataques y eventos de rechazo con causa. | MQTT y firmware. |
| **5. Resultados** | A/B, gráficos reales e historial ThingSpeak. | CSV del grupo/ThingSpeak. |

**TFT y operación local:** N5 utiliza pantalla ILI9341, NeoPixel, RTC DS1307 y pulsador. Las lecturas ausentes aparecen como ausentes, sin cifras inventadas.

**ThingSpeak:** [canal 3567916 (privado)](https://thingspeak.com/channels/3567916), escrito solo desde N5 por HTTPS con CA y Write API Key local. Asignación:

| Campo | Valor | Campo | Valor |
|---|---|---|---|
| 1 | Nivel Fog (0/1/2). | 5 | Viento N3 (grados). |
| 2 | Gas N1 (ADC). | 6 | Estado lavador N4. |
| 3 | Gas N2 (% relativo). | 7 | Confirmación N4. |
| 4 | Gas N3 (ppm simulados). | 8 | Ventilación respaldo. |

**Evidencia observada:** ThingSpeak devolvió `HTTP 200` con `entry_id > 0`. En sesiones sin telemetría reciente se prepararon solamente **fields 1 y 8**; por tanto, **fields 2–7 y operación simultánea de cinco Wokwi siguen pendientes de verificación**. No colocar API keys en la web; el canal sigue privado.

**Contingencia:** A = MQTT WSS + ThingSpeak; B = cambiar acceso de red o usar lectura autorizada de ThingSpeak; C = TFT y CSV histórico **identificado como tal**. No afirmar que esos planes fueron validados sin evidencia. [Guía de integración](docs/integracion.md).

---

## Ciberseguridad transversal

| Nodo | Ataque requerido | Defensa y evidencia esperada |
|---|---|---|
| N1 | Lectura falsificada imposible. | Validar rango/salto y registrar rechazo. |
| N2 | Suplantación de identidad MQTT sin firma. | HMAC por nodo, rechazo explícito. |
| N3 | Publicación falsa de «aire limpio». | HMAC, anti-Replay y verificación contextual. |
| N4 | Comando `OFF` falso o válido repetido. | HMAC, contador y veto de apagado durante riesgo. |
| N5 | Integrar los cuatro ataques. | Registro con **hora, nodo, ataque, acción y motivo**; mostrar lavador encendido frente a comando falso. |

Las capturas individuales están en las carpetas de cada nodo; **falta validar** que los cuatro ataques se activen desde el dashboard y generen eventos verificables en vivo. [Plan de ciberseguridad](pruebas/ciberseguridad/README.md).

## Estado de la entrega y pendientes

| Evidencia | Estado documentado |
|---|---|
| Diagnóstico ≥5 defectos y requisito reescrito | **Documentado** (8 defectos). |
| 12 secciones, arquitectura, usuarios, servicios y enlaces | **Documentado** en este README. |
| Firmwares ESP32 y pruebas individuales | **Disponibles** en carpetas N1–N5. |
| Dashboard de 5 pestañas | **Publicado**; depende de MQTT activo para datos vivos. |
| HTTP ThingSpeak desde Gateway | **Confirmado** con entrada positiva. |
| Enlace del diagrama general Cirkit | **REGISTRADO**: [abrir diseño](https://app.cirkitdesigner.com/project/373d1c97-d707-40e2-8416-f6e8a5aca5a3). |
| Captura exportada del Cirkit general y revisión de los 5 nodos, señales y GPIO | **PENDIENTE** de adjuntar/verificar. |
| Cálculo energético/autonomía y incertidumbre/actuación numérica | **PENDIENTE**. |
| Costo WAN mensual validado ≤S/150 | **PENDIENTE** de cotización. |
| Cinco Wokwi estables, HMAC, lazo cerrado y fields 2–7 | **FUNCIONAMIENTO REPORTADO POR EL EQUIPO; EVIDENCIA PENDIENTE**. Adjuntar registros/capturas actuales y describir resolución de los cortes históricos `state=-3`. |
| Comparación A/B (latencia y falsas alarmas) | **PENDIENTE** de datos propios. |
| Cuatro ataques completos desde la web | **PENDIENTE** de demostración integrada. |
| Cambio de parámetro y planes de contingencia | **PENDIENTE**. |
| Un commit verificable por integrante | **REVISAR** atribución en historial Git. |

**Registro técnico del error principal:** [diagnóstico MQTT/ThingSpeak](docs/diagnostico-mqtt-2026-11-05.md). Nunca presentar una hipótesis como causa confirmada.

## Exposición y contribuciones

La defensa del caso es de **10 minutos sobre el dashboard**, con intervención de los **cinco integrantes**. El documento advierte que sin intervención no hay nota.

- **Apertura — integrador Eduardo (1,5 min):** problema, contexto, reto y arquitectura en pestañas 1/2.
- **Cada uno de los 5 (25 s):** 0–8 s parte del problema; 8–18 s detección y actuación; 18–25 s ataque y defensa, en pestañas 3/4.
- **Cierre — integrador (2,5 min):** escenario en vivo, lavador, confirmación, A/B y conclusión, pestañas 3/5.
- **Preguntas del docente (2 min):** cada integrante debe poder justificar sensores, actuación, seguridad y sus evidencias.

**Regla de entrega de la presentación:** registrar **al menos un commit por integrante** en el repositorio, sin inventar atribuciones. Ver [historial](https://github.com/educmz/iot-g1-pesca-emisiones/commits/main/) y [flujo de Pull Requests](CONTRIBUTING.md).

## Referencias y estructura del repositorio

[Requerimientos](docs/requerimientos/) · [Arquitectura](docs/arquitectura/) · [Contrato MQTT](docs/contrato-mqtt/) · [Reglas](docs/reglas/) · [Cirkit](cirkit/) · [Firmwares Wokwi](wokwi/) · [Dashboard](dashboard/) · [ThingSpeak](thingspeak/) · [Pruebas A/B y seguridad](pruebas/) · [Checklist de rúbrica](docs/entregables.md) · [IA opcional](inteligencia-artificial/).

**Fuentes académicas proporcionadas:** «Laboratorio IoT — G1 Pesca (BMS): emisiones de una planta de harina de pescado» y «IoT Industrial en 12 Pasos». La empresa y cifras pertenecen al caso ficticio; el MQ-2 y el viento simulados no permiten afirmar precisión ambiental certificada. No subir `secrets.h`, `config.h` privados, HMAC, contraseñas ni claves de ThingSpeak.
