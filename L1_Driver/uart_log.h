#ifndef L1_DRIVER_UART_LOG_H
#define L1_DRIVER_UART_LOG_H

#include <stdint.h>

void uart_log_write(const char *msg, uint16_t len);

#endif
