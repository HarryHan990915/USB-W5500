#ifndef L2_SERVICE_HTTP_QUEUE_H
#define L2_SERVICE_HTTP_QUEUE_H

#include <stdint.h>
#include "cmsis_os.h"

/* Shared boundary between NetTask (owns the W5500/SPI and the HTTP TCP
 * socket) and HttpTask (owns request buffering and response generation).
 * NetTask never looks at request bytes; HttpTask never touches ioLibrary/
 * w5500_port. Mirrors scpi_queue.h's split exactly — see
 * openspec/changes/nettask-scpi-task-split for the rationale. */

#define HTTP_PORT 80
#define HTTP_SLOTS 1
#define HTTP_FIRST_SOCKET 3 /* 0=DHCP, 1=SCPI (scpi_queue.h), 2=telnet (telnet_queue.h) */

/* 300 comfortably covers both a typical browser request's headers (rx) and
 * this firmware's one static reply, headers+body included (tx). */
#define HTTP_QUEUE_MSG_LEN 300U
#define HTTP_QUEUE_DEPTH 4U

typedef struct {
    uint8_t sock_no;
    uint16_t len;
    uint8_t data[HTTP_QUEUE_MSG_LEN];
} http_queue_msg_t;

/* NetTask -> HttpTask: raw bytes read off the HTTP socket, unparsed. */
extern osMessageQueueId_t http_rx_queueHandle;
/* HttpTask -> NetTask: a complete reply buffer (headers+body) ready to send(). */
extern osMessageQueueId_t http_tx_queueHandle;

/* Creates both queues. Must run after osKernelInitialize() and before either
 * NetTask or HttpTask starts — called from app_init(). */
void http_queue_init(void);

/* Registers the function HttpTask calls right after queuing a reply, so
 * NetTask wakes immediately to transmit it instead of waiting out its
 * periodic timeout — mirrors scpi_queue_register_tx_wake(). */
void http_queue_register_tx_wake(void (*wake_fn)(void));

/* Calls the registered tx-wake function (no-op if none registered yet). */
void http_queue_notify_tx(void);

#endif
