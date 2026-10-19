#ifndef L2_SERVICE_NET_SERVICE_H
#define L2_SERVICE_NET_SERVICE_H

#include <stdint.h>

typedef enum {
    NET_WAIT_LINK = 0,
    NET_DHCP_RUNNING,
    NET_BOUND,
    NET_STATIC_FALLBACK,
    NET_SERVING,
} net_state_t;

void net_service_init(void);
void net_service_set_static_fallback(const uint8_t ip[4], const uint8_t sn[4], const uint8_t gw[4]);
/* Wires wake_fn to fire from the W5500 INT EXTI ISR, so a task blocked on
 * osThreadFlagsWait() returns immediately on new data/link activity instead
 * of waiting out its poll timeout. */
void net_service_register_int_wake(void (*wake_fn)(void));
void net_service_poll(void);
net_state_t net_service_get_state(void);
int net_service_is_serving(void);

#endif
