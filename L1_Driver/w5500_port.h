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

/* Registers a callback invoked from the W5500 INT EXTI ISR (falling edge).
 * Kept as a plain function pointer (no RTOS type in this HAL-only layer) so
 * L2_Service can wire it to a task wake without L1_Driver depending on
 * FreeRTOS. Pass NULL to unregister. */
typedef void (*w5500_int_cb_t)(void);
void w5500_port_set_int_callback(w5500_int_cb_t cb);

#endif
