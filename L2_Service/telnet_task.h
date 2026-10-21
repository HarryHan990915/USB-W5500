#ifndef L2_SERVICE_TELNET_TASK_H
#define L2_SERVICE_TELNET_TASK_H

/* Owns the "what does telnet actually do with a log line" side of things.
 * Today that's a plain forward (telnet_log_queue -> telnet_tx_queue) since
 * telnet is log-mirror-only, but this is the place future logic (filtering,
 * timestamps, an interactive command set) would land — never in NetTask or
 * in uart_log_write()'s tap. See telnet_queue.h. */
void telnet_task_run(void);

#endif
