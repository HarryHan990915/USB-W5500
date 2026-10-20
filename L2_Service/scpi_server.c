#include "scpi_server.h"
#include "net_service.h"
#include "w5500_port.h"
#include "scpi_queue.h"
#include "socket.h"
#include "cmsis_os.h"
#include <string.h>

/* NOTE: this vendored ioLibrary's recv() checks sock_io_mode (the
 * SF_IO_NONBLOCK bit) and returns SOCK_BUSY unconditionally *before* ever
 * checking whether data is actually available — a non-blocking socket here
 * can never receive real data via recv(), it only ever returns busy or, once
 * the peer tears the connection down, the SOCKERR_SOCKSTATUS error path.
 * Using SF_TCP_NODELAY instead keeps the socket blocking, which actually
 * works; revisit for a proper non-blocking fix (manual getSn_RX_RSR check +
 * raw read) at the FreeRTOS migration. */

static void socket_poll(uint8_t sock_no)
{
    uint8_t status = getSn_SR(sock_no);

    switch (status) {
    case SOCK_CLOSED: {
        /* Flush any stale not-yet-processed bytes left in scpi_rx_queue from
         * a now-dead connection, so a fresh connection on this socket number
         * doesn't have an old command's bytes delivered to it out of
         * context. */
        scpi_queue_msg_t stale;
        while (osMessageQueueGet(scpi_rx_queueHandle, &stale, NULL, 0) == osOK) { }

        socket(sock_no, Sn_MR_TCP, SCPI_PORT, SF_TCP_NODELAY);
        listen(sock_no);
        break;
    }
    case SOCK_CLOSE_WAIT:
        disconnect(sock_no);
        break;
    case SOCK_ESTABLISHED: {
        static uint8_t buf[SCPI_QUEUE_MSG_LEN];
        /* Hardened: ioLibrary's own recv() re-reads Sn_SR/Sn_RX_RSR internally
         * with no retry margin, and a single misread there was closing
         * perfectly good connections (see w5500_hardened_recv doc comment). */
        int32_t n = w5500_hardened_recv(sock_no, buf, sizeof(buf));
        if (n > 0) {
            scpi_queue_msg_t msg;
            msg.sock_no = sock_no;
            msg.len = (uint16_t)n;
            memcpy(msg.data, buf, (size_t)n);
            /* Bounded wait, not instant-drop: by the time w5500_hardened_recv()
             * returns, this data is already drained out of the W5500's RX
             * FIFO — dropping it here loses it for good instead of just
             * delaying it. ScpiShortTask is normally blocked waiting on this
             * exact queue, so under normal operation this never actually
             * waits. */
            (void)osMessageQueuePut(scpi_rx_queueHandle, &msg, 0, 50);
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
    w5500_clear_socket_con_discon_ir(sock_no);
}

static void drain_tx_queue(void)
{
    scpi_queue_msg_t msg;
    while (osMessageQueueGet(scpi_tx_queueHandle, &msg, NULL, 0) == osOK) {
        int32_t sent = send(msg.sock_no, msg.data, msg.len);
        if (sent <= 0) {
            /* ioLibrary's send() keeps its own cross-call "is the previous
             * transmit confirmed yet" state (sock_is_sending, private static
             * in socket.c) gated on correctly reading Sn_IR_SENDOK back over
             * SPI. A single misread on this board's noisy bus can leave that
             * state wedged, and every send() call after that returns
             * SOCK_BUSY forever — we have no way to reset it directly since
             * it's private to socket.c. close() does reset it as a side
             * effect of tearing the socket down, though, so force that here:
             * this one bad reply costs the current connection (client has to
             * reconnect), not every reply from then on. socket_poll()'s next
             * iteration sees SOCK_CLOSED and re-listens for a fresh client. */
            close(msg.sock_no);
        }
    }
}

void scpi_server_poll(void)
{
    /* Drained unconditionally, even if the link just dropped and
     * net_service_is_serving() is about to go false: this both transmits
     * fresh replies promptly and flushes stale ones (send() on a now-closed
     * socket just fails harmlessly) instead of letting them linger in the
     * queue until some future, unrelated connection reuses this socket
     * number. */
    drain_tx_queue();

    if (!net_service_is_serving()) {
        return;
    }
    for (int i = 0; i < SCPI_SLOTS; i++) {
        socket_poll((uint8_t)(SCPI_FIRST_SOCKET + i));
    }
}
