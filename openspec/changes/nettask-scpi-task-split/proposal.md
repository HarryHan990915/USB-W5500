## Why

`NetTask` currently does three unrelated jobs in one loop: link/DHCP state management, all W5500 SPI/socket I/O, and SCPI protocol parsing + short-command execution (via `scpi_server_poll()` calling `SCPI_Input()` inline). This worked while SCPI was the only protocol, but the project is about to add HTTP and ping support to the same task, which would mean stacking more protocol-specific parsing logic into what should be a protocol-agnostic transport loop. Splitting SCPI parsing out now, behind a queue-based interface, establishes the pattern (NetTask owns SPI and moves raw bytes; a dedicated task owns one protocol's parsing) that HTTP and ping can both follow later without ever touching NetTask's core loop again.

## What Changes

- `NetTask` becomes a pure network/SPI transport layer: link detection, DHCP/static-IP state machine (unchanged), and raw socket I/O (accept/read/write bytes) for every protocol socket it owns. It no longer calls `SCPI_Input()` or knows anything about SCPI syntax.
- New `ScpiShortTask`: owns the `scpi_t` parser context and the command dispatch table. Consumes raw inbound bytes from a new `scpi_rx_queue` (filled by NetTask), calls `SCPI_Input()`, and executes short/quick command callbacks directly on this task.
- Reply bytes produced by the SCPI write callback go into a new `scpi_tx_queue` instead of calling `send()` directly — NetTask is the only task that calls `send()`/touches the W5500 SPI bus, so it drains `scpi_tx_queue` and transmits on ScpiShortTask's behalf.
- Long-running commands (`TEST:LED:BLINk`, `TEST:LED:ABORt`) are still forwarded from ScpiShortTask to the LED-blink task via the existing `led_bench` request queue — unchanged.
- **BREAKING (internal only, no SCPI-visible behavior change)**: the task currently named `app_scpi_task` (CMSIS thread "ScpiTask") is renamed to `ScpiLongTask` (`app_scpi_long_task`) since it never parsed SCPI — it only ever ran `led_bench_run()`. The name `ScpiTask`/`app_scpi_task` is now free and intentionally not reused, to avoid confusion with the new `ScpiShortTask`.

## Capabilities

### New Capabilities

(none — this is an internal re-architecture of existing task responsibilities, not a new user-facing capability)

### Modified Capabilities

- `rtos-task-model`: task topology changes from {NetTask, ScpiTask, HousekeepTask} to {NetTask, ScpiShortTask, ScpiLongTask, HousekeepTask}; adds two message queues (`scpi_rx_queue`, `scpi_tx_queue`) as the NetTask ↔ ScpiShortTask boundary; documents that NetTask remains the sole owner of W5500 SPI/socket access.
- `scpi-command-interface`: the SCPI parser and command callbacks now execute on `ScpiShortTask` instead of `NetTask`; NetTask's role is reduced to moving raw bytes in/out over the new queues. Command behavior and replies are unchanged from the client's perspective; only the execution context moves.

## Impact

- Affected code: `L3_App/app_tasks.c` (task functions, CMSIS thread defs), `L2_Service/scpi_server.c` (split into NetTask-side raw I/O vs. ScpiShortTask-side parsing), `Core/Src/main.c` (new thread + 2 new `osMessageQueueId_t` creation), possibly a new `L2_Service/scpi_short_task.c` or similar to host the parser-side logic.
- No change to the SCPI command set, wire protocol, or client-visible behavior (`*IDN?`, `TEST:LED:*`, etc. all behave identically).
- Slightly increased per-command latency (one extra queue hand-off + task switch each way) in exchange for NetTask no longer depending on `scpi-parser` at all — sets up HTTP/ping to be added as additional protocol tasks without modifying NetTask's core loop.
