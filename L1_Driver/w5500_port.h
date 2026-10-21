#ifndef L1_DRIVER_W5500_PORT_H
#define L1_DRIVER_W5500_PORT_H

#include <stdint.h>

typedef enum {
    DRV_OK = 0,
    DRV_ERR_SPI = -1,
} drv_status_t;

typedef struct {
    uint8_t version;
    uint8_t rw_ok;
    uint8_t link_up;
} w5500_selftest_t;

int w5500_port_init(void);
/* Re-unmasks SIMR/Sn_IMR so the chip can assert INTn again. Called once by
 * w5500_port_init() at boot, and again by net_service.c on every link-up
 * transition in case the chip's own power rail got cycled independently of
 * the MCU's (see doc comment in w5500_port.c). */
void w5500_arm_interrupts(void);
drv_status_t w5500_selftest(w5500_selftest_t *r);
int w5500_link_up(void);
void w5500_set_static_ip(const uint8_t ip[4], const uint8_t sn[4], const uint8_t gw[4]);

/* MAC is derived from the STM32's 96-bit factory-programmed unique device ID
 * (UID_BASE), so every board flashed with this firmware gets a distinct MAC
 * instead of all of them colliding on the same hardcoded address. Keeps the
 * 02:08:DC prefix (locally-administered, per IEEE 802 — bit 1 of the first
 * byte set means "not a real vendor OUI") so devices built from this project
 * stay recognizable. */
void w5500_get_mac(uint8_t mac[6]);

/* Hardened replacement for ioLibrary's recv(): that function's internal
 * socket-status/RX-size reads have no retry margin and this board's SPI
 * link occasionally misreads them under load, causing false "connection
 * lost" closes. This re-checks both reads before trusting them.
 * Returns: >0 = bytes read, 0 = no data yet (connection still fine),
 *          -1 = confirmed not established/recoverable (caller should treat
 *          the socket as gone). */
int32_t w5500_hardened_recv(uint8_t sn, uint8_t *buf, uint16_t cap);

/* Clears all latched Sn_IR bits for one socket. Per the W5500 datasheet these
 * bits stay set until software writes 1 to clear them, and INTn is a single
 * level-ORed line across all 8 sockets — so one socket's un-cleared bit holds
 * the shared pin asserted forever and blocks every other socket's future
 * falling edges too, not just its own. Sockets whose traffic isn't already
 * funneled through w5500_hardened_recv() (e.g. the DHCP socket, driven
 * directly by ioLibrary's DHCP_run()) need this called explicitly.
 *
 * DO NOT call this on a socket that uses ioLibrary's send(): send() keeps
 * its own cross-call state in Sn_IR_SENDOK (see socket.c's sock_is_sending
 * bitmask) to know whether the previous transmit actually completed, and a
 * blanket 0xFF clear here wipes that out from under it — every send() call
 * after the first then falls into send()'s "else return SOCK_BUSY" path
 * forever (until the socket is closed/reopened, which resets the bitmask),
 * silently dropping replies while look like nothing is wrong on the receive
 * side. Use w5500_clear_socket_con_discon_ir() instead for TCP sockets. */
void w5500_clear_socket_ir(uint8_t sn);

/* Clears only Sn_IR_CON and Sn_IR_DISCON — the two bits nothing else in this
 * codebase ever clears for a TCP socket (w5500_hardened_recv() clears RECV;
 * ioLibrary's send()/disconnect() manage SENDOK/TIMEOUT themselves). Without
 * this, the first CON event latches the shared INTn pin low for the rest of
 * that connection's lifetime, and every later RECV on it still sets its bit
 * internally but can't produce a new falling edge since the pin never went
 * high in between — NetTask silently stops being interrupt-woken for that
 * socket after the first event. Safe to call unconditionally every poll
 * (unlike w5500_clear_socket_ir() above), but don't: these bits can only ever
 * be set right when a socket transitions to SOCK_ESTABLISHED (CON) or
 * SOCK_CLOSE_WAIT (DISCON), so calling this on every other poll tick — most
 * of a mostly-idle socket's life, e.g. while sitting in SOCK_LISTEN with
 * nobody connected — is a pure no-op SPI write repeated forever. Call it only
 * from those two transition points (see scpi_server.c/telnet_server.c/
 * http_server.c's socket_poll() for the pattern); doing it unconditionally
 * everywhere was adding enough constant low-level SPI bus traffic, across all
 * three protocol sockets, to be the actual cause of this board's mystery MCU
 * resets on its electrically marginal jump-wired link. */
void w5500_clear_socket_con_discon_ir(uint8_t sn);

/* Raw read of Sn_IR for diagnostics — e.g. logging whether it's stuck
 * asserted (non-zero) when nothing should currently be pending. */
uint8_t w5500_get_socket_ir(uint8_t sn);

/* Raw read of a socket's free TX space / available RX bytes — each maxes out
 * at 2048 (this project's per-socket buffer allocation). A value outside
 * [0, 2048], or one stuck unchanging across repeated calls on an active
 * connection, points at corrupted buffer-pointer state (e.g. from a bad SPI
 * transaction) rather than normal traffic. */
void w5500_get_buffer_state(uint8_t sn, uint16_t *tx_fsr, uint16_t *rx_rsr);

/* Running counts of which path w5500_hardened_recv() took, since boot.
 * vote_fail/rsr_disagree/cr_timeout are the three "gave up, didn't touch
 * Sn_IR" exit paths (see w5500_hardened_recv's body) — if these are
 * incrementing a lot relative to success, the SPI link is unreliable enough
 * that Sn_IR_RECV is frequently left set (never reaching the clearing line
 * at the end), which would explain the shared INTn pin staying latched
 * despite the CON/DISCON fix in scpi_server.c. */
void w5500_get_recv_stats(uint32_t *vote_fail, uint32_t *rsr_disagree, uint32_t *cr_timeout, uint32_t *success);

/* Registers a callback invoked from the W5500 INT EXTI ISR (falling edge).
 * Kept as a plain function pointer (no RTOS type in this HAL-only layer) so
 * L2_Service can wire it to a task wake without L1_Driver depending on
 * FreeRTOS. Pass NULL to unregister. */
typedef void (*w5500_int_cb_t)(void);
void w5500_port_set_int_callback(w5500_int_cb_t cb);

#endif
