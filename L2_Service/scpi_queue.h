#ifndef L2_SERVICE_SCPI_QUEUE_H
#define L2_SERVICE_SCPI_QUEUE_H

#include <stdint.h>
#include "cmsis_os.h"

/* Shared boundary between NetTask (owns the W5500/SPI and the SCPI TCP
 * socket) and ScpiShortTask (owns the scpi_t parser context). NetTask never
 * calls SCPI_Input()/looks at command syntax; ScpiShortTask never touches
 * ioLibrary/w5500_port. Both queue handles are created once in main.c (same
 * pattern as the CubeMX-generated task handles) and referenced here via
 * extern so neither side owns the other's lifetime.
 *
 * See openspec/changes/nettask-scpi-task-split for the full rationale —
 * this is meant to be the template future protocol tasks (HTTP, ping) follow
 * too, so NetTask's core loop never needs protocol-specific changes again. */

/* Shared socket layout, needed by both sides to stay in sync: NetTask
 * iterates sock_no in [SCPI_FIRST_SOCKET, SCPI_FIRST_SOCKET+SCPI_SLOTS), and
 * ScpiShortTask's per-slot scpi_t contexts must use the same numbering for
 * scpi_rx_queue message lookups to land on the right slot. */
#define SCPI_PORT 5025
#define SCPI_SLOTS 1 /* TODO: multi-client (2-3 sockets) deferred — see note in net-service spec review */
#define SCPI_FIRST_SOCKET 1 /* socket 0 is reserved for DHCP (net_service) */

/* 300 matches this codebase's pre-existing SCPI receive-buffer sizing
 * precedent and comfortably exceeds the longest reply any current command
 * produces (the IDN string is ~35 bytes) — not a hard protocol limit, just
 * this queue message's fixed capacity. */
#define SCPI_QUEUE_MSG_LEN 300U
/* Was 4 — bumped to 8 after observing silent drops under a burst of several
 * back-to-back commands/replies arriving faster than the other side drains
 * them (e.g. a client firing off 3 queries without waiting for each reply:
 * all 3 land in one w5500_hardened_recv() call, so ScpiShortTask produces 3
 * replies back-to-back with zero gap between them). See
 * scpi_queue_note_rx_drop()/scpi_queue_note_tx_drop() below for the counters
 * that exposed this. */
#define SCPI_QUEUE_DEPTH 8U

typedef struct {
    uint8_t sock_no;
    uint16_t len;
    uint8_t data[SCPI_QUEUE_MSG_LEN];
} scpi_queue_msg_t;

/* NetTask -> ScpiShortTask: raw bytes read off the SCPI socket, unparsed. */
extern osMessageQueueId_t scpi_rx_queueHandle;
/* ScpiShortTask -> NetTask: a complete reply buffer ready to send(). */
extern osMessageQueueId_t scpi_tx_queueHandle;

/* Creates both queues. Must run after osKernelInitialize() and before either
 * NetTask or ScpiShortTask starts — called from app_init(), same as
 * led_bench_init()'s mutex/queue creation. */
void scpi_queue_init(void);

/* Registers the function ScpiShortTask calls (from scpi_write_cb()) right
 * after queuing a reply, so NetTask wakes immediately to transmit it instead
 * of waiting out its periodic link-check timeout. L2_Service can't depend on
 * L3_App's app_net_wake_from_isr() directly (layering), so this mirrors the
 * existing net_service_register_int_wake() callback-registration pattern —
 * app_init() wires the same app_net_wake_from_isr() in for both. */
void scpi_queue_register_tx_wake(void (*wake_fn)(void));

/* Calls the registered tx-wake function (no-op if none registered yet).
 * Called by scpi_write_cb() in scpi_short_task.c right after a successful
 * osMessageQueuePut() onto scpi_tx_queueHandle. */
void scpi_queue_notify_tx(void);

/* Both queue puts (scpi_server.c's RX hand-off, scpi_short_task.c's
 * scpi_write_cb() TX hand-off) use a bounded-wait osMessageQueuePut() and,
 * before this, silently discarded the message on failure — a command or
 * reply could vanish with zero trace. These count every time that happens,
 * so a real occurrence shows up in diag_log_scpi_queue_stats() instead of
 * looking identical to "nothing happened". */
void scpi_queue_note_rx_drop(void);
void scpi_queue_note_tx_drop(void);
void scpi_queue_get_drop_stats(uint32_t *rx_drops, uint32_t *tx_drops);

#endif
