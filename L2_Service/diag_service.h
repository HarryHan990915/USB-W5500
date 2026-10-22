#ifndef L2_SERVICE_DIAG_SERVICE_H
#define L2_SERVICE_DIAG_SERVICE_H

#include <stdint.h>

void diag_log(const char *msg);
void diag_run_w5500_selftest(void);
int diag_link_up(void);
/* Full-system self-test backing *TST?: returns 0 on pass, nonzero on fail. */
int diag_run_full_selftest(void);

/* Logs W5500 socket-1 (the SCPI TCP socket) Sn_IR plus w5500_hardened_recv()'s
 * running exit-path counters, to pin down whether the shared INTn pin staying
 * latched is because Sn_IR never clears (vote_fail/rsr_disagree/cr_timeout
 * climbing relative to success) versus something else. */
void diag_log_scpi_recv_stats(void);

void diag_log_buffer_stats(void);

/* Logs scpi_rx_queue/scpi_tx_queue drop counts (see
 * scpi_queue_note_rx_drop()/scpi_queue_note_tx_drop() doc comment) — these
 * count commands/replies silently lost because the other side's queue
 * stayed full for the full bounded wait, which used to leave zero trace. */
void diag_log_scpi_queue_stats(void);

/* TIM2-backed microsecond latency instrumentation, backing DIAGnostic:LATency?.
 * mark_start/mark_end bracket one SCPI command's receive-to-reply-sent span. */
void diag_latency_init(void);
void diag_latency_mark_start(void);
void diag_latency_mark_end(void);
uint32_t diag_latency_get_last_us(void);

#endif
