#include "app_main.h"
#include "diag_service.h"
#include "net_service.h"
#include "scpi_commands.h"
#include "scpi_queue.h"
#include "http_queue.h"
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
    scpi_queue_init();
    http_queue_init();
    config_service_load_and_apply();
    net_service_init();
    net_service_register_int_wake(app_net_wake_from_isr);
    /* Same callback as the EXTI wake above: ScpiShortTask uses this to wake
     * NetTask immediately after queuing a reply, instead of NetTask waiting
     * out its periodic link-check timeout to notice (see
     * openspec/changes/nettask-scpi-task-split). */
    scpi_queue_register_tx_wake(app_net_wake_from_isr);
    /* Same pattern for HTTP: wake NetTask the instant HttpTask has a reply
     * ready instead of waiting on its periodic timeout. */
    http_queue_register_tx_wake(app_net_wake_from_isr);
    scpi_commands_register();
}
