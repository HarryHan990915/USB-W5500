#include "http_server.h"
#include "net_service.h"
#include "w5500_port.h"
#include "http_queue.h"
#include "sys_time.h"
#include "diag_service.h"
#include "socket.h"
#include "cmsis_os.h"
#include <string.h>

/* HTTP_SLOTS is 1 today, so a single flag/deadline is enough — same
 * single-slot precedent as scpi_queue.h's SCPI_SLOTS. Generalize this to a
 * per-slot array if HTTP_SLOTS ever grows. */
static uint8_t s_awaiting_sendok = 0;
static uint32_t s_sendok_deadline_ms;

/* Bounds how long a socket waits for SENDOK before being force-closed (see
 * check_sendok()) — long enough for a normal LAN round trip, short enough
 * that a peer that vanishes mid-transfer (browser tab closed, cable pulled)
 * can't wedge the one HTTP socket forever. */
#define HTTP_SENDOK_TIMEOUT_MS 2000U

static void socket_poll(uint8_t sock_no)
{
    uint8_t status = getSn_SR(sock_no);

    switch (status) {
    case SOCK_CLOSED: {
        /* Flush any stale not-yet-processed bytes left in http_rx_queue from
         * a now-dead connection, same reasoning as scpi_server.c. */
        http_queue_msg_t stale;
        while (osMessageQueueGet(http_rx_queueHandle, &stale, NULL, 0) == osOK) { }

        socket(sock_no, Sn_MR_TCP, HTTP_PORT, SF_TCP_NODELAY);
        listen(sock_no);
        break;
    }
    case SOCK_CLOSE_WAIT:
        disconnect(sock_no);
        /* CON/DISCON latch fix (see w5500_clear_socket_con_discon_ir's doc
         * comment) — only called here and in the SOCK_ESTABLISHED case below,
         * the only two transitions that can ever set CON/DISCON. Clearing
         * unconditionally every poll (the original approach) meant a pointless
         * extra SPI write every single cycle this socket spent idle in
         * SOCK_LISTEN waiting for a request — which, combined with telnet's
         * socket doing the same thing, was adding a lot of continuous SPI bus
         * traffic on an electrically marginal jump-wired link. Confirmed as
         * the actual cause of this board's mystery MCU resets by disabling
         * telnet/http polling entirely and seeing resets all but disappear. */
        w5500_clear_socket_con_discon_ir(sock_no);
        break;
    case SOCK_ESTABLISHED: {
        static uint8_t buf[HTTP_QUEUE_MSG_LEN];
        int32_t n = w5500_hardened_recv(sock_no, buf, sizeof(buf));
        if (n > 0) {
            http_queue_msg_t msg;
            msg.sock_no = sock_no;
            msg.len = (uint16_t)n;
            memcpy(msg.data, buf, (size_t)n);
            (void)osMessageQueuePut(http_rx_queueHandle, &msg, 0, 50);
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
    if (s_awaiting_sendok) {
        /* The previous reply hasn't been confirmed transmitted yet (see
         * check_sendok()) — don't start another send or let anything close
         * this socket out from under it. HTTP_SLOTS==1 means there's only
         * ever one reply in flight anyway. */
        return;
    }

    http_queue_msg_t msg;
    if (osMessageQueueGet(http_tx_queueHandle, &msg, NULL, 0) == osOK) {
        if (getSn_TX_FSR(msg.sock_no) < msg.len) {
            /* Vendored send()'s internal "wait for TX room" loop
             * (ThirdParty/ioLibrary_Driver/Ethernet/socket.c) has no
             * timeout at all — if the TX buffer can't free up fast enough
             * (stalled browser, or the link drops a few ACKs on this
             * jump-wired board) that loop spins NetTask forever, starving
             * HousekeepTask's IWDG refresh and resetting the board ~4s
             * later. Give up on this reply (HTTP has no retry path once
             * the client's gone quiet) instead of ever calling send()
             * without guaranteed room. */
            diag_log("CLOSING sock3(HTTP) tx full\r\n");
            close(msg.sock_no);
            return;
        }
        send(msg.sock_no, msg.data, msg.len);
        s_awaiting_sendok = 1;
        s_sendok_deadline_ms = sys_tick_ms() + HTTP_SENDOK_TIMEOUT_MS;
    }
}

/* This vendored send() (ThirdParty/ioLibrary_Driver/Ethernet/socket.c) only
 * waits for Sn_CR to clear — confirming the SEND command was accepted, not
 * that the data actually left the wire. It only checks Sn_IR_SENDOK (real
 * completion) at the start of that *same socket's next* send() call, via its
 * own sock_is_sending bookkeeping. scpi_server.c never hits this gap because
 * it never closes after a reply — the connection stays open, and whenever
 * the *next* reply needs sending, that next send() call does the actual
 * waiting before touching the wire again. HTTP has no "next call": one reply
 * per connection, then close. Closing immediately after send() here was
 * tearing the connection down before the response had actually gone out —
 * truncating it and leaving the socket wedged, which is why the page loaded
 * once (lucky timing on a tiny reply) and then every reconnect attempt
 * failed afterward. Poll for Sn_IR_SENDOK instead (non-blocking check, not a
 * spin-wait — NetTask must never block here), and force-close past
 * HTTP_SENDOK_TIMEOUT_MS so a peer that vanishes mid-transfer can't wedge
 * this permanently. */
static void check_sendok(uint8_t sock_no)
{
    if (!s_awaiting_sendok) {
        return;
    }
    if ((getSn_IR(sock_no) & Sn_IR_SENDOK) ||
        (int32_t)(sys_tick_ms() - s_sendok_deadline_ms) >= 0) {
        diag_log("CLOSING sock3(HTTP)\r\n");  // temporary debug breadcrumb
        close(sock_no);
        s_awaiting_sendok = 0;
    }
}

void http_socket_poll(void)
{
    drain_tx_queue();

    if (!net_service_is_serving()) {
        return;
    }
    for (int i = 0; i < HTTP_SLOTS; i++) {
        uint8_t sock_no = (uint8_t)(HTTP_FIRST_SOCKET + i);
        check_sendok(sock_no);
        socket_poll(sock_no);
    }
}
