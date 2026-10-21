#include "http_task.h"
#include "http_queue.h"
#include "cmsis_os.h"
#include <stdio.h>
#include <string.h>

#define HTTP_INPUT_BUF_LEN 300U

typedef struct {
    uint8_t sock_no;
    uint8_t in_use;
    char buf[HTTP_INPUT_BUF_LEN];
    uint16_t len;
} http_slot_t;

static http_slot_t s_slots[HTTP_SLOTS];

/* Fixed content: this firmware doesn't look at the request's method or path
 * at all, every request gets this same page back. See http_server.h for why
 * that means no routing logic belongs here either. */
static const char s_body[] =
    "<!DOCTYPE html><html><head><title>Cable Tester</title></head>"
    "<body><h1>Cable Tester</h1></body></html>";

static http_slot_t *claim_slot(uint8_t sock_no)
{
    for (int i = 0; i < HTTP_SLOTS; i++) {
        if (s_slots[i].in_use && s_slots[i].sock_no == sock_no) {
            return &s_slots[i];
        }
    }
    /* First byte seen for a socket not already tracked here: claim slot 0
     * (HTTP_SLOTS==1 today) for it. Any previous occupant's half-built
     * buffer is necessarily from a dead connection, since NetTask only
     * reuses a socket number once it's gone back to SOCK_CLOSED. */
    http_slot_t *slot = &s_slots[0];
    slot->sock_no = sock_no;
    slot->in_use = 1;
    slot->len = 0;
    return slot;
}

static int headers_complete(const char *buf, uint16_t len)
{
    for (uint16_t i = 3; i < len; i++) {
        if (buf[i - 3] == '\r' && buf[i - 2] == '\n' && buf[i - 1] == '\r' && buf[i] == '\n') {
            return 1;
        }
    }
    return 0;
}

static void send_response(uint8_t sock_no)
{
    http_queue_msg_t msg;
    msg.sock_no = sock_no;

    int header_len = snprintf((char *)msg.data, HTTP_QUEUE_MSG_LEN,
        "HTTP/1.1 200 OK\r\n"
        "Content-Type: text/html\r\n"
        "Content-Length: %u\r\n"
        "Connection: close\r\n"
        "\r\n",
        (unsigned)(sizeof(s_body) - 1));
    if (header_len < 0) {
        return;
    }

    uint16_t len = (uint16_t)header_len;
    uint16_t body_space = HTTP_QUEUE_MSG_LEN - len;
    uint16_t body_len = (uint16_t)(sizeof(s_body) - 1);
    if (body_len > body_space) {
        body_len = body_space; /* defensive — s_body comfortably fits today */
    }
    memcpy(msg.data + len, s_body, body_len);
    len = (uint16_t)(len + body_len);
    msg.len = len;

    if (osMessageQueuePut(http_tx_queueHandle, &msg, 0, 50) == osOK) {
        http_queue_notify_tx();
    }
}

void http_task_run(void)
{
    http_queue_msg_t msg;
    for (;;) {
        if (osMessageQueueGet(http_rx_queueHandle, &msg, NULL, osWaitForever) != osOK) {
            continue;
        }

        http_slot_t *slot = claim_slot(msg.sock_no);

        uint16_t copy_len = msg.len;
        if ((uint32_t)slot->len + copy_len > HTTP_INPUT_BUF_LEN) {
            copy_len = HTTP_INPUT_BUF_LEN - slot->len;
        }
        memcpy(slot->buf + slot->len, msg.data, copy_len);
        slot->len = (uint16_t)(slot->len + copy_len);

        /* Every HTTP request ends its header block with a blank line — that
         * (or a full buffer, as a bound) is the only thing worth waiting for
         * before replying, since the reply never depends on the request's
         * content. */
        if (headers_complete(slot->buf, slot->len) || slot->len >= HTTP_INPUT_BUF_LEN) {
            send_response(slot->sock_no);
            slot->in_use = 0;
            slot->len = 0;
        }
    }
}
