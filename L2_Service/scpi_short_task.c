#include "scpi_short_task.h"
#include "scpi_queue.h"
#include "diag_service.h"
#include "scpi/parser.h"
#include "scpi/units.h"
#include "cmsis_os.h"
#include <string.h>

#define SCPI_INPUT_BUF_LEN 300
#define SCPI_ERROR_QUEUE_LEN 16

typedef struct {
    uint8_t sock_no;
    scpi_t ctx;
    char input_buf[SCPI_INPUT_BUF_LEN];
    scpi_error_t error_queue[SCPI_ERROR_QUEUE_LEN];
} scpi_slot_t;

static scpi_slot_t s_slots[SCPI_SLOTS];
static scpi_interface_t s_interface;

static size_t scpi_write_cb(scpi_t *context, const char *data, size_t len)
{
    scpi_slot_t *slot = (scpi_slot_t *)context->user_context;

    if (len > SCPI_QUEUE_MSG_LEN) {
        /* Shouldn't happen — every reply this firmware currently produces
         * (IDN string, "OK", small status text) fits well under this.
         * Truncate defensively rather than overflow the queue message. */
        len = SCPI_QUEUE_MSG_LEN;
    }

    scpi_queue_msg_t msg;
    msg.sock_no = slot->sock_no;
    msg.len = (uint16_t)len;
    memcpy(msg.data, data, len);

    diag_latency_mark_end();

    /* Bounded wait, not instant-drop: NetTask wakes immediately once this is
     * queued (see scpi_server.c's tx drain + wake), so under normal
     * operation this never actually blocks. A short timeout here is cheaper
     * than silently losing a reply if NetTask is ever briefly behind.
     * Whether the actual send() over SPI succeeds is NetTask's problem from
     * here on — this function only reports whether the hand-off happened. */
    if (osMessageQueuePut(scpi_tx_queueHandle, &msg, 0, 50) != osOK) {
        return 0;
    }
    scpi_queue_notify_tx();
    return len;
}

void scpi_short_task_init(const scpi_command_t *commands,
                           const char *idn1, const char *idn2, const char *idn3, const char *idn4)
{
    memset(&s_interface, 0, sizeof(s_interface));
    s_interface.write = scpi_write_cb;

    for (int i = 0; i < SCPI_SLOTS; i++) {
        scpi_slot_t *slot = &s_slots[i];
        slot->sock_no = (uint8_t)(SCPI_FIRST_SOCKET + i);
        SCPI_Init(&slot->ctx, commands, &s_interface, scpi_units_def,
                  idn1, idn2, idn3, idn4,
                  slot->input_buf, sizeof(slot->input_buf),
                  slot->error_queue, SCPI_ERROR_QUEUE_LEN);
        slot->ctx.user_context = slot;
    }
}

static scpi_slot_t *find_slot(uint8_t sock_no)
{
    for (int i = 0; i < SCPI_SLOTS; i++) {
        if (s_slots[i].sock_no == sock_no) {
            return &s_slots[i];
        }
    }
    return NULL;
}

void scpi_short_task_run(void)
{
    scpi_queue_msg_t msg;
    for (;;) {
        if (osMessageQueueGet(scpi_rx_queueHandle, &msg, NULL, osWaitForever) != osOK) {
            continue;
        }
        scpi_slot_t *slot = find_slot(msg.sock_no);
        if (slot == NULL) {
            continue;
        }
        diag_latency_mark_start();
        SCPI_Input(&slot->ctx, (const char *)msg.data, (int)msg.len);
    }
}
