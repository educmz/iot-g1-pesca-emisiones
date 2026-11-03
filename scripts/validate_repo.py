"""Chequeo básico de estructura documental. No valida el hardware ni el firmware."""
from pathlib import Path
import csv
import sys

ROOT = Path(__file__).resolve().parents[1]
REQUIRED = [
    "README.md", "CONTRIBUTING.md", ".gitignore",
    "docs/requerimientos/diagnostico.md",
    "docs/requerimientos/requerimiento.md",
    "docs/contrato-mqtt/README.md",
    "docs/arquitectura/README.md",
    "docs/reglas/README.md",
    "cirkit/diagrama-general/README.md",
    "dashboard/index.html", "dashboard/js/app.js",
    "dashboard/js/config.example.js", "pruebas/plantilla-resultados.csv",
] + [f"wokwi/nodo{n}-{slug}/README.md" for n, slug in [
    (1,"pozas"),(2,"secadores"),(3,"perimetro"),
    (4,"lavador"),(5,"gateway")]]

missing = [path for path in REQUIRED if not (ROOT / path).is_file()]
if missing:
    print("Faltan archivos:", *missing, sep="\n - ")
    sys.exit(1)

with (ROOT / "pruebas/plantilla-resultados.csv").open(encoding="utf-8", newline="") as stream:
    reader = csv.reader(stream)
    header = next(reader)
    assert "alternativa" in header and "latencia_s" in header
    assert not list(reader), "La plantilla CSV no debe contener datos inventados"

for forbidden in ("dashboard/js/config.js", ".env", "secrets.h"):
    if (ROOT / forbidden).exists():
        print("Advertencia: archivo local sensible presente; compruebe .gitignore:", forbidden)

print("OK: estructura inicial y plantilla CSV verificadas.")
print("Nota: este chequeo NO prueba nodos, MQTT, ciberseguridad ni integración.")
