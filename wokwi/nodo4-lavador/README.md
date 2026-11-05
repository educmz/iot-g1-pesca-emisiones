# Nodo 4 — Lavador de gases

**Responsable:** Pedro  
**Proyecto:** G1 Pesca (BMS): Emisiones de una planta de harina de pescado  
**Nodo:** D4 – Lavador de gases  
**Plataformas:** ESP32, Wokwi, Cirkit Designer, MQTT y dashboard web.

## 1. Descripción

El Nodo 4 se encarga de representar el control del lavador de gases de una planta de harina de pescado.

Permite encender y apagar el lavador mediante un relé, regular la intensidad de funcionamiento a través de un motor paso a paso controlado por un A4988 y verificar su activación mediante un interruptor de confirmación independiente.

El nodo se comunica mediante MQTT con el gateway del sistema y aplica mecanismos de seguridad para evitar órdenes no autorizadas o peligrosas.

## 2. Componentes utilizados

| Componente | Función |
|---|---|
| ESP32 DevKit | Control principal del Nodo 4 |
| Relé SPDT | Representación del encendido y apagado del lavador |
| Transistor BC547 | Accionamiento del relé desde GPIO26 |
| Diodo de protección | Protección frente a la bobina del relé |
| Motor paso a paso bipolar | Representación del control de intensidad |
| Driver A4988 | Control del motor paso a paso |
| Interruptor SPDT | Confirmación independiente del funcionamiento |
| LED verde | Indicador de encendido |
| Resistencias | Protección del LED y polarización del transistor |
| Condensador de 100 µF | Desacoplo de alimentación del A4988 |
| Fuentes de alimentación | Alimentación del circuito de control y potencia |

El motor empleado en el diseño de Cirkit representa un motor bipolar Pololu #1208, de 200 pasos por revolución y 0,5 A por fase.

## 3. Asignación de pines

| GPIO ESP32 | Función |
|---|---|
| GPIO26 | Activación del relé |
| GPIO32 | Lectura de confirmación |
| GPIO18 | STEP del A4988 |
| GPIO19 | DIR del A4988 |
| GPIO23 | ENABLE del A4988 |

## 4. Funcionamiento del nodo

### 4.1. Encendido y apagado

El ESP32 recibe instrucciones de control mediante MQTT.

Cuando se solicita el encendido, activa la salida de control del relé y habilita el A4988 para controlar el motor paso a paso.

Cuando se solicita el apagado, verifica previamente las condiciones de seguridad. Si existe riesgo alto, rechaza la orden de apagado.

### 4.2. Regulación de intensidad

El nodo permite representar distintos niveles de intensidad de funcionamiento mediante cambios en la velocidad del motor.

Los niveles implementados en el firmware de Wokwi son:

- BAJO.
- MEDIO.
- MÁXIMO.

### 4.3. Confirmación de funcionamiento

El interruptor conectado a GPIO32 permite representar si el lavador se encuentra efectivamente activado.

La confirmación es independiente de la señal de control del relé.

Si el lavador recibe una orden de encendido y no obtiene confirmación dentro de 10 segundos, genera el evento `FALLA_LAVADOR`.

### 4.4. Funcionamiento autónomo

Si el Nodo 4 pierde la comunicación con el gateway durante más de 60 segundos, activa el lavador automáticamente como medida de seguridad.

El objetivo es mantener una condición segura ante fallas de comunicación.

## 5. Comunicación MQTT

El Nodo 4 utiliza el protocolo MQTT para intercambiar comandos, estados y alertas.

**Broker de pruebas:** `broker.hivemq.com`

| Topic | Propósito |
|---|---|
| `upc/g1/nodo4/cmd` | Recepción de comandos |
| `upc/g1/nodo4/datos` | Telemetría firmada, una lectura por sensor (dashboard grupal) |
| `upc/g1/nodo4/paquete` | Paquete completo HMAC (panel individual) |
| `upc/g1/nodo4/estado` | Publicación de estado y disponibilidad |
| `upc/g1/nodo4/alerta` | Publicación de alertas de seguridad y funcionamiento |

El firmware envía por `/datos` lecturas individuales de `lavador` (0/1), `confirmacion` (0/1 del interruptor), `intensidad` (1/2/3), `modo_seguro` (0/1) y `riesgo_alto` (0/1). Cada JSON incluye `id=nodo4,ts=millis(),seq,sensor,valor,unidad,calidad,firma` y se firma con HMAC-SHA256 sobre `id|ts|seq|sensor|valor(2 decimales)|unidad|calidad`.

El estado agrupado anterior de `D4` permanece firmado en `/paquete` y lo sigue usando el panel individual. No se debe interpretar la señal `lavador=1` como confirmación física: solamente el interruptor en GPIO32 genera `confirmacion=1`.

El firmware de Wokwi incorpora mecanismos de conexión, publicación y recepción MQTT.

La integración definitiva con el gateway grupal D5 debe comprobarse mediante una prueba conjunta.

## 6. Ciberseguridad

El Nodo 4 incorpora mecanismos de protección frente a comandos falsificados y ataques de repetición.

### 6.1. Validación HMAC

Los comandos protegidos incluyen una firma HMAC que permite comprobar su autenticidad e integridad.

Los comandos cuya validación falla son rechazados.

### 6.2. Protección contra Replay

El nodo controla la secuencia de los comandos recibidos.

Si un atacante vuelve a enviar un comando firmado que ya fue procesado, el nodo lo rechaza mediante la validación del contador de secuencia.

### 6.3. Protección ante riesgo crítico

Aunque una orden de apagado tenga una firma válida, el nodo debe rechazarla cuando la condición de riesgo sea alta.

Esta regla permite evitar que una orden autenticada genere una situación insegura.

### 6.4. Pruebas realizadas en Wokwi

Durante las pruebas individuales se comprobó:

- Recepción de comandos MQTT de encendido y apagado.
- Rechazo de órdenes con HMAC inválido.
- Rechazo de comandos repetidos.
- Rechazo de apagado durante riesgo alto.
- Generación de alertas por falta de confirmación.
- Activación del modo seguro por pérdida de comunicación con el gateway.

Las capturas y registros correspondientes deben conservarse como evidencia.

## 7. Implementación en Wokwi

Wokwi se utiliza para ejecutar el firmware funcional del Nodo 4 y realizar las pruebas de comunicación, actuación y seguridad.

**Enlace al proyecto Wokwi:** [https://wokwi.com/projects/479867720782481409](https://wokwi.com/projects/479867720782481409)

Archivos principales:

- `sketch.ino`: firmware completo del Nodo 4.
- `diagram.json`: conexiones del circuito.
- `libraries.txt`: dependencias utilizadas.
- `secrets.example.h`: ejemplo de configuración sin credenciales reales.

## 8. Implementación en Cirkit Designer

El circuito de D4 también se desarrolló dentro del proyecto grupal en Cirkit Designer.

**Proyecto Cirkit (sección nodo4):** [https://app.cirkitdesigner.com/project/4663bde6-8941-4af3-a259-1a44f4c8d95b](https://app.cirkitdesigner.com/project/4663bde6-8941-4af3-a259-1a44f4c8d95b)

### 8.1. Características del circuito

El diseño incluye el ESP32, el circuito de accionamiento del relé, el interruptor de confirmación, el A4988, el motor bipolar y los elementos de alimentación.

Se configuraron manualmente los siguientes valores:

| Elemento | Valor |
|---|---|
| Resistencia del LED | 330 Ω |
| Resistencia de base del BC547 | 1 kΩ |
| Resistencia base-emisor | 10 kΩ |
| Condensador del A4988 | 100 µF |

### 8.2. Prueba de simulación

Se ejecutó un firmware de diagnóstico en el ESP32 D4 para comprobar la ejecución del programa y el control del motor paso a paso.

Durante las pruebas:

- El ESP32 inició correctamente.
- La consola mostró los mensajes de diagnóstico.
- Se ejecutó una rutina de 50 pasos.
- Se observó el cambio de posición del motor durante la simulación.
- El resultado fue compatible con el movimiento esperado de aproximadamente 90° en modo de paso completo.

### 8.3. Limitaciones

Cirkit Designer identifica algunos componentes como elementos estáticos que no participan en la simulación, entre ellos el relé SPDT, el interruptor deslizante y el condensador del circuito.

Por ello, la simulación de Cirkit demuestra parcialmente el funcionamiento del control del motor, mientras que las pruebas funcionales del relé, confirmación y lógica de seguridad se realizaron en Wokwi.

El circuito no se ha validado para un montaje físico. La corriente de la bobina del relé y el límite de corriente del A4988 deben verificarse antes de construirlo.

## 9. Dashboard y herramientas de prueba

Se desarrollaron herramientas complementarias para enviar comandos MQTT, visualizar información del nodo y ejecutar pruebas de seguridad.

La carpeta `mqtt/` contiene el programa utilizado para enviar comandos de prueba y verificar los mecanismos HMAC y Replay.

La carpeta `dashboard/` contiene el puente de comunicación y los archivos del panel web.

Archivos principales:

- `mqtt/gateway_tester_puertos.py`
- `mqtt/config.example.json`
- `dashboard/bridge.py`
- `dashboard/requirements.txt`
- `dashboard/config.example.json`
- `dashboard/panel/panel-nodo4.html`
- `dashboard/panel/styles.css`
- `dashboard/panel/app.js`

Las configuraciones de ejemplo no deben contener secretos reales.

No publicar archivos de configuración privados, contraseñas ni claves HMAC.

## 10. Evidencias de funcionamiento

Se deben conservar las evidencias reales de:

| Evidencia | Plataforma | Estado |
|---|---|---|
| Encendido y apagado del lavador | Wokwi | Probado |
| Regulación de velocidad del motor | Wokwi | Probado |
| Confirmación mediante interruptor | Wokwi | Probado |
| Falla por ausencia de confirmación | Wokwi | Probado |
| Modo seguro por pérdida de gateway | Wokwi | Probado |
| Recepción y publicación MQTT | Wokwi / herramientas de prueba | Probado individualmente |
| Rechazo de HMAC inválido | Wokwi / MQTT | Probado |
| Rechazo de Replay | Wokwi / MQTT | Probado |
| Rechazo de apagado en riesgo alto | Wokwi / MQTT | Probado |
| Movimiento del motor mediante A4988 | Cirkit Designer | Probado |
| Integración conjunta con D5 | Proyecto grupal | Pendiente |

## 11. Registro de entrega

| Requisito | Estado |
|---|---|
| Firmware ESP32 `sketch.ino` | Implementado |
| Circuito Wokwi `diagram.json` | Implementado |
| Archivo `libraries.txt` | Incluido |
| Enlace Wokwi | Registrado arriba |
| Enlace Cirkit grupal | Registrado |
| Enlace Cirkit individual | Pendiente de agregar |
| Publicación MQTT | Probada individualmente |
| Actuación del lavador | Probada en Wokwi |
| Movimiento del motor | Probado en Wokwi y Cirkit |
| Ataque HMAC inválido | Rechazado en pruebas individuales |
| Ataque Replay | Rechazado en pruebas individuales |
| Dashboard HTML | Implementado y probado individualmente |
| Evidencias en GitHub | 8 archivos (7 capturas y un README) conservados |
| Integración definitiva con D5 | Pendiente |


## Integración corregida (noviembre de 2026)

- **Arranque seguro:** si aún no hay heartbeat HMAC aceptado de D5, enciende el lavador de forma preventiva y selecciona intensidad máxima. La confirmación real no se supone: sigue dependiendo exclusivamente del switch.
- **Pérdida de Gateway:** tras 60 s sin comando/heartbeat auténtico vuelve a modo seguro. No se permite OFF o reducir intensidad bajo riesgo alto ni en modo seguro.
- **Vigencia de comandos:** requiere NTP UTC, una marca `ts` Unix en segundos (máximo 120 s antigua o 45 s futura), `seq` mayor que el aceptado y almacenamiento persistente válido. HMAC canónico del Gateway: `D5|ts|seq|cmd|valor`. Las órdenes remotas se rechazan sin firma, tiempo o contador válidos.
- **Pruebas Serial:** un `HB` por consola **no** autentica D5. Los comandos de actuación por Serial están desactivados por defecto: activar el modo manual en código solo durante pruebas locales controladas.
- **Datos:** `/datos` publica lectura por sensor HMAC (`nodo4`); `/paquete` conserva el reporte completo (`D4`) con HMAC y firma histórica. `/estado` tiene LWT JSON offline, orientativo y no autenticado, y heartbeat online firmado. La alarma `FALLA_LAVADOR` se publica firmada en `/alerta` tras 10 s sin confirmación.
- **Claves:** el antiguo ejemplo `dashboard/config.example.json` contenía valores aparentemente utilizables que ya están expuestos en el historial Git. **Si se copiaron a Wokwi o al panel, hay que rotarlos** y sustituirlos por claves privadas distintas para D5 y D4. Los ejemplos actuales son marcadores; las claves privadas van en `config.local.json` y `secrets.h`, ignorados por Git.
- **Simulación:** los archivos corregidos en GitHub **no actualizan** el proyecto remoto en Wokwi ni el diseño alojado en Cirkit. Pedro tiene que copiarlos, colocar claves nuevas y volver a demostrar ON, OFF condicionado, confirmación, fallo de 10 s, HMAC/replay y pérdida de Gateway.
- **Hardware:** Wokwi simula el A4988 sin usar VMOT; un A4988 físico necesita alimentación del motor separada (normalmente 8–35 V), ajuste de corriente y desacoplo. El circuito no es apto para accionar maquinaria real sin un diseño eléctrico y de seguridad verificado.
- **Integración D5:** Eduardo todavía debe implementar comandos firmados, verificar el estado/confirmación de Pedro y gestionar `FALLA_LAVADOR` para activar la ventilación de respaldo. La fusión de estos archivos no demuestra ese flujo conjunto.

La simulación del lavador es educativa, no un controlador industrial.
