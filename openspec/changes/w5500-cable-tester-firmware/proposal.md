## Why

We need firmware for a NUCLEO-G071RB + W5500 network cable tester that exposes a standard SCPI-over-TCP interface (port 5025), so the device can be controlled and diagnosed remotely by any SCPI client (including AI agents via pyvisa/MCP) without needing test-fixture hardware to exist yet. The design is fully specified in `STM32 + W5500 网络线缆测试仪固件设计.pdf` (2026-10-14, @harry); this change turns that spec into a buildable, layered CubeIDE project and an ordered implementation plan. The project folder currently contains no firmware code — this is the initial implementation.

## What Changes

- New STM32CubeIDE project for NUCLEO-G071RB: SPI1+DMA to W5500 (PA5/PA6/PA7 = D13/D12/D11), CS=PB0 (D10), RST=PC7 (D9), INT=PA9 EXTI (D8) — confirmed against the physical board wiring and the official Zephyr `nucleo_g071rb` board file, superseding the doc's PB1/PB2 placeholders — USART2 (PA2/PA3) via ST-LINK VCP, TIM2 as a free-running 1 MHz µs timebase (M0+ has no DWT/CYCCNT), TIM6 as the FreeRTOS SysTick source (added at the milestone-8 RTOS migration), NVIC limited to 4 priority levels (no BASEPRI).
- Compiler-enforced 4-layer architecture (`Core/`, `Drivers/` generated-only; `ThirdParty/{ioLibrary_Driver, scpi-parser}` vendored; `L1_Driver/` → `L2_Service/` → `L3_App/`, each restricted to including only the layer directly below via per-folder CubeIDE include paths).
- W5500 driver (`w5500_port`) wired to WIZnet ioLibrary callbacks, with a self-test (VERSIONR, SHAR write/read-back, PHY link) consumed by `diag_service`.
- Network service (`net_service`) state machine: WAIT_LINK → DHCP_RUNNING → BOUND/STATIC_FALLBACK → SERVING, with static-IP fallback (192.168.1.200/24) persisted in flash.
- SCPI server on TCP 5025 (`scpi_server` + `scpi-parser`) implementing the full command table from the spec (`*IDN?`, `*RST`, `*TST?`, `SYSTem:*`, `DIAGnostic:*`, `CONFigure:*`, `MEASure:CONTinuity?`, `FETCh:RESult?`), mirrored on a USART2 CLI for when the network is down.
- Phase-1 `TEST:*` string command set (`string_service`, pure C, no HAL) to validate the TCP/SCPI pipeline end-to-end before tester-fixture hardware exists, including the two defined SCPI error codes (-109 missing parameter, -223 too much data).
- `tester_io_sim` (simulated 8-wire straight-through cable) behind a `TESTER_IO_SIM=1` build flag, swapped for real GPIO/ADC `tester_io` later without touching service/app code.
- `flash_store`/`config_service`: config persisted in the last 2 KB flash page (0x0801F800) with magic + CRC32, defaults on corruption.
- Migration from a bare-metal bring-up loop to 4 FreeRTOS tasks (`MeasureTask`, `NetTask`, `ScpiTask`, `HousekeepTask`), interrupt/notification-driven (W5500 INT → EXTI → task notify, SPI bursts over DMA), with TIM2-based latency instrumentation (`DIAGnostic:LATency?`).

## Capabilities

### New Capabilities
- `firmware-project-structure`: CubeIDE project scaffold, CubeMX peripheral config, vendored third-party libs, and the compiler-enforced L1/L2/L3 layering rule (including the CI include-check).
- `w5500-driver`: SPI1+DMA-backed W5500 port driver, HW reset sequencing, and the driver self-test used by diagnostics.
- `network-service`: link/DHCP/static-IP state machine and TCP/SCPI socket lifecycle management.
- `scpi-command-interface`: the full SCPI command table (system/diagnostic/config/measure) plus the USART2 CLI that mirrors it, and the phase-1 `TEST:*` string validation commands with their error-queue behavior.
- `tester-io-measurement`: simulated and (future) real tester I/O, continuity test execution, and result retrieval (`MEASure:CONTinuity?` / `FETCh:RESult?`).
- `config-persistence`: flash-backed configuration storage with CRC validation and default fallback.
- `rtos-task-model`: the 4-task FreeRTOS architecture, ISR/DMA/notification wiring, and latency measurement.

### Modified Capabilities
_None — `openspec/specs/` is currently empty; this is the initial feature set._

## Impact

- **New code**: entire `scpi_G071RB` CubeIDE project (currently just `.claude/` + `openspec/` scaffolding) — `Core/`, `Drivers/` (CubeMX-generated), `ThirdParty/ioLibrary_Driver`, `ThirdParty/scpi-parser`, `L1_Driver/`, `L2_Service/`, `L3_App/`.
- **Reference only, not copied verbatim**: `C:\Users\HarryHan\Documents\USB_MSC_H735` is an existing working STM32CubeIDE project (H735/Cortex-M7) on this machine — used only for its generic CubeIDE project-layout and build-settings conventions, not for anything H7-specific.
- **Dependencies**: WIZnet `ioLibrary_Driver` and `j123b567/scpi-parser` (both vendored as open-source third-party code), STM32CubeMX code generation, STM32CubeIDE build tooling.
- **Hardware**: NUCLEO-G071RB + W5500 breakout wired via jump wires per the pin table above; no cable-tester fixture hardware exists yet (simulated via `tester_io_sim`).
- **Near-term milestone** (tracked first in tasks.md): a clean 0-error/0-warning CubeIDE build of the scaffold + W5500 driver + minimal bare-metal self-test-over-UART `main`, before the later networking/SCPI/RTOS milestones.
- **Open questions** (do not block initial build): tested-cable pin count/connector type and whether resistance measurement (needs ADC) is required; whether an IP allowlist/password/`SYSTem:LOCK` is needed; whether mDNS/UDP-broadcast device discovery is needed.
