#include "scpi_queue.h"

osMessageQueueId_t scpi_rx_queueHandle;
osMessageQueueId_t scpi_tx_queueHandle;

static void (*s_tx_wake_fn)(void) = 0;

void scpi_queue_init(void)
{
    scpi_rx_queueHandle = osMessageQueueNew(SCPI_QUEUE_DEPTH, sizeof(scpi_queue_msg_t), NULL);
    scpi_tx_queueHandle = osMessageQueueNew(SCPI_QUEUE_DEPTH, sizeof(scpi_queue_msg_t), NULL);
}

void scpi_queue_register_tx_wake(void (*wake_fn)(void))
{
    s_tx_wake_fn = wake_fn;
}

void scpi_queue_notify_tx(void)
{
    if (s_tx_wake_fn != 0) {
        s_tx_wake_fn();
    }
}
