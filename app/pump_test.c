#include "pump_test.h"
#include "motor_pulse.h"
volatile PumpTestMailbox g_pump_test = {
    .magic = 0x50554D54U, .abi = 1U, .firmware_id = 0x00080000U,
    .duration_ms = PUMP_DURATION_MS
};
_Static_assert(sizeof(PumpTestMailbox) == 48U, "Update pump reader ABI");
void pump_test_fault(uint32_t code)
{
    motor_pulse_stop(); g_pump_test.fault = code; g_pump_test.state = PUMP_FAULT;
}
int pump_test_init(void)
{
    /* 仅供本次5V独立电机/CN7人工测试。上电不启动，不接App喂食指令。 */
    if (!motor_pulse_init()) { pump_test_fault(1U); return 0; }
    return 1;
}
void pump_test_poll(uint32_t now)
{
    uint32_t command = g_pump_test.command;
    uint32_t nonce = g_pump_test.nonce;
    uint32_t inverse = g_pump_test.nonce_inverse;
    if (command) { g_pump_test.command = 0U; }
    if (command == PUMP_STOP) {
        motor_pulse_stop();
        if (g_pump_test.state != PUMP_FAULT) { g_pump_test.state = PUMP_DONE; }
        return;
    }
    if (g_pump_test.state == PUMP_DONE || g_pump_test.state == PUMP_FAULT) {
        motor_pulse_stop(); return; /* 本次上电锁定，不允许同值重发或第二次动作。 */
    }
    if (g_pump_test.state == PUMP_RUNNING) {
        if (command) { pump_test_fault(2U); }
        else if (!motor_pulse_active()) { /* !表示单脉冲当前未计数，不能据此证明电机机械已停。 */
            motor_pulse_stop(); g_pump_test.completed++; g_pump_test.state = PUMP_DONE;
        } else if ((uint32_t)(now - g_pump_test.started_ms) > PUMP_DURATION_MS + 20U) {
            pump_test_fault(3U); /* 本地额外超时；正常停机由TIM1单脉冲完成。 */
        }
        return;
    }
    motor_pulse_stop();
    if (g_pump_test.state == PUMP_SETTLING && now >= 1000U) { g_pump_test.state = PUMP_READY; }
    if (command == 0U) { return; }
    if (command != PUMP_RUN || nonce == 0U || inverse != (nonce ^ UINT32_MAX)) {
        pump_test_fault(4U); return;
    }
    if (g_pump_test.state != PUMP_READY || g_pump_test.accepted_nonce != 0U) {
        pump_test_fault(5U); return;
    }
    g_pump_test.accepted_nonce = nonce;
    g_pump_test.started_ms = now; g_pump_test.state = PUMP_RUNNING;
    /* 仅A极性（PA8高/PA11低），不自动换向，不把它称为已验证的转向。 */
    if (!motor_pulse_start(MOTOR_POLARITY_A, PUMP_DURATION_MS)) { pump_test_fault(6U); }
}
