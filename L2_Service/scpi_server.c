#include "scpi_server.h"
#include "net_service.h"
#include "w5500_port.h"
#include "scpi_queue.h"
#include "diag_service.h"
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

        /* Same move for scpi_tx_queue: a reply ScpiShortTask already queued
         * for the connection that just died (e.g. the client disconnected —
         * cleanly or via RST — before NetTask got around to draining it)
         * used to sit here until the NEXT connection on this socket number
         * came along, at which point drain_tx_queue() would ship this
         * leftover ghost reply out to that unrelated new client (or, if that
         * new connection wasn't up yet, fail to send it and log a confusing
         * "CLOSING sock1(SCPI)" for a connection that's already gone) —
         * corrupting that session's reply order instead of just being
         * dropped. Safe to flush ALL of scpi_tx_queue here: SCPI_SLOTS==1
         * today, so every queued message already targets this one socket;
         * revisit (filter by sock_no) if SCPI_SLOTS ever grows. */
        while (osMessageQueueGet(scpi_tx_queueHandle, &stale, NULL, 0) == osOK) { }

        socket(sock_no, Sn_MR_TCP, SCPI_PORT, SF_TCP_NODELAY);
        listen(sock_no);
        break;
    }
    case SOCK_CLOSE_WAIT:
        disconnect(sock_no);
        /* CON/DISCON are never cleared anywhere else on this socket's normal
         * path, which would otherwise latch the shared INTn pin low for the
         * rest of the connection (see w5500_clear_socket_con_discon_ir doc
         * comment). Deliberately NOT the blanket w5500_clear_socket_ir() here
         * — this socket's replies go through ioLibrary's send(), which keeps
         * its own cross-call state in Sn_IR_SENDOK and breaks (silently drops
         * every reply after the first) if that bit gets wiped out from under
         * it. Only called here and in the SOCK_ESTABLISHED case below — the
         * only two transitions that can ever set CON/DISCON — not
         * unconditionally every poll: clearing on every tick while idly
         * sitting in SOCK_LISTEN with nobody connected was a pointless extra
         * SPI write repeated forever, and across all three protocol sockets
         * that constant low-level SPI chatter turned out to be the actual
         * cause of this board's mystery MCU resets (confirmed by disabling
         * telnet/http polling and seeing resets all but disappear). */
        w5500_clear_socket_con_discon_ir(sock_no);
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
             * waits. If it's still full 50 ticks later, the command is lost
             * — count it instead of silently discarding, see
             * scpi_queue_note_rx_drop() doc comment. */
            if (osMessageQueuePut(scpi_rx_queueHandle, &msg, 0, 50) != osOK) {
                scpi_queue_note_rx_drop();
            }
        }
        w5500_clear_socket_con_discon_ir(sock_no);
        break;
    }
    default:
        /* SOCK_LISTEN (the normal idle-waiting state) or a transient status —
         * nothing to clear, see above. */
        break;
    }
}

static void drain_tx_queue(void)
{
    scpi_queue_msg_t msg;
    while (osMessageQueueGet(scpi_tx_queueHandle, &msg, NULL, 0) == osOK) {
        if (getSn_TX_FSR(msg.sock_no) < msg.len) {
            /* Vendored send()'s internal "wait for TX room" loop
             * (ThirdParty/ioLibrary_Driver/Ethernet/socket.c) has no
             * timeout at all — if the TX buffer can't free up fast enough
             * (client stopped reading, or the link drops a few ACKs on
             * this jump-wired board) that loop spins NetTask forever,
             * starving HousekeepTask's IWDG refresh and resetting the
             * board ~4s later. Re-queue and retry next poll instead of
             * ever calling send() without guaranteed room. */
            (void)osMessageQueuePut(scpi_tx_queueHandle, &msg, 0, 0);
            break;
        }
        int32_t sent = send(msg.sock_no, msg.data, msg.len);
        if (sent == 0) {
            /* SOCK_BUSY, not a failure: ioLibrary's send() won't start a new
             * transmit on this socket until the previous one's Sn_IR_SENDOK
             * confirmation comes back from the peer (see socket.c's
             * sock_is_sending bookkeeping) — a real TCP round trip, so it
             * can never have arrived yet if this is the 2nd+ message drained
             * out of scpi_tx_queue in the same pass (e.g. a client that
             * fired off several SCPI queries back-to-back, queuing several
             * replies before NetTask got a chance to drain any of them).
             * Treating this the same as a real send failure was
             * force-closing a perfectly healthy connection on nothing more
             * than "two replies queued up at once". Put it back and stop
             * draining this round instead — the rest of the queue gets
             * force-closed along with it otherwise, since the socket's
             * already SOCK_CLOSED by then. Next poll (or the SENDOK IRQ
             * wake) retries once the previous send actually confirms. */
            (void)osMessageQueuePut(scpi_tx_queueHandle, &msg, 0, 0);
            break;
        }
        if (sent < 0) {
            /* A real failure (SOCKERR_*), not SOCK_BUSY. ioLibrary's send()
             * keeps its own cross-call "is the previous transmit confirmed
             * yet" state (sock_is_sending, private static in socket.c) gated
             * on correctly reading Sn_IR_SENDOK back over SPI. A single
             * misread on this board's noisy bus can leave that state
             * wedged, and every send() call after that returns an error
             * forever — we have no way to reset it directly since it's
             * private to socket.c. close() does reset it as a side effect of
             * tearing the socket down, though, so force that here: this one
             * bad reply costs the current connection (client has to
             * reconnect), not every reply from then on. socket_poll()'s next
             * iteration sees SOCK_CLOSED and re-listens for a fresh client. */
            diag_log("CLOSING sock1(SCPI)\r\n");
            close(msg.sock_no);
        }
    }
}

void scpi_socket_poll(void)
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
