# Cómo colaborar sin sobrescribir archivos

## Flujo obligatorio para el laboratorio

1. Solicitar acceso como colaborador a Eduardo (propietario del repositorio).
2. Clonar: `git clone https://github.com/educmz/iot-g1-pesca-emisiones.git`
3. Partir de la rama principal actualizada:
   ```bash
   git switch main
   git pull origin main
   git switch -c nodo1-fernando
   ```
   Cambiar el nombre de la rama según integrante.
4. Trabajar en **su carpeta** y, si es necesario, en su propio panel del dashboard.
5. Revisar `git status`. **Nunca subir contraseñas ni claves**.
6. Guardar cambios:
   ```bash
   git add .
   git commit -m "feat(nodo1): lecturas, actuación y pruebas"
   git push -u origin nodo1-fernando
   ```
7. En GitHub, abrir **Compare & pull request** con destino `main`. Adjuntar evidencia y enlace de Wokwi.
8. Eduardo valida la integración y fusiona el PR. No subir directamente a `main` durante el desarrollo individual.

## Nombres de ramas

- `nodo1-fernando`, `nodo2-fabricio`, `nodo3-gonzalo`, `nodo4-pedro`, `nodo5-eduardo`
- Para cambios grupales: `docs/contrato-v1`, `feature/dashboard`, `test/alternativas`, etc.

## Checklist de cada PR

- [ ] Incluí código fuente, `diagram.json`, `libraries.txt` cuando corresponda y enlace Wokwi.
- [ ] El nodo detecta y **actúa**; adjunté evidencias (capturas/logs reales).
- [ ] Usa los tópicos y campos acordados en `docs/contrato-mqtt/README.md`.
- [ ] Incluí ataque, defensa y evidencia de rechazo.
- [ ] No incluí credenciales reales.
- [ ] Documenté pasos para que el integrador repita la prueba.

**Conflictos:** no usar `git push --force` en `main`. Si hay conflicto, actualizar rama con `main` y resolver antes del PR.
