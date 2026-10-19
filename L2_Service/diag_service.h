#ifndef L2_SERVICE_DIAG_SERVICE_H
#define L2_SERVICE_DIAG_SERVICE_H

#include <stdint.h>

void diag_log(const char *msg);
void diag_run_w5500_selftest(void);
int diag_link_up(void);
/* Full-system self-test backing *TST?: returns 0 on pass, nonzero on fail. */
int diag_run_full_selftest(void);

/* TIM2-backed microsecond latency instrumentation, backing DIAGnostic:LATency?.
 * mark_start/mark_end bracket one SCPI command's receive-to-reply-sent span. */
void diag_latency_init(void);
void diag_latency_mark_start(void);
void diag_latency_mark_end(void);
uint32_t diag_latency_get_last_us(void);

#endif
