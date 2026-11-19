#define main inherited_app_test_main
#include "app_motor_test.c"
#undef main
int main(void)
{
    fresh(1);
    const unsigned ids[]={2U,101U,102U,103U};
    for(unsigned i=0;i<4;i++) {
        dp(ids[i],1U);assert(!motor.motor_on && !motor.pending && !app.quick_active);drain();
        dp(ids[i],0U);assert(!motor.motor_on && !motor.pending);drain();
    }
    puts("PASS diagnostic rejects actual UART start/quick/reverse, reports stopped");
    frame(0U,0,0,0);drain();assert(link.replies);
    puts("PASS heartbeat still responds with action gate active");
    return 0;
}
