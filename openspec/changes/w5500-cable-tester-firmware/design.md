## Context

`scpi_G071RB` is currently an empty project (only `.claude/` + `openspec/` scaffolding). The target hardware is a NUCLEO-G071RB (Cortex-M0+, 64 MHz, 128 KB flash, 36 KB RAM) wired by jump wires to a W5500 SPI-to-Ethernet breakout. The full design is specified in `STM32 + W5500 网络线缆测试仪固件设计.pdf` (2026-10-14). Cable-tester fixture hardware does not exist yet, so the test-pin I/O layer must be simulated behind a swappable interface. A separate, unrelated STM32CubeIDE project on this machine, `C:\Users\HarryHan\Documents\USB_MSC_H735` (H735/Cortex-M7), is available as a reference for generic CubeIDE project-layout and build-setting conventions only.

The G071RB's Cortex-M0+ core lacks BASEPRI and DWT/CYCCNT, and NVIC only has 4 priority levels — this constrains the FreeRTOS critical-section and timing design (see Decisions).

## Goals / Non-Goals

**Goals:**
- A CubeIDE project that builds cleanly (0 errors/0 warnings) as the first checkpoint, with the layered folder structure and include-path enforcement in place from the start.
- A W5500 driver whose self-test can be exercised and observed over the USART2/ST-LINK VCP log before any networking or RTOS code exists.
- Each subsequent bring-up milestone (link detection → static IP → DHCP → TCP echo → SCPI → FreeRTOS migration → simulated tester I/O → flash config + full self-test) independently buildable and verifiable in order.
- A SCPI/TCP surface that any pyvisa client (including AI agents) can drive once the relevant milestone lands.

**Non-Goals:**
- Real cable-tester fixture hardware/wiring — `tester_io_sim` stands in until hardware exists; swapping in real `tester_io` is out of scope for this change.
- Resistance measurement via ADC, IP allowlisting/`SYSTem:LOCK`, and mDNS/UDP discovery — open questions from the source doc, deferred until answered.
- Porting anything H7-specific from `USB_MSC_H735` — only its generic CubeIDE project conventions are reused.

## Decisions

- **Strict layering enforced via CubeIDE per-folder include paths, not just convention.** `L3_App/` gets an include path that only reaches `L2_Service/` headers, `L2_Service/` only reaches `L1_Driver/`, etc. Alternative considered: a single flat `Core/Src` with naming conventions — rejected because it relies on discipline instead of the compiler, and the source doc explicitly calls for a CI grep check on top of this.
- **TIM2 (32-bit, free-running @ 1 MHz) replaces DWT/CYCCNT for µs timing**, since Cortex-M0+ has no cycle counter. Used both for driver-level delay timing and for `DIAGnostic:LATency?` instrumentation in `ScpiTask`.
- **W5500 is owned exclusively by `NetTask`**; no mutex-guarded sharing across tasks. Rationale: WIZnet ioLibrary is not thread-safe, and mutex arbitration on an M0+ (4 priority levels, no BASEPRI, FreeRTOS critical sections mask all interrupts) risks priority inversion and long critical sections. All other tasks reach the W5500 only indirectly via service-layer queues/notifications into NetTask.
- **Interrupt/notification-driven tasks, not polling.** W5500 INT (EXTI falling edge) → `vTaskNotifyGiveFromISR` wakes `NetTask`; SPI bursts run over DMA with the requesting task blocked on `ulTaskNotifyTake` rather than busy-waiting. Keeps ISRs and critical sections short, which matters more on M0+ since critical sections disable all interrupts globally.
- **`tester_io_sim` behind a `TESTER_IO_SIM=1` compile-time flag, same header as the real driver.** Lets `tester_service`/`MEASure:CONTinuity?`/`FETCh:RESult?` be built and tested end-to-end now, and is the concrete proof that the L1/L2/L3 layering actually isolates hardware changes (swapping this one file requires no service/app changes later).
- **Config persisted in the last 2 KB flash page (0x0801F800) with magic + CRC32, 64-bit double-word writes.** Matches the G071's 2 KB page erase granularity and double-word program requirement; corruption falls back to compiled-in defaults rather than failing to boot.
- **Bring-up proceeds bare-metal first, FreeRTOS migration is milestone 8, not milestone 1.** Each early milestone (SPI self-test, link detection, static IP, DHCP, TCP echo, SCPI+pytest) is independently checkable without RTOS complexity in the loop; the RTOS/task-split lands once the single-threaded behavior is already proven, reducing the surface being debugged at once.
- **Phase-1 `TEST:*` string commands validate the full SCPI/TCP pipeline before any tester hardware or even the real measurement logic exists.** `string_service` is plain C with no HAL dependency, so it's also unit-testable on a PC — this is the spec's intended way to decouple "is the network/SCPI stack correct" from "is the test hardware correct."

## Risks / Trade-offs

- [36 KB total RAM is tight for FreeRTOS heap (~12 KB) + ioLibrary socket buffers (8 × up to 4 KB code-configured, spec suggests 4/4/2/2/1/1/1/1 KB split) + SCPI buffers] → Size task stacks from the spec's starting points (512 B–1.5 KB) and verify with `uxTaskGetStackHighWaterMark()` during bring-up milestone 8 before adding more features; tighten/loosen based on measured water marks, not guesses.
- [NVIC only has 4 priority levels and no BASEPRI, so FreeRTOS critical sections mask ALL interrupts globally, including ones unrelated to the critical section] → Keep ISRs and critical sections minimal, push real work into tasks (already a Decision above), and treat any observed latency spikes in `DIAGnostic:LATency?` as a signal to re-audit critical-section length.
- [Per-folder include-path enforcement in CubeIDE is a project-settings concern, not source code — easy to silently regress if someone edits `.cproject` or adds a file] → Add the CI grep check (from the source doc) for `#include "w5500` / `stm32xx_hal` inside `L3_App/` as a cheap backstop, not just rely on IDE settings.
- [WIZnet ioLibrary + scpi-parser are vendored third-party code, not written by us] → Vendor exact known-good versions into `ThirdParty/`, do not modify upstream source in place (wrap/extend instead), so updates stay tractable.
- [No tester-fixture hardware exists, so `MEASure:CONTinuity?` can only be validated against the simulated straight-through cable] → Explicitly scope "tester IO + MEASure validated against simulation only" as this change's definition of done for that capability; real-hardware validation is future work, tracked as an open question rather than assumed complete.

## Migration Plan

Sequential bring-up, each step independently buildable/flashable/observable before moving to the next (full detail in `tasks.md`):

1. CubeMX project + CubeIDE layered folder structure + vendored `ThirdParty/` libs → clean build.
2. `w5500_port` + self-test logged over USART2 → observe `VERSIONR == 0x04` on real hardware.
3. Link detection (`PHYCFGR` bit) once a cable is connected.
4. Static IP bring-up → verify with `ping`.
5. DHCP bring-up → UART prints leased IP.
6. TCP 5025 raw echo → verify with `nc`.
7. `scpi-parser` + phase-1 `TEST:*` commands → verify with the `pytest`+`pyvisa` suite from the source doc.
8. FreeRTOS migration (4 tasks, INT/EXTI wakeup, SPI DMA) → re-verify milestones 2–7 still pass under RTOS.
9. `tester_io_sim` wired to `tester_service`/`MEASure:CONTinuity?`/`FETCh:RESult?` → verify against the simulated straight-through cable.
10. `flash_store`/`config_service` persistence + full `*TST?` self-test + full `pytest` suite passing.

No rollback strategy needed beyond normal git revert — this is greenfield firmware with no existing deployed version to migrate from.

## Open Questions

- Tested-cable pin count and connector type, and whether resistance measurement (would require an ADC channel) is in scope.
- Whether an IP allowlist and/or `SYSTem:LOCK` is needed given any LAN client can currently control the device.
- Whether device discovery (mDNS or UDP broadcast) is needed to find the device on the LAN.
