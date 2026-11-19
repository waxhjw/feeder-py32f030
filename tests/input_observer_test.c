#include "input_observer.h"
#include <assert.h>
#include <stdio.h>
int main(void)
{
    InputObserver s={0};
    input_observer_step(&s,0U,7U);input_observer_step(&s,19U,7U);
    assert(!s.channel[0].valid);
    input_observer_step(&s,20U,7U);
    assert(s.channel[0].valid && s.channel[0].rises==0U && s.channel[0].falls==0U);
    puts("PASS startup establishes level without fake edge");
    input_observer_step(&s,21U,6U);input_observer_step(&s,25U,7U);input_observer_step(&s,45U,7U);
    assert(s.channel[0].falls==0U);puts("PASS bounce does not become stable edge");
    input_observer_step(&s,46U,0U);input_observer_step(&s,66U,0U);
    for(unsigned i=0;i<3;i++) { assert(s.channel[i].falls==1U && s.channel[i].stable==0U); }
    input_observer_step(&s,67U,7U);input_observer_step(&s,87U,7U);
    for(unsigned i=0;i<3;i++) { assert(s.channel[i].rises==1U); }
    puts("PASS three channels count only qualified transitions");
    input_observer_step(&s,200U,0U);assert(s.gaps==1U && !s.channel[0].valid);
    input_observer_step(&s,220U,0U);assert(s.channel[0].valid && s.channel[0].falls==1U);
    puts("PASS gap invalidates and reacquires without inventing transition");
    InputObserver w={0};input_observer_step(&w,0xFFFFFFF0U,7U);input_observer_step(&w,4U,7U);
    assert(w.channel[0].valid && w.max_gap==20U);
    unsigned count=w.samples;input_observer_step(&w,4U,7U);assert(count==w.samples);
    puts("PASS millisecond wrap and duplicate sample");
    return 0;
}
