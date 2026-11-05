#include <cassert>
#include <iostream>
#include "fog_logic.h"
int main() {
  const FogInputs ok={true,true,true,true,true,800,10,400,180,0};
  FogDecision v=evaluateFog(ok);
  assert(v.level==0 && v.safeToStop && !v.requireLavador);
  FogInputs one=ok; one.n2Percent=80;
  v=evaluateFog(one); assert(v.level==1 && v.elevatedSources==1 && !v.requireLavador);
  FogInputs two=one; two.n1Adc=3900;
  v=evaluateFog(two); assert(v.level==1 && v.elevatedSources==2 && v.requireLavador);
  two.windDegrees=90;
  v=evaluateFog(two); assert(v.level==2 && v.windToCommunity && v.requireLavador);
  FogInputs missing=ok; missing.n3Valid=false;
  v=evaluateFog(missing); assert(v.level==1 && !v.safeToStop);
  FogInputs risk=ok; risk.n3SimPpm=1200; risk.perimeterRisk=2; risk.n1Adc=4000; risk.windDegrees=90;
  v=evaluateFog(risk); assert(v.level==2 && v.requireLavador);
  FogInputs boundary=ok; boundary.windDegrees=135; boundary.n2Percent=85;boundary.n3SimPpm=1200;
  v=evaluateFog(boundary);assert(v.windToCommunity && v.level==2);
  std::cout<<"OK: siete escenarios Fog de laboratorio\n";
}
