#include "http_queue.h"

osMessageQueueId_t http_rx_queueHandle;
osMessageQueueId_t http_tx_queueHandle;

static void (*s_tx_wake_fn)(void) = 0;

void http_queue_init(void)
{
    http_rx_queueHandle = osMessageQueueNew(HTTP_QUEUE_DEPTH, sizeof(http_queue_msg_t), NULL);
    http_tx_queueHandle = osMessageQueueNew(HTTP_QUEUE_DEPTH, sizeof(http_queue_msg_t), NULL);
}

void http_queue_register_tx_wake(void (*wake_fn)(void))
{
    s_tx_wake_fn = wake_fn;
}

void http_queue_notify_tx(void)
{
    if (s_tx_wake_fn != 0) {
        s_tx_wake_fn();
    }
}
