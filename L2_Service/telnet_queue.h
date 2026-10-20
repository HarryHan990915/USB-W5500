#ifndef L2_SERVICE_TELNET_QUEUE_H
#define L2_SERVICE_TELNET_QUEUE_H

#include <stdint.h>
#include "cmsis_os.h"

/* Mirrors scpi_queue.h's NetTask<->other-task boundary pattern, but
 * one-directional: this only carries log bytes out to a connected telnet
 * client (port 23), there's no command parsing on the way in — telnet here
 * is a log mirror of uart_log_write()'s stream (what you currently see on
 * COM6), not an interactive shell. See telnet_server.h for the NetTask-side
 * socket state machine and openspec/changes/nettask-scpi-task-split for the
 * queue/task pattern this follows for SCPI. */

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

/* Producer side (any task, via uart_log_write()'s tap) -> NetTask. */
extern osMessageQueueId_t telnet_tx_queueHandle;

/* Creates the queue and registers telnet_queue_push() as uart_log_write()'s
 * tap, so every existing diag_log()/log_line()/log_ip() call site mirrors to
 * telnet automatically with zero changes to those files. Must run after
 * osKernelInitialize() and before anything calls uart_log_write() that you
 * want mirrored (a few boot-time lines before this runs will only reach
 * COM6 — harmless, since no telnet client could be connected that early
 * anyway). */
void telnet_queue_init(void);

/* Pushes one log line onto the queue, truncating if longer than
 * TELNET_QUEUE_MSG_LEN. Non-blocking and drop-on-full rather than
 * backpressuring the caller: log lines aren't mission-critical the way SCPI
 * replies are, and the caller here is whichever task just called
 * uart_log_write() (NetTask, ScpiShortTask, or HousekeepTask depending on who
 * logged) — stalling any of them to protect a telnet viewer would be a much
 * worse trade. Safe to call before telnet_queue_init() runs (no-ops) and from
 * any task context. */
void telnet_queue_push(const char *msg, uint16_t len);

/* Same registration pattern as scpi_queue_register_tx_wake(): wakes NetTask
 * immediately after a line is queued instead of waiting for its periodic
 * timeout, so telnet output appears as promptly as the COM6 UART output
 * does today. */
void telnet_queue_register_tx_wake(void (*wake_fn)(void));
void telnet_queue_notify_tx(void);

#endif
