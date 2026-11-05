#pragma once
#include <stdint.h>
#include <stddef.h>
// Reserve BEFORE publishing. After reboot, skip the entire previous block.
// Initial floor exceeds every legacy uint32 telemetry counter. Never reset RX NVS.
// NVS loss needs explicit re-provisioning; clones must not share a node identity/key.
class TelemetrySequence {
  uint64_t current_=0, limit_=0;
  bool ready_=false;
public:
  static constexpr uint64_t initial=UINT64_C(4294967296);
  static constexpr uint64_t maximum=UINT64_C(9007199254740991); // JSON exact in JS
  template<class Store> bool begin(Store& store) {
    current_=store.getULong64("txLimit",initial);
    limit_=current_;
    ready_=current_>=initial && current_<=maximum;
    return ready_;
  }
  template<class Store> uint64_t next(Store& store) {
    if(!ready_)return 0;
    if(current_==limit_) {
      if(limit_>maximum-4096) {ready_=false;return 0;}
      uint64_t reserved=limit_+4096;
      if(store.putULong64("txLimit",reserved)!=sizeof(uint64_t)) {
        ready_=false;return 0; // fail closed, no counter reuse
      }
      limit_=reserved;
    }
    return ++current_;
  }
};
