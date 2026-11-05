# Verificación de correcciones del Nodo 1 — versión para probar

1. Abre [el Wokwi de Fernando](https://wokwi.com/projects/479819491217347585) y para la simulación.
2. Copia `wifi-scan.ino` de esta carpeta en la pestaña `sketch.ino` de Wokwi. Copia también el contenido de `diagram.json` y `libraries.txt`.
3. Crea una pestaña nueva `secrets.h` a partir de `secrets.example.h`. Configura claves HMAC de laboratorio privadas, **distintas** para Nodo 1 y para órdenes del Gateway D5. Para que D5 firme órdenes a Nodo 1, debe utilizar la misma `HMAC_KEY_GATEWAY`. No subir ese archivo.
4. Ejecuta Play y comprueba mensajes `NODO1`, `[EDGE]` y, cuando conecte, `[MQTT]`.

| Prueba | Estímulo | Resultado esperado |
|---|---|---|
| Ambiente normal | MQ-2 a ~400 ppm simulados y DS18B20 a 22 °C | Relé y buzzer apagados después de lecturas válidas; ver ADC real en monitor |
| Alerta | Aumenta MQ-2 hasta que el ADC llegue al umbral 3820, o DS18B20 a 50 °C | Relé activado y buzzer sonando |
| Edge sin red | Tras arrancar y verificar un estado estable, deshabilita conectividad | Las decisiones locales siguen funcionando |
| Firma inválida | Envía en `upc/g1/nodo1/cmd` JSON con `id:"D5",cmd:"VENT_MAX",valor:"1",ts` actual, `seq` entero y `firma` con 64 ceros | Orden rechazada; registro de HMAC inválido |
| Replay | Envía dos veces la misma orden firmada con el mismo `seq` | Segunda orden rechazada |
| Ventilación forzada | Manda `VENT_MAX` válido mientras no hay alarma local | Relé encendido pese a lecturas normales |
| Desbloqueo | Manda `VENT_AUTO` válido con `valor:"0"` sin riesgo local | Control regresa a Edge |
| No apagar inseguro | Manda `VENT_AUTO` válido durante alarma | El relé sigue encendido |

**Firma de orden:** HMAC-SHA256 (hex minúscula) del texto UTF-8 `D5|ts|seq|cmd|valor`. El campo `ts` debe estar en segundos Unix y la hora NTP debe ser válida. El contador `seq` se conserva en NVS; para pruebas usa uno estrictamente creciente.

**Importante:** esta revisión solo modifica archivos del repositorio; todavía no hemos ejecutado o compilado este firmware en Wokwi ni actualizado el proyecto de Cirkit. No reutilizar las capturas de la versión anterior como evidencia de las funciones nuevas.
