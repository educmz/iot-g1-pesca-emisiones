# Ataques y defensas (documento del laboratorio)

| Nodo | Ataque solicitado | Defensa solicitada |
|---|---|---|
| 1 | Lectura imposible | validación de rango y salto máximo |
| 2 | Suplantación sin firma | verificación HMAC por nodo |
| 3 | Mensaje falso de «aire limpio» | firma + coherencia con pozas, secadores y viento |
| 4 | Orden falsa de apagado o replay | firma + contador + no apagar en concentración alta |
| 5 | Integración de todos los ataques | eventos registrados en pestaña «Ciberseguridad» |

Cada ataque debe dispararse desde HTML, *llegar realmente al sistema* y ser rechazado. Guardar capturas, logs o videos de:
- hora, nodo, ataque, acción tomada y motivo;
- rechazo real de la entrada/comando;
- lavador **encendido** después de orden falsa de apagado.

El botón de HTML solo envía una solicitud de simulación; la prueba se valida **únicamente al recibir el evento de rechazo desde el firmware**. El esquema de solicitudes `upc/g1/sim/ataque` es una extensión propuesta para implementar entre todos.
