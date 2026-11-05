"""Pruebas offline de formatos HMAC D4 y del panel (no MQTT externo)."""
import hashlib
import hmac
import sys
import unittest
from pathlib import Path

ROOT=Path(__file__).resolve().parents[1]
sys.path.insert(0,str(ROOT))
from bridge import STATIC,sign,sign_command,canonical_command,verify_node,verify_group_node

GATEWAY_KEY="CLAVE_TEST_EXCLUSIVA_GATEWAY_2026_D4"
NODE_KEY="CLAVE_TEST_EXCLUSIVA_NODO4_2026_D4"
CONFIG={
"topic_datos":"upc/g1/nodo4/datos",
"topic_paquete":"upc/g1/nodo4/paquete",
"topic_alerta":"upc/g1/nodo4/alerta",
"topic_estado":"upc/g1/nodo4/estado",
"clave_gateway":GATEWAY_KEY,"clave_nodo4":NODE_KEY
}
def group(sensor="lavador",valor=1,unit="estado",seq=42):
    m={"id":"nodo4","ts":12345,"seq":seq,"sensor":sensor,
       "valor":valor,"unidad":unit,"calidad":"ok"}
    canon=f'nodo4|12345|{seq}|{sensor}|{valor:.2f}|{unit}|ok'
    m["firma"]=sign(canon,NODE_KEY)
    return m
def package():
    m={"id":"D4","ts":1793830000,"valor":"ON","unidad":"estado_lavador",
       "calidad":"VALIDO","intensidad":"MAXIMA","confirmado":"NO",
       "seguro":"SI","riesgo":"ALTO","seq":23}
    canon='D4|1793830000|ON|estado_lavador|VALIDO|MAXIMA|NO|SI|ALTO|23'
    m["firma"]=sign(canon,NODE_KEY)
    return m

class D4Tests(unittest.TestCase):
    def test_command_signature(self):
        m=sign_command("ON",GATEWAY_KEY,100,ts=1793830000,valor="")
        canon="D5|1793830000|100|ON|"
        self.assertEqual(canonical_command(m),canon)
        self.assertEqual(m["firma"],hmac.new(GATEWAY_KEY.encode(),canon.encode(),hashlib.sha256).hexdigest())

    def test_group_valid(self):
        self.assertTrue(verify_group_node(group(),CONFIG["topic_datos"],CONFIG))

    def test_group_modified_is_rejected(self):
        m=group()
        m["valor"]=0
        self.assertFalse(verify_group_node(m,CONFIG["topic_datos"],CONFIG))

    def test_group_units_and_ranges(self):
        self.assertFalse(verify_group_node(group("intensidad",4,"nivel"),CONFIG["topic_datos"],CONFIG))
        self.assertFalse(verify_group_node(group("confirmacion",1,"ppm"),CONFIG["topic_datos"],CONFIG))
        self.assertFalse(verify_group_node(group(),CONFIG["topic_paquete"],CONFIG))

    def test_group_all_sensors(self):
        for name,value,unit in [
            ("lavador",0,"estado"),("confirmacion",1,"estado"),
            ("intensidad",3,"nivel"),("modo_seguro",1,"estado"),
            ("riesgo_alto",0,"estado")
        ]:
            with self.subTest(name=name):
                self.assertTrue(verify_group_node(group(name,value,unit),CONFIG["topic_datos"],CONFIG))

    def test_legacy_package_for_panel(self):
        m=package()
        self.assertTrue(verify_node(m,CONFIG["topic_paquete"],CONFIG))
        m["confirmado"]="SI"
        self.assertFalse(verify_node(m,CONFIG["topic_paquete"],CONFIG))
        self.assertFalse(verify_node(m,CONFIG["topic_datos"],CONFIG))

    def test_signed_failure_event(self):
        m={"id":"D4","ts":1793830000,"valor":"FALLA_LAVADOR","unidad":"evento",
           "calidad":"VALIDO","tipo":"FALLA_LAVADOR","motivo":"SIN_CONFIRMACION_10S","seq":0}
        canon="D4|1793830000|FALLA_LAVADOR|evento|VALIDO|FALLA_LAVADOR|SIN_CONFIRMACION_10S|0"
        m["firma"]=sign(canon,NODE_KEY)
        self.assertTrue(verify_node(m,CONFIG["topic_alerta"],CONFIG))

    def test_panel_html_exists(self):
        self.assertTrue((ROOT / STATIC["/"][0]).is_file())
        self.assertTrue((ROOT / STATIC["/index.html"][0]).is_file())


if __name__ == "__main__":
    unittest.main()
