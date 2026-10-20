#ifndef L2_SERVICE_SCPI_SERVER_H
#define L2_SERVICE_SCPI_SERVER_H

/* NetTask-side only: owns the SCPI TCP socket's lifecycle (listen/accept/
 * close) and moves raw bytes between the socket and scpi_rx_queue/
 * scpi_tx_queue. Does not parse SCPI or know command syntax — see
 * scpi_short_task.h for that, and openspec/changes/nettask-scpi-task-split
 * for the full split rationale. Named scpi_socket_poll (not scpi_server_poll)
 * precisely because it's transport-only; "server" implied protocol handling
 * that doesn't live here. Call scpi_socket_poll() from NetTask's loop;
 * nothing to initialize on this side (no per-connection state lives here
 * anymore), so there is no scpi_server_init(). */
void scpi_socket_poll(void);

#endif
