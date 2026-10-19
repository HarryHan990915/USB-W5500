#include "sys_time.h"
#include "stm32g0xx_hal.h"

extern TIM_HandleTypeDef htim2;

uint32_t sys_tick_ms(void)
{
    return HAL_GetTick();
}

void sys_time_init(void)
{
    HAL_TIM_Base_Start(&htim2);
}

uint32_t sys_time_us(void)
{
    return __HAL_TIM_GET_COUNTER(&htim2);
}
