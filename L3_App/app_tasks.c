#include "app_tasks.h"
#include "diag_service.h"
#include "net_service.h"
#include "scpi_server.h"
#include "led_bench.h"
#include "cmsis_os.h"
#include <stdio.h>
#include <string.h>

/* Task handles created in main.c (CubeMX-generated osThreadNew calls). */
extern osThreadId_t NetTaskHandle;
extern osThreadId_t ScpiTaskHandle;
extern osThreadId_t HousekeepTaskHandle;

#define NET_WAKE_FLAG 0x1U

/* Counts *why* app_net_task woke each cycle — real EXTI-driven wake vs. the
 * 2ms timeout firing with nothing to do. Read (non-atomically) from
 * HousekeepTask in report_stack()'s neighborhood below; a torn read of a
 * free-running counter is an acceptable diagnostic-only tradeoff, same as
 * the STACK watermarks already logged there. */
static volatile uint32_t s_net_isr_wakes = 0;
static volatile uint32_t s_net_timeout_wakes = 0;

void app_net_task(void *argument)
{
    (void)argument;
    for (;;) {
        /* Primarily woken by the W5500 INT EXTI via app_net_wake_from_isr()
         * now — actual data/connection events (DHCP packets, SCPI traffic)
         * all raise that interrupt and get handled immediately, no polling
         * needed for them.
         *
         * The 1 s timeout is NOT a "missed interrupt" safety net — it's load
         * bearing for two things the W5500 has no interrupt source for at
         * all: PHYCFGR's link-up/link-down bit is a plain status bit with no
         * associated IR event (see w5500.h's IR_* defines — only CONFLICT/
         * UNREACH/PPPoE/MP exist at chip level, nothing for PHY link
         * change), so link_up_debounced() can only ever learn the link came
         * up by being polled; and DHCP_time_handler()'s retransmit timer is
         * time-driven, not event-driven, and needs a roughly-once-per-second
         * call regardless of socket traffic. Without this periodic wake,
         * NetTask would block forever on osWaitForever right out of reset
         * (nothing has happened on any socket yet to raise an interrupt),
         * and DHCP would never even start. */
        uint32_t flags = osThreadFlagsWait(NET_WAKE_FLAG, osFlagsWaitAny, 1000);
        if (flags == osFlagsErrorTimeout) {
            s_net_timeout_wakes++;
        } else if ((flags & osFlagsError) == 0) {
            s_net_isr_wakes++;
        }
        net_service_poll();
        scpi_server_poll();
    }
}

void app_net_wake_from_isr(void)
{
    osThreadFlagsSet(NetTaskHandle, NET_WAKE_FLAG);
}

void app_scpi_task(void *argument)
{
    (void)argument;
    /* SCPI command parsing/dispatch still runs inline inside
     * scpi_server_poll() (called from NetTask) — this task isn't a general
     * command-queue consumer. It's specifically where long-running work
     * (started by a NetTask SCPI handler, like TEST:LED:BLINk) actually
     * executes, so NetTask never blocks on it and progress can still be
     * polled (TEST:LED:STATus?) while it's running. */
    for (;;) {
        led_bench_run();
    }
}

static void report_stack(const char *name, osThreadId_t handle)
{
    uint32_t bytes_free = osThreadGetStackSpace(handle);
    char buf[48];
    int n = snprintf(buf, sizeof(buf), "STACK %s: %lu bytes free\r\n", name, (unsigned long)bytes_free);
    if (n > 0) {
        diag_log(buf);
    }
}

void app_housekeep_task(void *argument)
{
    (void)argument;
    int last_link = -1;
    int tick = 0;

    for (;;) {
        /* Lowest-priority task: if any higher-priority task (NetTask in
         * particular) ever gets stuck spinning instead of blocking/yielding,
         * this task starves and stops refreshing, and the IWDG resets the
         * board instead of it staying wedged until someone power-cycles it. */
        app_iwdg_refresh();

        int link = diag_link_up();
        if (link != last_link) {
            last_link = link;
            diag_log(link ? "LINK: UP\r\n" : "LINK: DOWN\r\n");
        }

        if (++tick >= 10) { /* stack watermarks every ~10s, not every tick */
            tick = 0;
            report_stack("Net", NetTaskHandle);
            report_stack("Scpi", ScpiTaskHandle);
            report_stack("Housekeep", HousekeepTaskHandle);

            char wake_buf[64];
            int wn = snprintf(wake_buf, sizeof(wake_buf), "NET WAKE: isr=%lu poll=%lu int_pin=%d\r\n",
                               (unsigned long)s_net_isr_wakes, (unsigned long)s_net_timeout_wakes,
                               app_read_w5500_int_pin());
            if (wn > 0) {
                diag_log(wake_buf);
            }
        }

        osDelay(1000);
    }
}
