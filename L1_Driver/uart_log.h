#ifndef L1_DRIVER_UART_LOG_H
#define L1_DRIVER_UART_LOG_H

#include <stdint.h>

void uart_log_write(const char *msg, uint16_t len);

/* Registers a callback invoked with every uart_log_write() call, in addition
 * to the UART transmit itself. Kept as a plain function pointer (no RTOS/
 * queue type in this HAL-only layer) so L2_Service can mirror the log stream
 * elsewhere (telnet) without this layer depending on L2_Service — same
 * dependency-injection pattern as w5500_port_set_int_callback(). Pass NULL to
 * unregister. */
typedef void (*uart_log_tap_t)(const char *msg, uint16_t len);
void uart_log_set_tap(uart_log_tap_t tap);

#endif
