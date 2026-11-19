#include "py32f0xx.h"
#include "wbr_pair.h"
volatile WbrPairCommand g_wbr_pair_command = {.magic = WBR_PAIR_MAGIC, .abi = 1U};
_Static_assert(sizeof(WbrPairCommand) == 48U, "Update pairing reader ABI");
static uint32_t reports_before_request;
void wbr_pair_poll(WbrInit *link, uint32_t now)
{
    if (g_wbr_pair_command.command) {
        __DMB();
        uint32_t command = g_wbr_pair_command.command, nonce = g_wbr_pair_command.nonce;
        uint32_t inverse = g_wbr_pair_command.nonce_inverse;
        g_wbr_pair_command.command = 0U; __DMB();
        if (command != WBR_PAIR_REQUEST || !nonce || inverse != ~nonce || nonce == g_wbr_pair_command.accepted_nonce) {
            g_wbr_pair_command.result = WBR_PAIR_INVALID;
        } else {
            g_wbr_pair_command.accepted_nonce = nonce;
            if (link->pair_attempts) { g_wbr_pair_command.result = WBR_PAIR_ALREADY_REQUESTED; }
            else if (link->locked || !wbr_init_serial_ready(link) || link->count || link->sending) {
                g_wbr_pair_command.result = WBR_PAIR_NOT_READY;
            } else if (link->network_state == 0U || link->network_state == 1U || link->network_state == 6U) {
                g_wbr_pair_command.result = WBR_PAIR_ALREADY_PAIRING;
            } else if (wbr_init_request_pair(link)) {
                reports_before_request = link->network_reports;
                g_wbr_pair_command.request_ms = now;
                g_wbr_pair_command.result = WBR_PAIR_QUEUED;
            } else { g_wbr_pair_command.result = WBR_PAIR_NOT_READY; }
        }
    }
    uint32_t result = g_wbr_pair_command.result;
    if (result >= WBR_PAIR_QUEUED && result <= WBR_PAIR_ACKED) {
        if (link->network_reports > reports_before_request &&
            (link->network_state == 0U || link->network_state == 1U || link->network_state == 6U)) {
            g_wbr_pair_command.result = WBR_PAIR_READY;
        } else if ((uint32_t)(now - g_wbr_pair_command.request_ms) >= 20000U) {
            g_wbr_pair_command.result = WBR_PAIR_TIMEOUT; /* 不重发、不重置主控或模组。 */
        } else if (link->pair_acks) { g_wbr_pair_command.result = WBR_PAIR_ACKED; }
        else if (link->pair_transmitted) { g_wbr_pair_command.result = WBR_PAIR_SENT; }
    }
    g_wbr_pair_command.attempts = link->pair_attempts;
    g_wbr_pair_command.tx_count = link->pair_transmitted;
    g_wbr_pair_command.ack_count = link->pair_acks;
    g_wbr_pair_command.network_state = link->network_state;
}
