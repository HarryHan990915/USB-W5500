#include "net_service.h"
#include "w5500_port.h"
#include "uart_log.h"
#include "sys_time.h"
#include "dhcp.h"
#include "socket.h"
#include <stdio.h>
#include <string.h>

#define DHCP_SOCKET 0
#define DHCP_TIMEOUT_MS 15000U

static uint8_t s_dhcp_buf[600];
static net_state_t s_state = NET_WAIT_LINK;
static uint32_t s_state_enter_ms;
static uint32_t s_next_dhcp_tick_ms;

/* The raw PHYCFGR link bit can read a spurious single value occasionally
 * (observed on this board's jump-wired connection — likely SPI/PHY noise
 * under load, not real link loss). A single-sample-per-tick debounce isn't
 * enough if the loop is fast and the noise is bursty, so this takes a
 * time-spaced majority vote (5 samples, ~2 ms apart) and only re-evaluates
 * once per second — much harder for a short noise burst to flip. */
#define LINK_VOTE_SAMPLES 5
static int s_link_cached = 0;
static uint32_t s_next_link_check_ms;

static int link_up_debounced(void)
{
    uint32_t now = sys_tick_ms();
    if ((int32_t)(now - s_next_link_check_ms) < 0) {
        return s_link_cached;
    }
    s_next_link_check_ms = now + 1000U;

    int up_votes = 0;
    for (int i = 0; i < LINK_VOTE_SAMPLES; i++) {
        if (w5500_link_up()) {
            up_votes++;
        }
        uint32_t wait_until = sys_tick_ms() + 2U;
        while ((int32_t)(sys_tick_ms() - wait_until) < 0) { }
    }
    s_link_cached = (up_votes > LINK_VOTE_SAMPLES / 2) ? 1 : 0;
    return s_link_cached;
}

static void log_line(const char *msg)
{
    uart_log_write(msg, (uint16_t)strlen(msg));
}

static void log_ip(const char *label, const uint8_t ip[4])
{
    char buf[48];
    int n = snprintf(buf, sizeof(buf), "%s: %u.%u.%u.%u\r\n", label, ip[0], ip[1], ip[2], ip[3]);
    if (n > 0) {
        uart_log_write(buf, (uint16_t)n);
    }
}

static void close_all_sockets(void)
{
    for (uint8_t s = 0; s < 8; s++) {
        close(s);
    }
}

static void enter_state(net_state_t s)
{
    s_state = s;
    s_state_enter_ms = sys_tick_ms();
}

void net_service_init(void)
{
    enter_state(NET_WAIT_LINK);
}

net_state_t net_service_get_state(void)
{
    return s_state;
}

void net_service_register_int_wake(void (*wake_fn)(void))
{
    w5500_port_set_int_callback(wake_fn);
}

int net_service_is_serving(void)
{
    return s_state == NET_SERVING;
}

static void start_dhcp(void)
{
    log_line("NET: link up, starting DHCP\r\n");
    DHCP_init(DHCP_SOCKET, s_dhcp_buf);
    s_next_dhcp_tick_ms = sys_tick_ms() + 1000U;
    enter_state(NET_DHCP_RUNNING);
}

static uint8_t s_static_ip[4] = {192, 168, 2, 200};
static uint8_t s_static_sn[4] = {255, 255, 255, 0};
static uint8_t s_static_gw[4] = {192, 168, 2, 1};

void net_service_set_static_fallback(const uint8_t ip[4], const uint8_t sn[4], const uint8_t gw[4])
{
    memcpy(s_static_ip, ip, 4);
    memcpy(s_static_sn, sn, 4);
    memcpy(s_static_gw, gw, 4);
}

static void fall_back_to_static_ip(void)
{
    DHCP_stop();
    log_line("NET: DHCP failed/timeout, falling back to static IP\r\n");
    w5500_set_static_ip(s_static_ip, s_static_sn, s_static_gw);
    log_ip("NET: static IP", s_static_ip);
    enter_state(NET_STATIC_FALLBACK);
    enter_state(NET_SERVING);
}

static void poll_wait_link(void)
{
    if (link_up_debounced()) {
        start_dhcp();
    }
}

static void poll_dhcp_running(void)
{
    if (!link_up_debounced()) {
        DHCP_stop();
        close_all_sockets();
        enter_state(NET_WAIT_LINK);
        return;
    }

    uint32_t now = sys_tick_ms();
    if ((int32_t)(now - s_next_dhcp_tick_ms) >= 0) {
        DHCP_time_handler();
        s_next_dhcp_tick_ms = now + 1000U;
    }

    uint8_t ret = DHCP_run();
    /* DHCP_run() drives socket 0 directly via ioLibrary and never clears its
     * Sn_IR, unlike w5500_hardened_recv() which does this for the SCPI
     * socket — without it, the first DHCP packet latches socket 0's Sn_IR
     * and keeps the shared INTn pin asserted forever, silently blocking the
     * EXTI wake for every socket from then on (see w5500_clear_socket_ir doc
     * comment). */
    w5500_clear_socket_ir(DHCP_SOCKET);
    if (ret == DHCP_IP_LEASED) {
        uint8_t ip[4];
        getIPfromDHCP(ip);
        log_ip("NET: DHCP bound, IP", ip);
        enter_state(NET_BOUND);
        enter_state(NET_SERVING);
    } else if (ret == DHCP_FAILED || (int32_t)(now - s_state_enter_ms) > (int32_t)DHCP_TIMEOUT_MS) {
        fall_back_to_static_ip();
    }
}

static void poll_serving(void)
{
    if (!link_up_debounced()) {
        close_all_sockets();
        enter_state(NET_WAIT_LINK);
    }
}

void net_service_poll(void)
{
    switch (s_state) {
    case NET_WAIT_LINK:
        poll_wait_link();
        break;
    case NET_DHCP_RUNNING:
        poll_dhcp_running();
        break;
    case NET_SERVING:
        poll_serving();
        break;
    case NET_BOUND:
    case NET_STATIC_FALLBACK:
    default:
        /* transient — always folded into NET_SERVING within the same tick */
        enter_state(NET_SERVING);
        break;
    }
}
