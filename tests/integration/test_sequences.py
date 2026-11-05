"""Exercise the exact allocator embedded in all four firmwares against NVS failures."""
from pathlib import Path
import subprocess
import tempfile

root=Path(__file__).resolve().parents[2]
paths=[root/'wokwi/nodo1-pozas/wifi-scan/wifi-scan.ino',*[root/f'wokwi/{n}/sketch.ino' for n in ('nodo2-secadores','nodo3-perimetro','nodo4-lavador')]]
blocks=[p.read_text().split('// BEGIN PERSISTENT TELEMETRY SEQUENCE')[1].split('// END PERSISTENT TELEMETRY SEQUENCE')[0] for p in paths]
assert all(b==blocks[0] for b in blocks), 'Allocator diverged across nodes'
stub='''#pragma once
#include <cstdint>
#include <cstddef>
class Preferences {
public:
  static inline uint32_t disk=0;
  static inline unsigned writes=0;
  static inline bool openOK=true, writeOK=true;
  bool begin(const char*,bool){return openOK;}
  uint32_t getUInt(const char*,uint32_t){return disk;}
  size_t putUInt(const char*,uint32_t n){++writes;if(!writeOK)return 0;disk=n;return 4;}
};
'''
tests='''
#include <cassert>
int main(){
  TelemetrySequence first;
  assert(first.next()==1 && Preferences::disk==256 && Preferences::writes==1);
  for(uint32_t i=2;i<=256;i++)assert(first.next()==i);
  assert(Preferences::writes==1);
  assert(first.next()==257 && Preferences::disk==512);
  TelemetrySequence afterPowerLoss;
  assert(afterPowerLoss.next()==513); // never reuse unused reserved values
  Preferences::writeOK=false;
  TelemetrySequence failedWrite;
  assert(failedWrite.next()==0);
  Preferences::writeOK=true;
  assert(failedWrite.next()==0); // failed instance remains closed
  Preferences::openOK=false;
  TelemetrySequence failedOpen;
  assert(failedOpen.next()==0);
  Preferences::openOK=true;
  Preferences::disk=UINT32_MAX-100;
  TelemetrySequence exhausted;
  assert(exhausted.next()==0); // never wrap
}
'''
with tempfile.TemporaryDirectory() as d:
 p=Path(d);(p/'Preferences.h').write_text(stub);(p/'test.cpp').write_text(blocks[0]+tests)
 subprocess.run(['g++','-std=c++17','-Wall','-Wextra','-Werror','-I',d,str(p/'test.cpp'),'-o',str(p/'test')],check=True)
 subprocess.run([str(p/'test')],check=True)
print('PASS: block boundary, reboot, reservation write failure, NVS open failure, overflow; all four allocators identical')
