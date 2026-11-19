#ifndef FEEDER_PUMP_TEST_H
#define FEEDER_PUMP_TEST_H
#include <stdint.h>
#define PUMP_RUN 0x50554D50U
#define PUMP_STOP 0x53544F50U
#define PUMP_DURATION_MS 200U
enum { PUMP_SETTLING, PUMP_READY, PUMP_RUNNING, PUMP_DONE, PUMP_FAULT };
typedef struct {
    uint32_t magic, abi, firmware_id, command, nonce, nonce_inverse, accepted_nonce;
    uint32_t state, fault, started_ms, completed, duration_ms;
} PumpTestMailbox;
extern volatile PumpTestMailbox g_pump_test;
int pump_test_init(void);
void pump_test_poll(uint32_t now);
void pump_test_fault(uint32_t code);
#endif
