#include "diag_service.h"
#include "uart_log.h"
#include "w5500_port.h"
#include "sys_time.h"
#include <stdio.h>
#include <string.h>

static uint32_t s_latency_start_us;
static uint32_t s_last_latency_us;

void diag_latency_init(void)
{
    sys_time_init();
}

void diag_latency_mark_start(void)
{
    s_latency_start_us = sys_time_us();
}

void diag_latency_mark_end(void)
{
    s_last_latency_us = sys_time_us() - s_latency_start_us;
}

uint32_t diag_latency_get_last_us(void)
{
    return s_last_latency_us;
}

void diag_log(const char *msg)
{
    uart_log_write(msg, (uint16_t)strlen(msg));
}

void diag_run_w5500_selftest(void)
{
    w5500_port_init();

    w5500_selftest_t r;
    drv_status_t st = w5500_selftest(&r);

    uint8_t mac[6];
    w5500_get_mac(mac);

    char buf[128];
    int n = snprintf(buf, sizeof(buf),
        "W5500 self-test: %s, VERSIONR=0x%02X, rw_ok=%d, link_up=%d, MAC=%02X:%02X:%02X:%02X:%02X:%02X\r\n",
        (st == DRV_OK) ? "PASS" : "FAIL", r.version, r.rw_ok, r.link_up,
        mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
    if (n > 0) {
        uart_log_write(buf, (uint16_t)n);
    }
}

int diag_link_up(void)
{
    return w5500_link_up();
}

int diag_run_full_selftest(void)
{
    w5500_selftest_t r;
    drv_status_t st = w5500_selftest(&r);
    return (st == DRV_OK) ? 0 : 1;
}

void diag_log_buffer_stats(void)
{
    uint16_t tx_fsr, rx_rsr;
    char buf[96];
    int n;

    w5500_get_buffer_state(1, &tx_fsr, &rx_rsr);
    n = snprintf(buf, sizeof(buf), "BUF sock1(SCPI): tx_fsr=%u rx_rsr=%u\r\n", tx_fsr, rx_rsr);
    if (n > 0) { uart_log_write(buf, (uint16_t)n); }

    w5500_get_buffer_state(2, &tx_fsr, &rx_rsr);
    n = snprintf(buf, sizeof(buf), "BUF sock2(Telnet): tx_fsr=%u rx_rsr=%u\r\n", tx_fsr, rx_rsr);
    if (n > 0) { uart_log_write(buf, (uint16_t)n); }

    w5500_get_buffer_state(3, &tx_fsr, &rx_rsr);
    n = snprintf(buf, sizeof(buf), "BUF sock3(HTTP): tx_fsr=%u rx_rsr=%u\r\n", tx_fsr, rx_rsr);
    if (n > 0) { uart_log_write(buf, (uint16_t)n); }
}

void diag_log_scpi_recv_stats(void)
{
    /* Socket 1 mirrors scpi_server.c's SCPI_FIRST_SOCKET (socket 0 is
     * reserved for DHCP) — hardcoded here since that constant is private to
     * scpi_server.c and this is a throwaway diagnostic, not a real
     * dependency. */
    uint8_t ir = w5500_get_socket_ir(1);
    uint32_t vote_fail, rsr_disagree, cr_timeout, success;
    w5500_get_recv_stats(&vote_fail, &rsr_disagree, &cr_timeout, &success);

    char buf[96];
    int n = snprintf(buf, sizeof(buf),
        "RECV STATS: sock1_ir=0x%02X vote_fail=%lu rsr_disagree=%lu cr_timeout=%lu success=%lu\r\n",
        ir, (unsigned long)vote_fail, (unsigned long)rsr_disagree,
        (unsigned long)cr_timeout, (unsigned long)success);
    if (n > 0) {
        uart_log_write(buf, (uint16_t)n);
    }
}
