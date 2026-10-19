#include "test_led.h"
#include "main.h"

void test_led_init(void)
{
    HAL_GPIO_WritePin(TEST_LED_GPIO_Port, TEST_LED_Pin, GPIO_PIN_RESET);
}

void test_led_set(int on)
{
    HAL_GPIO_WritePin(TEST_LED_GPIO_Port, TEST_LED_Pin, on ? GPIO_PIN_SET : GPIO_PIN_RESET);
}
