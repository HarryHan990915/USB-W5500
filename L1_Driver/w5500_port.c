#include "w5500_port.h"
#include "wizchip_conf.h"
#include "w5500.h"
#include "socket.h"
#include "main.h"
#include <string.h>

extern SPI_HandleTypeDef hspi1;

/* Tiny busy-wait inter-transaction gap. Back-to-back SPI register reads with
 * zero gap (e.g. inside ioLibrary's own internal polling loops, which this
 * project doesn't control/pace) were observed to make register reads
 * (Sn_SR, PHYCFGR) unreliable on this jump-wired board even at 250 kHz SPI
 * clock — pointing at settle time between transactions, not raw bit rate.
 * (hspi1's prescaler in main.c has since been retuned a few times to bound
 * this experimentally — check its current value/CalculateBaudRate there
 * rather than trusting the "250 kHz" figure here.)
 * cs_sel/cs_desel wrap every register access (ours and the vendored
 * library's), so pacing here covers both uniformly. */
static void spi_bus_settle(void)
{
    for (volatile int i = 0; i < 2000; i++) { }
}

static void cs_sel(void)   { HAL_GPIO_WritePin(W5500_CS_GPIO_Port, W5500_CS_Pin, GPIO_PIN_RESET); }
static void cs_desel(void) { HAL_GPIO_WritePin(W5500_CS_GPIO_Port, W5500_CS_Pin, GPIO_PIN_SET); spi_bus_settle(); }

static uint8_t spi_rb(void)
{
    uint8_t rx = 0, tx = 0xFF;
    HAL_SPI_TransmitReceive(&hspi1, &tx, &rx, 1, 10);
    return rx;
}

static void spi_wb(uint8_t b)
{
    HAL_SPI_Transmit(&hspi1, &b, 1, 10);
}

/* Burst transfers: blocking, chunked HAL_SPI_TransmitReceive for now.
 * An earlier DMA-based version (HAL_SPI_Receive_DMA/Transmit_DMA busy-waited
 * on HAL_SPI_GetState) caused the MCU to reset as soon as a real multi-byte
 * burst ran (e.g. the first TCP recv() of SCPI payload data) — likely because
 * those are uni-directional HAL calls on a peripheral configured full-duplex.
 * Reverted to blocking transfers (same primitive spi_rb/spi_wb already use)
 * until the DMA path is revisited at the FreeRTOS migration (rtos-task-model),
 * where task-notification waiting makes it safe to debug properly. */
static void spi_rburst(uint8_t *buf, uint16_t len)
{
    uint8_t tx_dummy[32];
    memset(tx_dummy, 0xFF, sizeof(tx_dummy));
    uint16_t done = 0;
    while (done < len) {
        uint16_t chunk = (uint16_t)((len - done) > sizeof(tx_dummy) ? sizeof(tx_dummy) : (len - done));
        HAL_SPI_TransmitReceive(&hspi1, tx_dummy, &buf[done], chunk, 1000);
        done = (uint16_t)(done + chunk);
    }
}

static void spi_wburst(uint8_t *buf, uint16_t len)
{
    uint8_t rx_dummy[32];
    uint16_t done = 0;
    while (done < len) {
        uint16_t chunk = (uint16_t)((len - done) > sizeof(rx_dummy) ? sizeof(rx_dummy) : (len - done));
        HAL_SPI_TransmitReceive(&hspi1, &buf[done], rx_dummy, chunk, 1000);
        done = (uint16_t)(done + chunk);
    }
}

static void w5500_hw_reset(void)
{
    HAL_GPIO_WritePin(W5500_RST_N_GPIO_Port, W5500_RST_N_Pin, GPIO_PIN_RESET);
    HAL_Delay(1); /* HAL_Delay(1) guarantees >=1 full SysTick period, well over the required 500 us */
    HAL_GPIO_WritePin(W5500_RST_N_GPIO_Port, W5500_RST_N_Pin, GPIO_PIN_SET);
    HAL_Delay(2);
}

int w5500_port_init(void)
{
    cs_desel();
    w5500_hw_reset();

    reg_wizchip_cs_cbfunc(cs_sel, cs_desel);
    reg_wizchip_spi_cbfunc(spi_rb, spi_wb);
    reg_wizchip_spiburst_cbfunc(spi_rburst, spi_wburst);

    uint8_t tx_size[8] = {2, 2, 2, 2, 2, 2, 2, 2};
    uint8_t rx_size[8] = {2, 2, 2, 2, 2, 2, 2, 2};
    int8_t ret = wizchip_init(tx_size, rx_size);
    if (ret != 0) {
        return ret;
    }

    /* IMR and SIMR both reset to 0x00, which — per the W5500 datasheet's
     * IR/SIR/SIMR description — blocks every socket event from ever
     * reaching the physical INTn pin, no matter what Sn_IMR says. That's why
     * W5500_INT_N_Pin's EXTI never fired: the NVIC/GPIO wiring in main.c was
     * always correct, the chip just never asserted the pin. Unmask all 8
     * sockets at the SIMR gate; Sn_IMR already defaults to 0xFF but is set
     * explicitly so the intent isn't left to an implicit reset value. This
     * alone isn't enough to keep the interrupt usable long-term, though:
     * Sn_IR[RECV] latches until software clears it (see the matching
     * setSn_IR() call in w5500_hardened_recv() below) — without that, INTn
     * would assert once on the first byte ever received and then stay low
     * forever, since nothing re-arms it. */
    setSIMR(0xFF);
    for (uint8_t s = 0; s < 8; s++) {
        setSn_IMR(s, Sn_IR_RECV | Sn_IR_DISCON | Sn_IR_CON | Sn_IR_TIMEOUT);
    }

    return 0;
}

/* STM32G071's 96-bit factory unique ID, per RM0444. Three consecutive 32-bit
 * words starting at this fixed address. */
#define STM32_UID_BASE 0x1FFF7590UL

void w5500_get_mac(uint8_t mac[6])
{
    const uint32_t *uid = (const uint32_t *)STM32_UID_BASE;
    /* XOR all 96 bits down to 32 so every one of the UID's bytes influences
     * the MAC, then use the low 24 bits as the device-unique part. Good
     * enough to avoid collisions across a handful of boards on one bench;
     * not a cryptographic hash. */
    uint32_t combined = uid[0] ^ uid[1] ^ uid[2];
    mac[0] = 0x02;
    mac[1] = 0x08;
    mac[2] = 0xDC;
    mac[3] = (uint8_t)(combined >> 16);
    mac[4] = (uint8_t)(combined >> 8);
    mac[5] = (uint8_t)(combined);
}

drv_status_t w5500_selftest(w5500_selftest_t *r)
{
    r->version = getVERSIONR();

    uint8_t mac_w[6];
    uint8_t mac_r[6] = {0};
    w5500_get_mac(mac_w);
    setSHAR(mac_w);
    getSHAR(mac_r);
    r->rw_ok = (memcmp(mac_w, mac_r, 6) == 0) ? 1 : 0;
    r->link_up = (getPHYCFGR() & PHYCFGR_LNK_ON) ? 1 : 0;

    return (r->version == 0x04 && r->rw_ok) ? DRV_OK : DRV_ERR_SPI;
}

int w5500_link_up(void)
{
    return (getPHYCFGR() & PHYCFGR_LNK_ON) ? 1 : 0;
}

void w5500_set_static_ip(const uint8_t ip[4], const uint8_t sn[4], const uint8_t gw[4])
{
    wiz_NetInfo net = {0};
    w5500_get_mac(net.mac);
    memcpy(net.ip, ip, 4);
    memcpy(net.sn, sn, 4);
    memcpy(net.gw, gw, 4);
    net.dhcp = NETINFO_STATIC;
    wizchip_setnetinfo(&net);
}

#define SR_VOTE_SAMPLES 5
#define SR_VOTE_MIN_AGREE 3
#define SN_CR_WAIT_MAX 10000

int32_t w5500_hardened_recv(uint8_t sn, uint8_t *buf, uint16_t cap)
{
    int agree = 0;
    for (int i = 0; i < SR_VOTE_SAMPLES; i++) {
        uint8_t status = getSn_SR(sn);
        if (status == SOCK_ESTABLISHED || status == SOCK_CLOSE_WAIT) {
            agree++;
        }
        spi_bus_settle();
    }
    if (agree < SR_VOTE_MIN_AGREE) {
        return -1; /* majority says the socket is gone; let the caller handle teardown */
    }

    uint16_t rsr1 = getSn_RX_RSR(sn);
    spi_bus_settle();
    uint16_t rsr2 = getSn_RX_RSR(sn);
    uint16_t rsr;
    if (rsr1 == rsr2) {
        rsr = rsr1;
    } else {
        spi_bus_settle();
        uint16_t rsr3 = getSn_RX_RSR(sn);
        if (rsr3 == rsr1) {
            rsr = rsr1;
        } else if (rsr3 == rsr2) {
            rsr = rsr2;
        } else {
            return 0; /* three disagreeing reads — be conservative, try again next poll */
        }
    }

    if (rsr == 0) {
        return 0;
    }

    uint16_t len = (rsr < cap) ? rsr : cap;
    wiz_recv_data(sn, buf, len);
    setSn_CR(sn, Sn_CR_RECV);

    int cr_wait = 0;
    while (getSn_CR(sn)) {
        if (++cr_wait >= SN_CR_WAIT_MAX) {
            /* Chip never cleared Sn_CR — same SPI misread class this file
             * already hardens recv() against elsewhere. This used to spin
             * NetTask forever, wedging the whole board until a physical
             * reset; bail out instead and let the caller retry next poll. */
            return -1;
        }
        spi_bus_settle();
    }

    /* Per the W5500 datasheet, Sn_IR[RECV] stays set until software clears
     * it by writing 1 — it is NOT cleared by reading the data out above.
     * Left alone, INTn would assert low on the first byte this socket ever
     * receives and then stay low permanently (every later RECV event ORs
     * into the same already-set bit, so no new falling edge is ever
     * produced), meaning the EXTI wake in app_tasks.c would fire exactly
     * once per socket, ever. Clearing it here re-arms the pin so each
     * subsequent SCPI command's data also gets a real wake instead of
     * silently falling back to the 2ms poll after the very first one. */
    setSn_IR(sn, Sn_IR_RECV);

    return (int32_t)len;
}

static w5500_int_cb_t s_int_cb = 0;

void w5500_port_set_int_callback(w5500_int_cb_t cb)
{
    s_int_cb = cb;
}

/* W5500_INT_N_Pin (PA9 / Nucleo D8) is falling-edge EXTI, shared EXTI4_15_IRQHandler ->
 * HAL_GPIO_EXTI_IRQHandler -> this weak HAL callback. */
void HAL_GPIO_EXTI_Callback(uint16_t GPIO_Pin)
{
    if (GPIO_Pin == W5500_INT_N_Pin && s_int_cb != 0) {
        s_int_cb();
    }
}
