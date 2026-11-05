#include <cassert>
#include "../sample_history.h"
int main() {
  SampleHistory<3> h;
  assert(!h.fresh(0,12000));
  h.add(1,10,12000);h.add(1,20,12000);h.add(0,30,12000);
  assert(h.latest()==0); // physical confirmation lost immediately, no averaging
  assert(h.avg()>0.5f);
  assert(h.fresh(12030,12000));assert(!h.fresh(12031,12000));
  h.add(9,12031,12000);assert(h.count==1 && h.avg()==9);
  h.add(6,12032,12000);assert(h.avg()==7.5f);
  h.invalidate();h.add(4,12033,12000);assert(h.avg()==4);
  h.invalidate();h.add(2,UINT32_MAX-5,12000);
  assert(h.fresh(5,12000));assert(!h.fresh(12000,12000));
}
