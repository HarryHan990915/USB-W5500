#include "led_bench.h"
#include "test_led.h"
#include "diag_service.h"
#include "cmsis_os.h"
#include <stdio.h>

#define LED_BENCH_DEFAULT_INTERVAL_MS 500
#define LED_BENCH_MIN_INTERVAL_MS 1
#define LED_BENCH_MAX_INTERVAL_MS 60000
#define LED_BENCH_QUEUE_DEPTH 4 /* queued start requests; NetTask never blocks on a full queue */

typedef struct {
    int running;
    int done;
    int total;
    int interval_ms; /* on-time and off-time per blink */
} led_bench_state_t;

static led_bench_state_t s_state;
static osMutexId_t s_mutex;
static const osMutexAttr_t s_mutex_attr = {
    .name = "led_bench_mutex",
    .attr_bits = osMutexPrioInherit,
};
static osMessageQueueId_t s_queue;

/* Depth-1 queue used purely as an abort signal, waited on (with a timeout)
 * from inside the blink loop instead of a plain osDelay(). osDelay() can't
 * be woken early by anything, which is what used to make TEST:LED:ABORt
 * take up to a full blink period to take effect; a queue get-with-timeout
 * returns the instant something is put. */
static osMessageQueueId_t s_abort_queue;

void led_bench_init(void)
{
    s_mutex = osMutexNew(&s_mutex_attr);
    s_queue = osMessageQueueNew(LED_BENCH_QUEUE_DEPTH, sizeof(int), NULL);
    s_abort_queue = osMessageQueueNew(1, sizeof(int), NULL);
    s_state.interval_ms = LED_BENCH_DEFAULT_INTERVAL_MS;
}

void led_bench_set_interval_ms(int ms)
{
    if (ms < LED_BENCH_MIN_INTERVAL_MS) {
        ms = LED_BENCH_MIN_INTERVAL_MS;
    }
    if (ms > LED_BENCH_MAX_INTERVAL_MS) {
        ms = LED_BENCH_MAX_INTERVAL_MS;
    }
    osMutexAcquire(s_mutex, osWaitForever);
    s_state.interval_ms = ms;
    osMutexRelease(s_mutex);
}

int led_bench_get_interval_ms(void)
{
    int ms;
    osMutexAcquire(s_mutex, osWaitForever);
    ms = s_state.interval_ms;
    osMutexRelease(s_mutex);
    return ms;
}

int led_bench_request_start(int count)
{
    if (count < 0) {
        count = 0;
    }
    if (count > 600) {
        /* Same guard as before: don't let a typo tie up ScpiLongTask for hours. */
        count = 600;
    }
    /* Non-blocking put: ScpiShortTask must never block here. If the queue is full
     * (LED_BENCH_QUEUE_DEPTH requests already waiting), reject instead of
     * silently dropping — the caller reports SCPI_ERROR_QUEUE_OVERFLOW. */
    return osMessageQueuePut(s_queue, &count, 0, 0) == osOK ? 1 : 0;
}

void led_bench_request_abort(void)
{
    /* Non-blocking put: if a previous abort is still sitting unconsumed in
     * this depth-1 queue, a stop is already pending, so a failed put here is
     * fine to ignore. */
    int dummy = 1;
    (void)osMessageQueuePut(s_abort_queue, &dummy, 0, 0);
}

void led_bench_get_status(int *running, int *done, int *total)
{
    osMutexAcquire(s_mutex, osWaitForever);
    *running = s_state.running;
    *done = s_state.done;
    *total = s_state.total;
    osMutexRelease(s_mutex);
}

void led_bench_run(void)
{
    int count;

    /* Blocks here — no idle polling — until a request is enqueued. This is
     * what lets ScpiLongTask sit at zero CPU between benchmark runs instead of
     * waking up every 100ms to check a flag. */
    if (osMessageQueueGet(s_queue, &count, NULL, osWaitForever) != osOK) {
        return;
    }
    if (count <= 0) {
        return;
    }

    /* Discard any abort left pending from before this run started (e.g. a
     * stray TEST:LED:ABORt while idle) — it must not abort the sequence
     * that's about to start. */
    int dummy;
    while (osMessageQueueGet(s_abort_queue, &dummy, NULL, 0) == osOK) { }

    osMutexAcquire(s_mutex, osWaitForever);
    s_state.running = 1;
    s_state.done = 0;
    s_state.total = count;
    osMutexRelease(s_mutex);

    for (int i = 0; i < count; i++) {
        /* Re-read every blink (not snapshotted once for the whole run) so a
         * TEST:LED:INTerval change takes effect on the very next blink,
         * instead of only on the next TEST:LED:BLINk sequence. */
        int interval = led_bench_get_interval_ms();
        int aborted = 0;

        /* Waiting on s_abort_queue instead of osDelay() so ABORt cuts this
         * short the instant it's requested, instead of only being noticed
         * after both halves of the blink finish. */
        test_led_set(1);
        if (osMessageQueueGet(s_abort_queue, &dummy, NULL, interval) == osOK) {
            aborted = 1;
        }
        test_led_set(0);
        if (!aborted && osMessageQueueGet(s_abort_queue, &dummy, NULL, interval) == osOK) {
            aborted = 1;
        }

        osMutexAcquire(s_mutex, osWaitForever);
        s_state.done = i + 1;
        osMutexRelease(s_mutex);

        char msg[32];
        int len = snprintf(msg, sizeof(msg), "LED: blink %d/%d\r\n", i + 1, count);
        if (len > 0) {
            diag_log(msg);
        }

        if (aborted) {
            diag_log("LED: aborted\r\n");
            break;
        }
    }

    osMutexAcquire(s_mutex, osWaitForever);
    s_state.running = 0;
    osMutexRelease(s_mutex);
}
