#ifndef L2_SERVICE_TELNET_QUEUE_H
#define L2_SERVICE_TELNET_QUEUE_H

#include <stdint.h>
#include "cmsis_os.h"

/* Same three-piece split as scpi_queue.h/http_queue.h, adapted for telnet's
 * one-directional job (mirroring uart_log_write()'s stream — what you
 * currently see on COM6 — to a connected telnet client on port 23; there's
 * no command parsing on the way in, this isn't an interactive shell):
 *
 *   uart_log_write()'s tap -> telnet_log_queue -> TelnetTask -> telnet_tx_queue -> NetTask
 *
 * NetTask never sees a raw log line, only finished bytes on telnet_tx_queue
 * ready to send() — same as it never sees raw SCPI/HTTP request bytes'
 * meaning. TelnetTask is the one place that knows "this came from
 * uart_log_write()" today; if telnet ever grows real logic (filtering,
 * timestamps, an interactive command set), it lands there, not in NetTask.
 * See telnet_server.h for the NetTask-side socket state machine and
 * openspec/changes/nettask-scpi-task-split for the pattern this follows. */

#define TELNET_PORT 23
#define TELNET_SOCKET 2 /* socket 0 = DHCP, socket 1 = SCPI (see scpi_queue.h) */

/* Sized for one uart_log_write() call's worth of text; the longest existing
 * log line (diag_run_w5500_selftest()'s self-test line) is under 100 bytes. */
#define TELNET_QUEUE_MSG_LEN 128U
#define TELNET_QUEUE_DEPTH 8U

typedef struct {
    uint16_t len;
    uint8_t data[TELNET_QUEUE_MSG_LEN];
} telnet_queue_msg_t;

/* Producer side (any task, via uart_log_write()'s tap) -> TelnetTask. */
extern osMessageQueueId_t telnet_log_queueHandle;
/* TelnetTask -> NetTask: ready to send() as-is. */
extern osMessageQueueId_t telnet_tx_queueHandle;

/* Creates both queues and registers telnet_queue_push() as uart_log_write()'s
 * tap, so every existing diag_log()/log_line()/log_ip() call site mirrors to
 * telnet automatically with zero changes to those files. Must run after
 * osKernelInitialize() and before anything calls uart_log_write() that you
 * want mirrored (a few boot-time lines before this runs will only reach
 * COM6 — harmless, since no telnet client could be connected that early
 * anyway). */
void telnet_queue_init(void);

/* Pushes one log line onto telnet_log_queue, truncating if longer than
 * TELNET_QUEUE_MSG_LEN. Non-blocking and drop-on-full rather than
 * backpressuring the caller: log lines aren't mission-critical the way SCPI
 * replies are, and the caller here is whichever task just called
 * uart_log_write() (NetTask, ScpiShortTask, or HousekeepTask depending on who
 * logged) — stalling any of them to protect a telnet viewer would be a much
 * worse trade. Safe to call before telnet_queue_init() runs (no-ops) and from
 * any task context. */
void telnet_queue_push(const char *msg, uint16_t len);

/* Registers the function TelnetTask calls right after queuing a line onto
 * telnet_tx_queue, so NetTask wakes immediately to transmit it instead of
 * waiting out its periodic timeout — mirrors scpi_queue_register_tx_wake(). */
void telnet_queue_register_tx_wake(void (*wake_fn)(void));

/* Calls the registered tx-wake function (no-op if none registered yet). */
void telnet_queue_notify_tx(void);

#endif
