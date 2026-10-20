#ifndef L2_SERVICE_TELNET_SERVER_H
#define L2_SERVICE_TELNET_SERVER_H

/* NetTask-side only: owns the telnet TCP socket's lifecycle (listen/accept/
 * close) and sends whatever telnet_queue_push() (uart_log_write()'s tap) has
 * queued to the connected client. Log-mirror only — incoming bytes are read
 * and discarded, never parsed, so this never needs a companion task the way
 * SCPI's scpi_short_task is. Call telnet_socket_poll() from NetTask's loop. */
void telnet_socket_poll(void);

#endif
