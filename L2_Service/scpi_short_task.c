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
    /* Accumulates every scpi_write_cb() call for the command(s) currently
     * being processed by ONE SCPI_Input() call, so the whole reply (content,
     * the ";" delimiter between chained queries, the trailing line ending —
     * each written via its OWN separate interface->write() call internally,
     * see ThirdParty/scpi-parser/libscpi/src/parser.c's writeDelimiter()/
     * writeNewLine()) goes out as ONE atomic scpi_tx_queue message instead
     * of several. Splitting them used to let the pieces of a single reply
     * get separated by the W5500's SOCK_BUSY gap between back-to-back
     * send() calls (waiting for the previous piece's Sn_IR_SENDOK, a real
     * TCP round trip) — queuing one piece at a time meant a busy piece
     * blocked every later piece behind it in scpi_tx_queue's FIFO, including
     * unrelated replies to later commands, for several poll cycles. See
     * scpi_flush_output() below. */
    uint8_t out_buf[SCPI_QUEUE_MSG_LEN];
    uint16_t out_len;
} scpi_slot_t;

static scpi_slot_t s_slots[SCPI_SLOTS];
static scpi_interface_t s_interface;

static size_t scpi_write_cb(scpi_t *context, const char *data, size_t len)
{
    scpi_slot_t *slot = (scpi_slot_t *)context->user_context;

    size_t space = SCPI_QUEUE_MSG_LEN - slot->out_len;
    if (len > space) {
        /* Shouldn't happen — every reply this firmware currently produces
         * (IDN string, "OK", small status text, up to a handful chained with
         * ';') fits well under this. Truncate defensively rather than
         * overflow the output buffer. */
        len = space;
    }
    memcpy(slot->out_buf + slot->out_len, data, len);
    slot->out_len = (uint16_t)(slot->out_len + len);

    diag_latency_mark_end();
    return len;
}

/* Flushes everything scpi_write_cb() buffered during the SCPI_Input() call
 * that just returned, as one atomic scpi_tx_queue message — see the
 * scpi_slot_t.out_buf doc comment above. No-op if the input didn't produce
 * any output (e.g. a non-query command with no ack, or an input line with no
 * complete command in it yet). */
static void scpi_flush_output(scpi_slot_t *slot)
{
    if (slot->out_len == 0) {
        return;
    }

    scpi_queue_msg_t msg;
    msg.sock_no = slot->sock_no;
    msg.len = slot->out_len;
    memcpy(msg.data, slot->out_buf, slot->out_len);
    slot->out_len = 0;

    /* Bounded wait, not instant-drop: NetTask wakes immediately once this is
     * queued (see scpi_server.c's tx drain + wake), so under normal
     * operation this never actually blocks. A short timeout here is cheaper
     * than silently losing a reply if NetTask is ever briefly behind.
     * Whether the actual send() over SPI succeeds is NetTask's problem from
     * here on — this function only reports whether the hand-off happened. */
    if (osMessageQueuePut(scpi_tx_queueHandle, &msg, 0, 50) != osOK) {
        /* Reply lost — scpi_tx_queue stayed full for 50 ticks straight.
         * Count it instead of silently discarding, see
         * scpi_queue_note_tx_drop() doc comment. */
        scpi_queue_note_tx_drop();
        return;
    }
    scpi_queue_notify_tx();
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
        scpi_flush_output(slot);
    }
}
