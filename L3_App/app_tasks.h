#ifndef L3_APP_APP_TASKS_H
#define L3_APP_APP_TASKS_H

/* Hard limit, not a tunable: the W5500 has exactly 8 hardware sockets (SIMR/
 * each socket's Sn_IMR is one bit of an 8-bit register — see
 * w5500_arm_interrupts() in L1_Driver/w5500_port.c), so this firmware can
 * never meaningfully own more than 8 independent per-socket protocol
 * pollers. Sized to the hardware, not picked arbitrarily. */
#define NET_TASK_MAX_POLLS 8

typedef void (*net_task_poll_fn_t)(void);

/* Registers a function NetTask calls once per poll cycle, after
 * net_service_poll() has updated link/DHCP state — the pattern every
 * protocol socket poller (scpi_socket_poll(), http_socket_poll(), ...)
 * already follows: check net_service_is_serving() internally, then poll its
 * own socket(s). Adding a new protocol is then just one call here, with no
 * changes to app_net_task()/app_tasks.c itself.
 *
 * Must be called during app_init() — before osKernelStart() — not safe to
 * call once NetTask is already running (no locking around the registry).
 * Returns 1 on success, 0 if NET_TASK_MAX_POLLS is already full. */
int net_task_register_poll(net_task_poll_fn_t poll_fn);

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
