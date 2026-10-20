#ifndef L3_APP_LED_BENCH_H
#define L3_APP_LED_BENCH_H

void led_bench_init(void);

/* On/off half-period for each blink, in milliseconds. Defaults to 500 (the
 * value that used to be hardcoded). Takes effect on the next start — an
 * in-progress sequence keeps running at the interval it started with. */
void led_bench_set_interval_ms(int ms);
int led_bench_get_interval_ms(void);

/* Called from ScpiShortTask (the TEST:LED:BLINk SCPI handler) — enqueues the
 * requested count (non-blocking put) and returns immediately; the actual
 * blinking happens in ScpiLongTask (see led_bench_run()). Multiple requests
 * queue up and run in order instead of the newest silently overwriting an
 * older one. Returns 1 if accepted, 0 if the queue is full (caller should
 * report SCPI_ERROR_QUEUE_OVERFLOW). */
int led_bench_request_start(int count);

/* Called from ScpiShortTask (the TEST:LED:ABORt SCPI handler) — asks the
 * currently running sequence (if any) to stop. Takes effect immediately
 * (wakes ScpiLongTask out of whichever half of the current blink it's in,
 * instead of waiting for the blink to finish on its own). */
void led_bench_request_abort(void);

/* Called from ScpiShortTask (the TEST:LED:STATus? SCPI handler) — non-blocking
 * read of current progress. */
void led_bench_get_status(int *running, int *done, int *total);

/* Called in a loop from ScpiLongTask. Blocks on the request queue until the next
 * start request arrives, then runs the blink sequence itself, updating
 * progress after every blink — this is what lets TEST:LED:STATus? see fresh
 * progress instead of ScpiShortTask blocking on one command for the whole
 * sequence. */
void led_bench_run(void);

#endif
