#ifndef L2_SERVICE_SCPI_SHORT_TASK_H
#define L2_SERVICE_SCPI_SHORT_TASK_H

#include "scpi/types.h"

/* Owns the scpi_t parser context(s) and the command dispatch table. Must be
 * called once (from app_init(), after scpi_queue_init()) before
 * scpi_short_task_run() starts consuming. */
void scpi_short_task_init(const scpi_command_t *commands,
                           const char *idn1, const char *idn2, const char *idn3, const char *idn4);

/* Runs forever: blocks on scpi_rx_queue, parses+dispatches each chunk via
 * SCPI_Input(), and short command callbacks execute inline here. Replies go
 * out via scpi_tx_queue (see scpi_write_cb in scpi_short_task.c) — this
 * function never touches ioLibrary/w5500_port directly. Call once from
 * app_scpi_short_task(). */
void scpi_short_task_run(void);

#endif
