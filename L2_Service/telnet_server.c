#include "telnet_server.h"
#include "net_service.h"
#include "w5500_port.h"
#include "telnet_queue.h"
#include "diag_service.h"
#include "socket.h"
#include "cmsis_os.h"

static void drain_tx_queue(void)
{
    telnet_queue_msg_t msg;
    while (osMessageQueueGet(telnet_tx_queueHandle, &msg, NULL, 0) == osOK) {
        if (getSn_SR(TELNET_SOCKET) != SOCK_ESTABLISHED) {
            continue; /* no client connected: discard, see telnet_queue.h */
        }
        if (getSn_TX_FSR(TELNET_SOCKET) < msg.len) {
            /* Vendored send()'s internal "wait for TX room" loop
             * (ThirdParty/ioLibrary_Driver/Ethernet/socket.c) has no
             * timeout at all — if the telnet peer isn't draining fast
             * enough (or the link drops a few ACKs on this jump-wired
             * board) that loop spins NetTask forever, starving
             * HousekeepTask's IWDG refresh and resetting the board ~4s
             * later. Drop this line instead of ever calling send() when
             * there isn't guaranteed room — telnet is a lossy log mirror
             * by design (see the no-client-connected case above). */
            continue;
        }
        int32_t sent = send(TELNET_SOCKET, msg.data, msg.len);
        if (sent == 0) {
            /* SOCK_BUSY, not a failure — same reasoning as scpi_server.c's
             * drain_tx_queue(): the previous line's Sn_IR_SENDOK confirmation
             * hasn't come back yet, which is expected when several log lines
             * get queued faster than one TCP round trip. Drop this one
             * (telnet is a lossy log mirror by design, see the
             * no-client-connected case above) instead of force-closing a
             * perfectly healthy connection over normal send pacing. */
            continue;
        }
        if (sent < 0) {
            /* A real failure (SOCKERR_*), not SOCK_BUSY — force-close rather
             * than leaving ioLibrary's internal sock_is_sending state wedged
             * for every line after this one. */
            diag_log("CLOSING sock2(Telnet)\r\n");  // temporary debug breadcrumb
            close(TELNET_SOCKET);
        }
    }
}

static void discard_rx(void)
{
    static uint8_t scratch[32];
    /* Telnet clients commonly send option-negotiation bytes (IAC ...) right
     * after connecting; this is log-mirror-only and never reads them for
     * meaning — just drain so the W5500's RX FIFO doesn't fill up and stall
     * the connection. */
    (void)w5500_hardened_recv(TELNET_SOCKET, scratch, sizeof(scratch));
}

static void socket_poll(void)
{
    uint8_t status = getSn_SR(TELNET_SOCKET);

    switch (status) {
    case SOCK_CLOSED:
        socket(TELNET_SOCKET, Sn_MR_TCP, TELNET_PORT, SF_TCP_NODELAY);
        listen(TELNET_SOCKET);
        break;
    case SOCK_CLOSE_WAIT:
        disconnect(TELNET_SOCKET);
        /* CON/DISCON latch fix (see w5500_clear_socket_con_discon_ir's doc
         * comment) — without this the shared INTn pin stays asserted after a
         * connect/disconnect and every socket stops being interrupt-woken.
         * Only called here and in the SOCK_ESTABLISHED case below — those are
         * the only two transitions that can ever actually set CON/DISCON, so
         * clearing anywhere else (notably: every poll while idly sitting in
         * SOCK_LISTEN with nobody connected, which is most of this socket's
         * life) was a pure no-op SPI write repeated forever. Across telnet's
         * and HTTP's sockets — both idle most of the time — that was a lot of
         * continuous, pointless SPI bus traffic on an electrically marginal
         * jump-wired link, which correlated with this board's mystery MCU
         * resets (confirmed by disabling telnet/http polling entirely and
         * seeing resets all but disappear). */
        w5500_clear_socket_con_discon_ir(TELNET_SOCKET);
        break;
    case SOCK_ESTABLISHED:
        discard_rx();
        w5500_clear_socket_con_discon_ir(TELNET_SOCKET);
        break;
    default:
        /* SOCK_LISTEN (the normal idle-waiting state) or a transient status —
         * nothing to clear, see above. */
        break;
    }
}

void telnet_socket_poll(void)
{
    /* Drained unconditionally, same reasoning as scpi_server.c: flush/discard
     * whatever's queued even if the link just dropped, instead of letting it
     * linger until some future connection reuses this socket number. */
    drain_tx_queue();

    if (!net_service_is_serving()) {
        return;
    }
    socket_poll();
}
