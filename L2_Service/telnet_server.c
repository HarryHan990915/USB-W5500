#include "telnet_server.h"
#include "net_service.h"
#include "w5500_port.h"
#include "telnet_queue.h"
#include "socket.h"
#include "cmsis_os.h"

static void drain_tx_queue(void)
{
    telnet_queue_msg_t msg;
    while (osMessageQueueGet(telnet_tx_queueHandle, &msg, NULL, 0) == osOK) {
        if (getSn_SR(TELNET_SOCKET) != SOCK_ESTABLISHED) {
            continue; /* no client connected: discard, see telnet_queue.h */
        }
        int32_t sent = send(TELNET_SOCKET, msg.data, msg.len);
        if (sent <= 0) {
            /* Same move as scpi_server.c's drain_tx_queue(): force-close on a
             * failed send rather than leaving ioLibrary's internal
             * sock_is_sending state wedged for every line after this one. */
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
        break;
    case SOCK_ESTABLISHED:
        discard_rx();
        break;
    default:
        break;
    }

    /* Same CON/DISCON latch fix as scpi_server.c's socket_poll() — without
     * this the shared INTn pin stays asserted after the first connect/
     * disconnect and every socket stops being interrupt-woken. */
    w5500_clear_socket_con_discon_ir(TELNET_SOCKET);
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
