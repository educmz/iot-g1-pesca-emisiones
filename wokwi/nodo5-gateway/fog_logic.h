#pragma once
// Pure C++: decision rules in a host-testable file; values are SIMULATED.
// Each source is independent. Never convert ADC/% to a purported H2S/NH3 ppm.
struct FogInputs {
  bool n1Valid, n2Valid, n3Valid, windValid, perimeterRiskValid;
  float n1Adc, n2Percent, n3SimPpm, windDegrees;
  int perimeterRisk;
};
struct FogDecision {
  // 0 NORMAL; 1 ALERTA (including missing data); 2 CRITICO
  int level;
  int elevatedSources;
  bool windToCommunity;
  bool requireLavador;
  bool safeToStop;
};
inline FogDecision evaluateFog(const FogInputs& in) {
  bool wind = in.windValid && in.windDegrees >= 45.0f && in.windDegrees <= 135.0f;
  bool a = in.n1Valid && in.n1Adc >= 3820.0f;
  bool b = in.n2Valid && in.n2Percent >= 60.0f;
  bool c = in.n3Valid && (in.n3SimPpm >= 1000.0f ||
                      (in.perimeterRiskValid && in.perimeterRisk >= 2));
  int count = (a ? 1 : 0) + (b ? 1 : 0) + (c ? 1 : 0);
  bool complete = in.n1Valid && in.n2Valid && in.n3Valid &&
                  in.windValid && in.perimeterRiskValid;
  bool safe = complete && count == 0 && in.perimeterRisk == 0;
  int level = count >= 2 && wind ? 2 : ((count > 0 || !safe) ? 1 : 0);
  return {level, count, wind, count >= 2, safe};
}
