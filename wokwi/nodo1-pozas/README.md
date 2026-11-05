# Nodo 1 — Pozas de materia prima

**Responsable:** Fernando  
**Rama de corrección:** `fix/nodo1-buzzer-adc-hmac`

## Versión de Wokwi corregida
Los archivos se encuentran en [wifi-scan/](wifi-scan/), conservando la ubicación original. La simulación compartida por el equipo **no se actualiza automáticamente** desde GitHub.

- [Firmware](wifi-scan/wifi-scan.ino): salida local con buzzer, lectura por ADC1, telemetría por sensor y órdenes firmadas HMAC/anti-replay.
- [Circuito](wifi-scan/diagram.json): MQ-2 simulado en GPIO34, DS18B20 en GPIO15 con pull-up de 4,7 kΩ, buzzer en GPIO16 y relé en GPIO17.
- [Librerías](wifi-scan/libraries.txt).
- [Plantilla de secretos](wifi-scan/secrets.example.h): copiar como `secrets.h` en Wokwi y sustituir las claves privadas, **sin subirlas a GitHub**.
- [Guía de pruebas](wifi-scan/PRUEBAS.md).

## Funcionamiento
- **Edge:** alerta de gas en `ADC >= 3820` o temperatura `>= 45 °C`, con activación de relé y buzzer. Son umbrales de demostración, no límites de H2S o amoníaco. Un sensor inválido mantiene un estado de seguridad conservador.
- **Sin Wi-Fi:** el firmware intenta reconectar sin un bucle `while` de reconexión; la alarma local se sigue evaluando. Algunas funciones MQTT todavía pueden bloquear brevemente durante intentos de conexión.
- **MQTT:** publica sensores separados en `upc/g1/nodo1/datos` con `id,ts,seq,sensor,valor,unidad,calidad,firma`; el `ts` es `millis()`. Firma HMAC-SHA256 hex de `id|ts|seq|sensor|valor(2 decimales)|unidad|calidad`.
- **Ventilación forzada:** espera en `upc/g1/nodo1/cmd` mensajes `{"id":"D5","ts":<Unix s>,"seq":<entero>,"cmd":"VENT_MAX","valor":"1","firma":"<HMAC>"}`; para liberar la orden usa `VENT_AUTO` y `valor:"0"`. Firma comandos en `D5|ts|seq|cmd|valor`, como la convención interna de comandos de Nodo 4. No ejecuta órdenes sin HMAC válido, tiempo aceptable y secuencia superior a la última persistida.
- Las salidas y el contador se conservan en modo preventivo frente a reinicios/fallas; sin claves válidas, las órdenes se rechazan.

## Evidencias anteriores
[11 capturas de Fernando](img/) y [registro](Evidencia.md) **pertenecen a la versión anterior**; no demuestran que esta revisión compile, suene el buzzer ni valide HMAC. Hay que repetir las pruebas antes del merge.

## Enlaces existentes
- **Wokwi:** https://wokwi.com/projects/479864540006804481
- **Cirkit:** https://app.cirkitdesigner.com/project/75988b2c-c401-4ed8-ae5e-e7d56159e44f

**Cirkit Designer no se modifica al editar GitHub.** Fernando debe cambiar el diseño de Cirkit manualmente para representar buzzer/MQ-2. Un MQ-2 físico común usa 5 V y requiere adaptar la salida AOUT al máximo admitido por el ADC ESP32 (3,3 V). El diseño Wokwi representa un prototipo simulado, no valida el cableado físico de alimentación.

## Pendiente
Compilar en Wokwi, ejecutar pruebas de riesgo y reconexión, verificar la integración firmada con D5 y capturar nuevas evidencias. 
