#include "app_main.h"
#include "diag_service.h"
#include "net_service.h"
#include "scpi_commands.h"
#include "config_service.h"
#include "test_led.h"
#include "led_bench.h"
#include "app_tasks.h"

void app_init(void)
{
    diag_log("scpi_G071RB: app_init OK\r\n");
    diag_run_w5500_selftest();
    diag_latency_init();
    test_led_init();
    led_bench_init();
    config_service_load_and_apply();
    net_service_init();
    net_service_register_int_wake(app_net_wake_from_isr);
    scpi_commands_register();
}
