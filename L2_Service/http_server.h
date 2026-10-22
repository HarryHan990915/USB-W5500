#ifndef L2_SERVICE_HTTP_SERVER_H
#define L2_SERVICE_HTTP_SERVER_H

#include <stdint.h>

/* NetTask-side only: owns the HTTP TCP socket's lifecycle (listen/accept/
 * close) and moves raw bytes between the socket and http_rx_queue/
 * http_tx_queue. Does not parse HTTP or know what a request looks like —
 * see http_task.h for that. Same split as scpi_server.c/scpi_short_task.c —
 * see openspec/changes/nettask-scpi-task-split for the rationale. Call
 * http_socket_poll() from NetTask's loop, passing the shared SIR snapshot
 * (see net_task_poll_fn_t in app_tasks.h). */
void http_socket_poll(uint8_t sir);

#endif
