#include "telnet_task.h"
#include "telnet_queue.h"
#include "cmsis_os.h"

void telnet_task_run(void)
{
    telnet_queue_msg_t msg;
    for (;;) {
        if (osMessageQueueGet(telnet_log_queueHandle, &msg, NULL, osWaitForever) != osOK) {
            continue;
        }
        /* Plain forward today — see header. Still a real hop (not an inline
         * call from the tap) so NetTask and the tap stay exactly as ignorant
         * of "telnet" as they are of "SCPI"/"HTTP" semantics. */
        if (osMessageQueuePut(telnet_tx_queueHandle, &msg, 0, 50) == osOK) {
            telnet_queue_notify_tx();
        }
    }
}
