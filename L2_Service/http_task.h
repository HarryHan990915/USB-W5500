#ifndef L2_SERVICE_HTTP_TASK_H
#define L2_SERVICE_HTTP_TASK_H

/* Owns request buffering and reply generation for the HTTP server. Consumes
 * raw bytes from http_rx_queue (filled by NetTask) and produces a complete
 * reply onto http_tx_queue (drained by NetTask) — see http_queue.h and
 * openspec/changes/nettask-scpi-task-split. Never touches ioLibrary/
 * w5500_port directly. */
void http_task_run(void);

#endif
