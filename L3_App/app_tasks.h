#ifndef L3_APP_APP_TASKS_H
#define L3_APP_APP_TASKS_H

void app_net_task(void *argument);
/* Owns the scpi_t parser context; parses/dispatches short SCPI commands.
 * Consumes raw bytes from scpi_rx_queue (filled by NetTask) and produces
 * reply bytes onto scpi_tx_queue (drained by NetTask) — see
 * openspec/changes/nettask-scpi-task-split. */
void app_scpi_short_task(void *argument);
/* Long-running SCPI command execution (TEST:LED:BLINk/ABORt). Renamed from
 * app_scpi_task: it never parsed SCPI, it only ran led_bench_run() — see
 * openspec/changes/nettask-scpi-task-split. */
void app_scpi_long_task(void *argument);
/* Owns HTTP request buffering and static-page reply generation. Consumes raw
 * bytes from http_rx_queue (filled by NetTask) and produces a reply onto
 * http_tx_queue (drained by NetTask) — see http_queue.h. */
void app_http_task(void *argument);
/* Owns telnet's "what to do with a log line" logic (a plain forward today).
 * Consumes raw lines from telnet_log_queue (filled by uart_log_write()'s tap)
 * and produces bytes onto telnet_tx_queue (drained by NetTask) — see
 * telnet_queue.h. */
void app_telnet_task(void *argument);
void app_housekeep_task(void *argument);

/* Registered with net_service_register_int_wake(); wakes NetTask from the
 * W5500 INT EXTI ISR instead of waiting out its poll timeout. */
void app_net_wake_from_isr(void);

/* Defined in main.c (hand-added IWDG, see note by the hiwdg declaration
 * there). Kept as a plain wrapper so app_tasks.c doesn't need the HAL
 * include path this source folder's build config doesn't have wired in. */
void app_iwdg_refresh(void);

/* Defined in main.c. Raw instantaneous level of W5500_INT_N (0/1), bypassing
 * the EXTI/ISR path entirely — lets housekeeping tell apart "the pin never
 * toggles" (wiring fault) from "it toggles but no EXTI fires" (config fault)
 * when s_net_isr_wakes stays stuck at 0. */
int app_read_w5500_int_pin(void);

#endif
