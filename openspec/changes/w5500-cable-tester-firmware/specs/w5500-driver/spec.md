## ADDED Requirements

### Requirement: W5500 HAL callback wiring
`w5500_port_init()` SHALL register HAL-backed CS, single-byte SPI, and DMA burst callbacks with WIZnet ioLibrary (`reg_wizchip_cs_cbfunc`, `reg_wizchip_spi_cbfunc`, `reg_wizchip_spiburst_cbfunc`) and SHALL call `wizchip_init()` with an 8-socket, 2 KB/2 KB (RX/TX) memory split per socket.

#### Scenario: Driver initializes without error
- **WHEN** `w5500_port_init()` is called once during startup with the W5500 correctly wired
- **THEN** it returns success and the ioLibrary callback table is fully populated (CS, single-byte SPI, burst SPI)

### Requirement: Hardware reset sequencing
`w5500_hw_reset()` SHALL drive the W5500 RST pin low for more than 500 µs, then release it and wait approximately 2 ms before any SPI transaction, using the TIM2-based microsecond timer (not a busy-loop instruction count).

#### Scenario: Reset pulse meets the W5500 minimum reset time
- **WHEN** `w5500_hw_reset()` executes
- **THEN** RST is held low for a duration measured via TIM2 that is greater than 500 µs before being released

### Requirement: Burst transfers use DMA with notification-based waiting
`spi_rburst()`/`spi_wburst()` SHALL perform multi-byte SPI transfers using DMA, with the calling task blocked on `ulTaskNotifyTake()` while the DMA transfer is in flight, rather than polling or busy-waiting.

#### Scenario: Burst transfer does not busy-wait the CPU
- **WHEN** a burst read/write of N bytes (N > 1) is issued
- **THEN** the calling task yields the CPU (blocked on task notification) until the DMA complete interrupt signals completion, rather than spinning on a status flag

### Requirement: Driver self-test
`w5500_selftest()` SHALL read `VERSIONR`, write and read back the `SHAR` (MAC address) register to verify bidirectional SPI, and read the `PHYCFGR` link bit, returning `DRV_OK` only when `VERSIONR == 0x04` and the SHAR read-back matches what was written; `DRV_ERR_SPI` otherwise. This function SHALL be the sole interface `diag_service` uses to assess driver health — no other layer SHALL call W5500 register-access functions directly.

#### Scenario: Self-test passes on correctly wired hardware
- **WHEN** `w5500_selftest()` is called with SPI, CS, and the W5500 chip correctly wired
- **THEN** it reports `VERSIONR == 0x04`, `rw_ok == true`, and returns `DRV_OK`

#### Scenario: Self-test reports failure on a wiring/SPI-mode fault
- **WHEN** `w5500_selftest()` is called with the SPI mode, CS, or wiring incorrect such that `VERSIONR` reads `0x00` or `0xFF`
- **THEN** it returns `DRV_ERR_SPI`
