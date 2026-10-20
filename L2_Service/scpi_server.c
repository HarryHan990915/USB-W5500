#include "scpi_server.h"
#include "net_service.h"
#include "diag_service.h"
#include "w5500_port.h"
#include "socket.h"
#include "scpi/parser.h"
#include "scpi/units.h"
#include <string.h>

#define SCPI_PORT 5025
#define SCPI_SLOTS 1 /* TODO: multi-client (2-3 sockets) deferred — see note in net-service spec review */
#define SCPI_FIRST_SOCKET 1 /* socket 0 is reserved for DHCP (net_service) */
#define SCPI_INPUT_BUF_LEN 300
#define SCPI_ERROR_QUEUE_LEN 16
#define SCPI_RECV_BUF_LEN 300

typedef struct {
    uint8_t sock_no;
    scpi_t ctx;
    char input_buf[SCPI_INPUT_BUF_LEN];
    scpi_error_t error_queue[SCPI_ERROR_QUEUE_LEN];
    int open;
} scpi_slot_t;

static scpi_slot_t s_slots[SCPI_SLOTS];
static scpi_interface_t s_interface;

static size_t scpi_write_cb(scpi_t *context, const char *data, size_t len)
{
    scpi_slot_t *slot = (scpi_slot_t *)context->user_context;
    int32_t sent = send(slot->sock_no, (uint8_t *)data, (uint16_t)len);
    diag_latency_mark_end();
    if (sent <= 0) {
        /* ioLibrary's send() keeps its own cross-call "is the previous
         * transmit confirmed yet" state (sock_is_sending, private static in
         * socket.c) gated on correctly reading Sn_IR_SENDOK back over SPI.
         * A single misread on this board's noisy bus (same class of issue
         * w5500_hardened_recv() already works around for the receive side)
         * can leave that state wedged, and every send() call after that
         * returns SOCK_BUSY forever — we have no way to reset it directly
         * since it's private to socket.c. close() does reset it as a side
         * effect of tearing the socket down, though, so force that here:
         * this one bad reply costs the current connection (client has to
         * reconnect), not every reply from then on. slot_poll()'s next
         * iteration sees SOCK_CLOSED and re-listens for a fresh client. */
        close(slot->sock_no);
        return 0;
    }
    return (size_t)sent;
}

void scpi_server_init(const scpi_command_t *commands,
                       const char *idn1, const char *idn2, const char *idn3, const char *idn4)
{
    memset(&s_interface, 0, sizeof(s_interface));
    s_interface.write = scpi_write_cb;

    for (int i = 0; i < SCPI_SLOTS; i++) {
        scpi_slot_t *slot = &s_slots[i];
        slot->sock_no = (uint8_t)(SCPI_FIRST_SOCKET + i);
        slot->open = 0;
        SCPI_Init(&slot->ctx, commands, &s_interface, scpi_units_def,
                  idn1, idn2, idn3, idn4,
                  slot->input_buf, sizeof(slot->input_buf),
                  slot->error_queue, SCPI_ERROR_QUEUE_LEN);
        slot->ctx.user_context = slot;
    }
}

static void slot_poll(scpi_slot_t *slot)
{
    uint8_t status = getSn_SR(slot->sock_no);

    switch (status) {
    case SOCK_CLOSED:
        /* NOTE: this vendored ioLibrary's recv() checks sock_io_mode (the
         * SF_IO_NONBLOCK bit) and returns SOCK_BUSY unconditionally *before*
         * ever checking whether data is actually available — a non-blocking
         * socket here can never receive real data via recv(), it only ever
         * returns busy or, once the peer tears the connection down, the
         * SOCKERR_SOCKSTATUS error path. Using SF_TCP_NODELAY instead keeps
         * the socket blocking, which actually works; revisit for a proper
         * non-blocking fix (manual getSn_RX_RSR check + raw read) at the
         * FreeRTOS migration. */
        socket(slot->sock_no, Sn_MR_TCP, SCPI_PORT, SF_TCP_NODELAY);
        listen(slot->sock_no);
        break;
    case SOCK_CLOSE_WAIT:
        disconnect(slot->sock_no);
        break;
    case SOCK_ESTABLISHED: {
        static uint8_t buf[SCPI_RECV_BUF_LEN];
        /* Hardened: ioLibrary's own recv() re-reads Sn_SR/Sn_RX_RSR internally
         * with no retry margin, and a single misread there was closing
         * perfectly good connections (see w5500_hardened_recv doc comment). */
        int32_t n = w5500_hardened_recv(slot->sock_no, buf, sizeof(buf));
        if (n > 0) {
            diag_latency_mark_start();
            SCPI_Input(&slot->ctx, (const char *)buf, (int)n);
        }
        break;
    }
    default:
        break;
    }

    /* CON/DISCON are never cleared anywhere else on this socket's normal
     * path, which would otherwise latch the shared INTn pin low for the rest
     * of the connection (see w5500_clear_socket_con_discon_ir doc comment).
     * Deliberately NOT the blanket w5500_clear_socket_ir() here — this
     * socket's replies go through ioLibrary's send(), which keeps its own
     * cross-call state in Sn_IR_SENDOK and breaks (silently drops every
     * reply after the first) if that bit gets wiped out from under it. */
    w5500_clear_socket_con_discon_ir(slot->sock_no);
}

void scpi_server_poll(void)
{
    if (!net_service_is_serving()) {
        return;
    }
    for (int i = 0; i < SCPI_SLOTS; i++) {
        slot_poll(&s_slots[i]);
    }
}
