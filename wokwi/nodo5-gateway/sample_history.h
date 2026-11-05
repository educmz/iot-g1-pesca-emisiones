#pragma once
#include <stdint.h>
template<int Capacity> struct SampleHistory {
  float samples[Capacity]={};
  uint8_t count=0,cursor=0;
  uint32_t at=0;
  bool fresh(uint32_t now,uint32_t maxAge) const {
    return count>0 && uint32_t(now-at)<=maxAge;
  }
  void invalidate() {count=0;cursor=0;at=0;}
  void add(float value,uint32_t now,uint32_t maxAge) {
    if(count && !fresh(now,maxAge))invalidate();
    samples[cursor]=value;cursor=(cursor+1)%Capacity;
    if(count<Capacity)count++;
    at=now;
  }
  float latest() const {return count?samples[(cursor+Capacity-1)%Capacity]:0;}
  float avg() const {
    if(!count)return 0;
    float sum=0;for(int i=0;i<count;i++)sum+=samples[i];
    return sum/count;
  }
};
