# Nodo 5 — Gateway Fog ResQ

Responsable: Eduardo. Este firmware integra el contrato real de las ramas fusionadas de Fernando, Fabricio, Gonzalo y Pedro.

## Funciones implementadas
- Se mantienen ESP32, pantalla TFT ILI9341, RTC DS1307, NeoPixel y pulsador originales.
- Suscripción MQTT a upc/g1/+/datos, upc/g1/nodo4/alerta y upc/g1/seguridad/eventos.
- Verificación por nodo de HMAC-SHA256 y secuencia estrictamente creciente antes de usar lecturas. Material: id|ts_millis|seq|sensor|valor(2 decimales)|unidad|calidad.
- Verifica identificador, unidad, sensor, rango y calidad. Persiste el máximo seq por nodo en NVS. Los estados sin firma nunca autorizan actuaciones.
- Promedio móvil de tres muestras por sensor. Si un sensor esencial (incluidas las temperaturas de N1 y N2) no actualiza en 12 s, no puede permitir orden OFF. Un dato firmado con calidad inválida descarta las muestras antiguas de ese sensor.
- Dos fuentes independientes elevadas activan lavador: N1 gas ADC >=3820 o temperatura >=45 C; N2 gas relativo >=60% ADC o temperatura >=50 C; N3 gas MQ-2 simulado >=1000 ppm o riesgo nivel 2. NO se convierten estas unidades entre sí.
- Si dos fuentes están elevadas y el viento de Gonzalo apunta al Este (45 a 135 grados), el estado es CRITICO. Una fuente elevada o datos faltantes dan ALERTA. Solo la situación completamente segura permite apagar.
- Comandos D5 con HMAC-SHA256 y contador monotónico persistido: D5|ts_unix_segundos|seq|cmd|valor. Los envía a N4 (HB, RIESGO_ON, MAX, ON, RIESGO_OFF, OFF), N3 (HB con valor 1), y N1 (VENT_MAX/VENT_AUTO).
- Detecta confirmación real de N4 (sensor confirmado desde el switch) y EVENTO FALLA_LAVADOR firmado por N4. Si no se confirma en 10 segundos después de ON, o si D4 enciende preventivamente sin confirmación durante 10 s, firma VENT_MAX a N1.
- VENT_MAX queda enclavado incluso tras reinicio del Gateway. Para levantarlo, presionar el botón durante 2,5 segundos y demostrar riesgo normal con fuentes frescas más confirmación física de N4. Pulsación breve solo reconoce.
- ThingSpeak opcional por HTTPS CA validada y Write API Key privada, a un intervalo de 20 s. No sustituye datos ausentes con ceros. Sin CA y clave no se conecta, y MQTT sigue funcionando.

## Archivos
- sketch.ino: Gateway Fog con MQTT, HMAC, TFT y actuadores.
- fog_logic.h: reglas Fog puras en C++.
- tests/test_fog.cpp: siete escenarios de decisión y viento.
- secrets.example.h: plantilla sin claves reales.
- diagram.json: conserva exactamente el cableado anterior.
- libraries.txt: añade ArduinoJson 6.21.5.
- .github/workflows/nodo5-fog.yml: compila el ESP32 y ejecuta los tests C++.

## Lo que Eduardo debe configurar fuera de GitHub
1. Abrir https://wokwi.com/projects/479700208383720449 y detener la simulación.
2. Copiar el nuevo sketch.ino y libraries.txt a Wokwi. Crear una pestaña fog_logic.h con el archivo del repositorio.
3. Crear la pestaña secrets.h a partir de secrets.example.h. Generar allí cinco claves privadas, largas y distintas: HMAC_KEY_NODO1, HMAC_KEY_NODO2, HMAC_KEY_NODO3, HMAC_KEY_NODO4, HMAC_KEY_GATEWAY.
4. Las claves de telemetría deben coincidir con la del nodo correspondiente: N1 secrets.h, N2 config.h (HMAC_KEY), N3 secrets.h, N4 secrets.h (CLAVE_NODO4).
5. La clave privada de D5 debe coincidir en N1 (HMAC_KEY_GATEWAY), N3 (HMAC_KEY_GATEWAY) y N4 (CLAVE_GATEWAY). NUNCA mostrar claves en captura, commit ni chat.
6. Las cuatro simulaciones de los otros nodos también deben tener copiadas sus versiones finales de main; GitHub no actualiza automáticamente Wokwi/Cirkit.
7. Para ThingSpeak, crear un canal en su cuenta, obtener Write API Key y configurar el CA PEM verificado de api.thingspeak.com; mantener ambos dentro de secrets.h. El Gateway funciona sin ThingSpeak si no se configura.

## Ensayos de la demostración
| Caso | Estímulo | Comportamiento esperado |
|---|---|---|
| Arranque | D5 solo, sin sensores | ALERTA por datos incompletos, no manda OFF |
| Normal | N1, N2 y N3 bajos; viento y riesgo frescos | NORMAL y, si está permitido, RIESGO_OFF + OFF firmados |
| Una fuente | Solo D2 gas >60% | ALERTA informativa, todavía no hay corroboración |
| Dos fuentes | N1 ADC >=3820 + D2 gas >=60% | RIESGO_ON, MAX y ON a D4 |
| Comunidad | Dos fuentes elevadas y viento 90 grados | CRITICO en TFT, NeoPixel rojo |
| Confirmación | Switch del lavador activo después de ON | Confirmación verificada, sin respaldo |
| Falla | Switch de D4 no confirma en 10 s, o llega FALLA_LAVADOR firmado | D5 manda VENT_MAX a D1 |
| Defensa HMAC | Alterar una medida o repetir mensaje firmado | Rechazo y aumento de contador |
| Desconexión | Apagar un nodo >12 s | ALERTA, nunca ordenar OFF por datos faltantes |
| Liberación | Ambiente normal, D4 confirmado y botón pulsado 2,5 s | VENT_AUTO firmado a D1 |

## Alcance y límites conocidos
- No confundir ADC de N1, % de ADC de N2 y ppm SIMULADOS del MQ-2 de N3 con mediciones certificadas de NH3 o H2S.
- Los eventos publicados por otros nodos en seguridad/eventos no tienen un formato HMAC uniforme; el Gateway los registra como avisos NO confiables, sin actuar. Los eventos D4 de /alerta requieren firma y tiempo Unix reciente.
- Las secuencias de telemetría en los nodos de origen no persisten tras reinicio, mientras el Gateway sí guarda máximos NVS. Por seguridad, un nodo reiniciado podría quedar rechazado hasta que se reconfigure la época/contador de forma controlada. La solución robusta futura exige contador persistente o boot epoch firmado por cada emisor; no hacer auto-reset remoto de contadores.
- El broker público TCP 1883 no es confidencial ni garantiza disponibilidad. El prototipo es educativo y no apto para control industrial.
- La NVS se escribe por cada muestra firmada: sirve como defensa didáctica, no es estrategia adecuada de desgaste de flash en una instalación real.
- Las pruebas de GitHub no ejecutan Wokwi, la interacción física de cinco simuladores ni un canal privado ThingSpeak. Hay que repetir las pruebas con claves locales y adjuntar evidencias.
- La CA PEM de ThingSpeak puede cambiar; verificarla y actualizarla antes de usar TLS.

**Enlace Wokwi original:** https://wokwi.com/projects/479700208383720449
