#include "app_tasks.h"
#include "diag_service.h"
#include "net_service.h"
#include "w5500_port.h"
#include "scpi_short_task.h"
#include "http_task.h"
#include "led_bench.h"
#include "cmsis_os.h"
#include <stdio.h>
#include <string.h>

/* Task handles created in main.c. Most via CubeMX-generated osThreadNew()
 * calls; HttpTaskHandle is hand-added inside USER CODE markers instead (not
 * in the .ioc's FreeRTOS task list — see the note by HttpTaskHandle's
 * declaration). */
extern osThreadId_t NetTaskHandle;
extern osThreadId_t ScpiShortTaskHandle;
extern osThreadId_t ScpiLongTaskHandle;
extern osThreadId_t HttpTaskHandle;
extern osThreadId_t HousekeepTaskHandle;

#define NET_WAKE_FLAG 0x1U

/* Counts *why* app_net_task woke each cycle — real event-driven wake (either
 * the W5500 INT EXTI, or ScpiShortTask signaling a reply is ready on
 * scpi_tx_queue — both call app_net_wake_from_isr(), so this counter no
 * longer distinguishes the two, just "woken for a reason" vs. "the 5s
 * timeout fired with nothing to do". Read (non-atomically) from
 * HousekeepTask in report_stack()'s neighborhood below; a torn read of a
 * free-running counter is an acceptable diagnostic-only tradeoff, same as
 * the STACK watermarks already logged there. */
static volatile uint32_t s_net_isr_wakes = 0;
static volatile uint32_t s_net_timeout_wakes = 0;

/* Registered protocol poll functions — see net_task_register_poll() doc
 * comment in app_tasks.h. Populated once during app_init() (before
 * osKernelStart()), then only ever read from here on, so no locking around
 * it is needed once NetTask starts. */
static net_task_poll_fn_t s_poll_fns[NET_TASK_MAX_POLLS];
static int s_poll_fn_count = 0;

int net_task_register_poll(net_task_poll_fn_t poll_fn)
{
    if (poll_fn == NULL || s_poll_fn_count >= NET_TASK_MAX_POLLS) {
        return 0;
    }
    s_poll_fns[s_poll_fn_count++] = poll_fn;
    return 1;
}

void app_net_task(void *argument)
{
    (void)argument;
    for (;;) {
        /* Primarily woken via app_net_wake_from_isr() now, from two sources:
         * the W5500 INT EXTI (data/connection events on any socket NetTask
         * owns get handled immediately), and ScpiShortTask after it queues a
         * reply onto scpi_tx_queue (so replies go out promptly instead of
         * waiting on the periodic timeout below) — see
         * scpi_queue_register_tx_wake() in app_main.c.
         *
         * The 5 s timeout is NOT a "missed interrupt" safety net — it's load
         * bearing for two things the W5500 has no interrupt source for at
         * all: PHYCFGR's link-up/link-down bit is a plain status bit with no
         * associated IR event (see w5500.h's IR_* defines — only CONFLICT/
         * UNREACH/PPPoE/MP exist at chip level, nothing for PHY link
         * change), so link_up_debounced() can only ever learn the link came
         * up by being polled; and DHCP_time_handler()'s retransmit timer is
         * time-driven, not event-driven, and needs a periodic call regardless
         * of socket traffic (net_service.c's own DHCP_TIMEOUT_MS=15000
         * fallback still works fine at this coarser granularity — it just
         * advances in ~5 s steps instead of ~1 s). Without this periodic
         * wake, NetTask would block forever on osWaitForever right out of
         * reset (nothing has happened on any socket yet to raise an
         * interrupt), and DHCP would never even start. */
        uint32_t flags = osThreadFlagsWait(NET_WAKE_FLAG, osFlagsWaitAny, 3000);
        if (flags == osFlagsErrorTimeout) {
            s_net_timeout_wakes++;
        } else if ((flags & osFlagsError) == 0) {
            s_net_isr_wakes++;
        }
        net_service_poll();

        /* One SIR read per wake, shared by every registered poller, instead
         * of each poller unconditionally servicing every socket it owns on
         * every wake (which is what "an interrupt fired somewhere" used to
         * get treated as, since the EXTI itself can't say which socket):
         * SIR's bit n mirrors socket n's masked Sn_IR live, so a 0 bit means
         * genuinely nothing pending there right now, not just "we didn't
         * check" — see w5500_get_sir() doc comment. Skipped entirely before
         * the link is up: no sockets are open yet, so the read would just be
         * wasted SPI traffic, and pollers already no-op on
         * !net_service_is_serving(). */
        uint8_t sir = net_service_is_serving() ? w5500_get_sir() : 0;
        for (int i = 0; i < s_poll_fn_count; i++) {
            s_poll_fns[i](sir);
        }
    }
}

void app_net_wake_from_isr(void)
{
    osThreadFlagsSet(NetTaskHandle, NET_WAKE_FLAG);
}

void app_scpi_short_task(void *argument)
{
    (void)argument;
    /* scpi_short_task_run() is itself an infinite loop (blocks on
     * scpi_rx_queue, parses, dispatches) — no outer for(;;) needed here. */
    scpi_short_task_run();
}

void app_http_task(void *argument)
{
    (void)argument;
    /* http_task_run() is itself an infinite loop (blocks on http_rx_queue,
     * buffers the request, replies with the static page) — no outer for(;;)
     * needed here. */
    http_task_run();
}

void app_scpi_long_task(void *argument)
{
    (void)argument;
    /* This is where long-running SCPI command work (started by a
     * ScpiShortTask command handler, like TEST:LED:BLINk) actually executes,
     * so neither NetTask nor ScpiShortTask ever blocks on it and progress can
     * still be polled (TEST:LED:STATus?) while it's running. Renamed from
     * app_scpi_task: it never parsed SCPI, it only ever ran led_bench_run()
     * — see openspec/changes/nettask-scpi-task-split. */
    for (;;) {
        led_bench_run();
    }
}

#ifdef DEBUG
/* Per-task stack high-water-mark logging — Debug-build only (see .cproject:
 * DEBUG is defined for the Debug configuration, not Release). Compiled out
 * entirely in Release, not just silenced: zero flash/CPU/UART cost in
 * production, and nothing here leaks task-layout details into a shipped
 * build's log output. osThreadGetStackSpace() reports the WORST-EVER free
 * space for that task since boot (FreeRTOS's uxTaskGetStackHighWaterMark()
 * is a one-way ratchet, not a live reading) — sampling it periodically is
 * what makes the trend ("is some task's margin shrinking over time")
 * visible on COM6 across a long run. */
static void report_stack(const char *name, osThreadId_t handle)
{
    uint32_t bytes_free = osThreadGetStackSpace(handle);
    char buf[48];
    int n = snprintf(buf, sizeof(buf), "STACK %s: %lu bytes free\r\n", name, (unsigned long)bytes_free);
    if (n > 0) {
        diag_log(buf);
    }
}
#endif

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
#ifdef DEBUG
            report_stack("Net", NetTaskHandle);
            report_stack("ScpiShort", ScpiShortTaskHandle);
            report_stack("ScpiLong", ScpiLongTaskHandle);
            report_stack("Http", HttpTaskHandle);
            report_stack("Housekeep", HousekeepTaskHandle);
#endif

            char wake_buf[64];
            int wn = snprintf(wake_buf, sizeof(wake_buf), "NET WAKE: isr=%lu poll=%lu int_pin=%d\r\n",
                               (unsigned long)s_net_isr_wakes, (unsigned long)s_net_timeout_wakes,
                               app_read_w5500_int_pin());
            if (wn > 0) {
                diag_log(wake_buf);
            }
            diag_log_scpi_recv_stats();
            diag_log_scpi_queue_stats();
            diag_log_buffer_stats();
        }

        osDelay(1000);
    }
}
