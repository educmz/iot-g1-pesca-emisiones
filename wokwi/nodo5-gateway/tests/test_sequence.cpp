#include <cassert>
#include <map>
#include <string>
#include <cstring>
#include "../../nodo1-pozas/wifi-scan/telemetry_sequence.h"
struct Store {
  uint64_t saved=TelemetrySequence::initial;
  bool fail=false;
  unsigned writes=0;
  uint64_t getULong64(const char*,uint64_t) {return saved;}
  size_t putULong64(const char*,uint64_t n) {
    writes++;if(fail)return 0;saved=n;return sizeof(n);
  }
};
int main() {
  Store store;
  TelemetrySequence a;
  assert(a.next(store)==0); // not provisioned
  assert(a.begin(store));
  uint64_t first=a.next(store);
  assert(first>UINT32_MAX); // migration never reuses a legacy counter
  assert(store.saved>=first);
  for(int i=1;i<4096;i++)assert(a.next(store)==first+i);
  assert(store.writes==1);
  assert(a.next(store)==first+4096);
  assert(store.writes==2);
  TelemetrySequence reboot;
  assert(reboot.begin(store));
  uint64_t resumed=reboot.next(store);
  assert(resumed>first+8191); // reserved but unused counters are skipped
  // Crash immediately after reservation cannot cause reuse.
  TelemetrySequence crash;
  assert(crash.begin(store));assert(crash.next(store)>resumed);
  Store broken;broken.fail=true;
  TelemetrySequence b;assert(b.begin(broken));assert(b.next(broken)==0);
  broken.fail=false;assert(b.next(broken)==0); // failure stays closed until reboot
  Store full;full.saved=TelemetrySequence::maximum;
  TelemetrySequence c;assert(c.begin(full));assert(c.next(full)==0);
  Store corrupt;corrupt.saved=1;
  TelemetrySequence d;assert(!d.begin(corrupt));assert(d.next(corrupt)==0);
}
