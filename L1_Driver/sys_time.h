#ifndef L1_DRIVER_SYS_TIME_H
#define L1_DRIVER_SYS_TIME_H

#include <stdint.h>

uint32_t sys_tick_ms(void);

/* Starts TIM2 (configured by CubeMX as a 32-bit free-running counter at
 * 1 MHz) so sys_time_us() returns a microsecond timestamp. */
void sys_time_init(void);
uint32_t sys_time_us(void);

#endif
