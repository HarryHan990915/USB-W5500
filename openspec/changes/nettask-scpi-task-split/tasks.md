## 1. Rename ScpiTask to ScpiLongTask (no behavior change)

- [x] 1.1 In `Core/Src/main.c`: rename `ScpiTaskHandle` → `ScpiLongTaskHandle`, `ScpiTask_attributes` → `ScpiLongTask_attributes` (`.name = "ScpiLongTask"`), `StartScpiTask` → `StartScpiLongTask`, and the corresponding `osThreadNew(...)` call/comment. Also updated `.ioc`'s `FREERTOS.Tasks01` to match (priority changed AboveNormal(32)→Normal(24) per design.md) so a future CubeMX regeneration doesn't revert this.
- [x] 1.2 In `L3_App/app_tasks.c`: rename `app_scpi_task` → `app_scpi_long_task`; update the `extern osThreadId_t ScpiTaskHandle` declaration to `ScpiLongTaskHandle`; update `report_stack("Scpi", ...)` call site's label/handle to match (`"ScpiLong"`).
- [x] 1.3 Update `L3_App/app_tasks.h` prototype name. Also swept stale `ScpiTask`/"Called from NetTask" comment references in `led_bench.c/.h` and `scpi_commands.c` (the latter updated again in group 3 once the call site actually moved).
- [x] 1.4 Build verified (clean). **Flash/hardware verification still needed from the user** — no physical board access in this environment.

## 2. Add scpi_rx_queue / scpi_tx_queue and the ScpiShortTask skeleton

- [x] 2.1 Queue item type decided: both `scpi_rx_queue` and `scpi_tx_queue` use the same `scpi_queue_msg_t {uint8_t sock_no; uint16_t len; uint8_t data[SCPI_QUEUE_MSG_LEN]}` (`SCPI_QUEUE_MSG_LEN=300`, matching the codebase's existing SCPI buffer-sizing precedent) — defined in new shared header `L2_Service/scpi_queue.h`.
- [x] 2.2 **Deviation from plan**: queues are created in new `L2_Service/scpi_queue.c`'s `scpi_queue_init()`, called from `app_init()` (not declared/created directly in `main.c`) — matches this codebase's existing precedent of `led_bench_init()` creating its own queues/mutex from `app_init()` rather than centralizing RTOS object creation in `main.c`. Depth 4 each, as planned.
- [x] 2.3 In `Core/Src/main.c`: added `ScpiShortTaskHandle`, `ScpiShortTask_attributes` (`.name = "ScpiShortTask"`, AboveNormal priority), `StartScpiShortTask`, thread creation via `osThreadNew(...)`. Also added to `.ioc`'s `FREERTOS.Tasks01`.
- [x] 2.4 **Deviation from plan**: implemented the real consumer directly (see group 3) rather than landing a no-op skeleton first — collapsed with group 3 since there was no intermediate hardware flash/test checkpoint available in this environment either way.
- [ ] 2.5 Build verified (see 5.1). **Flash/hardware verification still needed from the user.**

## 3. Move SCPI parsing from NetTask to ScpiShortTask

- [x] 3.1 Split into a new `L2_Service/scpi_short_task.c`/`.h` (ScpiShortTask-side: parser ownership) alongside the rewritten `L2_Service/scpi_server.c`/`.h` (NetTask-side: socket lifecycle + queue plumbing only, no more `scpi_server_init()` since there's no per-connection state left on this side).
- [x] 3.2 Moved `scpi_slot_t`, `s_slots`, `s_interface`, `SCPI_Init()` call, and command table wiring into `scpi_short_task.c`'s `scpi_short_task_init()`. `scpi_commands.c` now calls `scpi_short_task_init(...)` instead of the old `scpi_server_init(...)`.
- [x] 3.3 `scpi_write_cb()` now builds a `scpi_queue_msg_t` and pushes it to `scpi_tx_queueHandle` with a bounded 50 ms wait (not instant-drop — see code comment: losing an already-dequeued-from-the-chip reply is worse than a brief wait, and ScpiShortTask/NetTask wake each other immediately so this should never actually block in practice).
- [x] 3.4 Implemented `scpi_short_task_run()`: blocks on `scpi_rx_queueHandle` (`osWaitForever`), calls `SCPI_Input()` per received chunk. `app_scpi_short_task()` in `app_tasks.c` just calls it once (it's self-looping).
- [x] 3.5 Rewrote `scpi_server.c`'s socket handling (`socket_poll()`, renamed from `slot_poll()` since there's no more per-task "slot" concept on this side): `SOCK_CLOSED`/`SOCK_CLOSE_WAIT` unchanged; `SOCK_ESTABLISHED` still calls `w5500_hardened_recv()` then pushes the result to `scpi_rx_queueHandle` (bounded 50 ms wait, same rationale as 3.3 — data is already drained from the chip's RX FIFO by the time we have it). `w5500_clear_socket_con_discon_ir()` call kept unchanged. **Added beyond the original plan**: `SOCK_CLOSED` now also flushes any stale unprocessed `scpi_rx_queue` entries before re-listening, so a new connection never gets fed a dead connection's leftover bytes.
- [x] 3.6 Added `drain_tx_queue()` to `scpi_server.c`, called unconditionally (even when not `net_service_is_serving()`, to flush/discard stale queued replies rather than let them linger for a future unrelated connection) at the top of `scpi_server_poll()`. **Important correctness relocation**: the `send()`-failure recovery (`close(sock_no)` on `sent <= 0`, to un-wedge ioLibrary's internal `sock_is_sending` state — see existing doc comment) moved here too, since `scpi_write_cb()` can no longer touch `close()`/`send()` directly post-split.
- [x] 3.7 Wired via a new `scpi_queue_register_tx_wake()` callback-registration function in `scpi_queue.h/.c` (mirrors the existing `net_service_register_int_wake()` pattern, since `L2_Service` can't depend on `L3_App`'s `app_net_wake_from_isr()` directly). `app_init()` registers `app_net_wake_from_isr` for this too, so `scpi_write_cb()` wakes NetTask immediately after queuing a reply via the same `NET_WAKE_FLAG` used for the EXTI wake.

## 4. Verify layering stays enforced

- [x] 4.1 Confirmed: `scpi_short_task.c` only includes `scpi_queue.h`, `diag_service.h`, `scpi/parser.h`, `scpi/units.h`, `cmsis_os.h` — no W5500/ioLibrary headers.
- [x] 4.2 Extended `tools/check_layering.sh` with a second check specifically blocking `w5500|socket.h|wizchip` includes in `L2_Service/scpi_short_task.*`.
- [x] 4.3 `bash tools/check_layering.sh` passes (both checks).

## 5. Build, flash, and regression-test

- [x] 5.1 Full rebuild from clean: 0 errors, 0 new warnings (only the pre-existing `-Wparentheses` ones in `w5500_port.c`, unrelated to this change). Also manually updated `Debug/L2_Service/subdir.mk` and `Debug/objects.list` to include the two new source files, since this project's generated makefiles list sources explicitly rather than globbing the directory.
- [ ] 5.2 **Needs user hardware verification**: flash and confirm DHCP bind / static-IP fallback still work unchanged.
- [ ] 5.3 **Needs user hardware verification**: run `manual_scpi_test.py <ip>` end-to-end.
- [ ] 5.4 **Needs user hardware verification**: interleave `TEST:LED:BLINk`/`ABORt` with short queries to confirm short commands still respond promptly while a long command runs.
- [x] 5.5 `NET WAKE:`/`RECV STATS:` logging still references valid handles/labels (`ScpiShortTaskHandle`/`ScpiLongTaskHandle` both now reported in housekeeping's stack-usage log).
- [ ] 5.6 **Needs user hardware verification**: re-run the W5500 5 V power-cycle interrupt re-arm check.
