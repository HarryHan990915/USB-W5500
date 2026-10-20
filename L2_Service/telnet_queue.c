#include "telnet_queue.h"
#include "uart_log.h"
#include <string.h>

osMessageQueueId_t telnet_tx_queueHandle;

static void (*s_tx_wake_fn)(void) = 0;

void telnet_queue_init(void)
{
    telnet_tx_queueHandle = osMessageQueueNew(TELNET_QUEUE_DEPTH, sizeof(telnet_queue_msg_t), NULL);
    uart_log_set_tap(telnet_queue_push);
}

void telnet_queue_push(const char *msg, uint16_t len)
{
    if (telnet_tx_queueHandle == NULL) {
        return;
    }
    if (len > TELNET_QUEUE_MSG_LEN) {
        len = TELNET_QUEUE_MSG_LEN;
    }

    telnet_queue_msg_t qmsg;
    qmsg.len = len;
    memcpy(qmsg.data, msg, len);

    if (osMessageQueuePut(telnet_tx_queueHandle, &qmsg, 0, 0) == osOK) {
        telnet_queue_notify_tx();
    }
    /* Full queue (no telnet client draining it): drop silently, see header. */
}

void telnet_queue_register_tx_wake(void (*wake_fn)(void))
{
    s_tx_wake_fn = wake_fn;
}

void telnet_queue_notify_tx(void)
{
    if (s_tx_wake_fn != 0) {
        s_tx_wake_fn();
    }
}
