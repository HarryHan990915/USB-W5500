#include "uart_log.h"
#include "stm32g0xx_hal.h"

extern UART_HandleTypeDef huart2;

void uart_log_write(const char *msg, uint16_t len)
{
    HAL_UART_Transmit(&huart2, (const uint8_t *)msg, len, HAL_MAX_DELAY);
}
