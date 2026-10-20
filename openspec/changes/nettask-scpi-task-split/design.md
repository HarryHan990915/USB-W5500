## Context

`NetTask` (`L3_App/app_tasks.c:app_net_task`) currently runs a loop that: (1) waits on a thread flag with a periodic fallback timeout, (2) calls `net_service_poll()` for link/DHCP state, and (3) calls `scpi_server_poll()`, which directly drives the SCPI TCP socket *and* calls `SCPI_Input()` inline — meaning SCPI command parsing and every short command's callback execute on `NetTask` itself. `NetTask` is the only task permitted to touch the W5500 over SPI (established and hardened over the course of several interrupt/race-condition fixes already applied to this codebase — see `w5500_hardened_recv()`, the CON/DISCON interrupt-latch fix, and the SIMR/Sn_IMR re-arm fix).

The project plans to add HTTP and ping support, both of which would also need socket I/O through `NetTask`. Without this change, each new protocol would mean adding more protocol-specific parsing logic directly into `NetTask`'s loop, which defeats the purpose of having a single SPI-owning transport task.

A second, pre-existing task currently named `ScpiTask`/`app_scpi_task` does not actually touch SCPI parsing at all — it only runs `led_bench_run()`, the blocking loop behind `TEST:LED:BLINk`/`ABORt`. This name has been a source of confusion in this exact conversation and is being corrected as part of this change.

## Goals / Non-Goals

**Goals:**
- `NetTask` ends this change with zero dependency on `scpi-parser` or any SCPI command semantics — it only moves bytes.
- SCPI parsing and short-command execution move to a new `ScpiShortTask`.
- The task formerly called `ScpiTask`/`app_scpi_task` is renamed `ScpiLongTask`/`app_scpi_long_task` to reflect what it has always actually done.
- Establish a reusable NetTask ↔ protocol-task queue pattern (`<proto>_rx_queue` / `<proto>_tx_queue`) that HTTP and ping can adopt later without further NetTask changes.
- No change to SCPI wire behavior: command set, reply formats, and timing characteristics (aside from the added queue hand-off latency) are unchanged.

**Non-Goals:**
- Not implementing HTTP or ping in this change — only establishing the pattern SCPI now follows, which they're expected to follow later.
- Not changing the W5500 interrupt architecture, DHCP/link state machine, or any of the already-applied SPI reliability hardening (`w5500_hardened_recv()`, CON/DISCON IR clearing, SIMR/Sn_IMR re-arm). Those stay exactly as they are; this change only relocates *where* received SCPI bytes get interpreted.
- Not adding multi-client SCPI support (`SCPI_SLOTS` stays 1) — out of scope, unrelated to this split.

## Decisions

### Decision: Byte-level queues, not command-object queues
`scpi_rx_queue` carries raw byte chunks (as read off the socket by `w5500_hardened_recv()`), not parsed command objects. `ScpiShortTask` owns the `scpi_t` context and calls `SCPI_Input()` itself.

**Alternative considered**: Have `NetTask` do minimal line-splitting (buffer until `\n`) and queue whole command lines. Rejected — this would require `NetTask` to understand SCPI's line-termination convention, which is exactly the kind of protocol-specific knowledge this change is trying to remove from `NetTask`. Raw bytes keep `NetTask` protocol-agnostic; `scpi-parser`'s `SCPI_Input()` already handles partial/multi-command buffering internally on the `ScpiShortTask` side.

### Decision: Reply bytes go back through a queue, not a direct `send()` from ScpiShortTask
`scpi_write_cb()` (the function `scpi-parser` calls to emit output) pushes bytes onto `scpi_tx_queue` instead of calling ioLibrary's `send()`.

**Alternative considered**: Let `ScpiShortTask` call `send()` directly, protecting the shared SPI bus with a mutex shared with `NetTask`. Rejected for now — mutex-protected concurrent SPI access from two tasks reintroduces exactly the class of race condition this codebase just spent considerable effort debugging and fixing (CON/DISCON interrupt latching, `Sn_IR_SENDOK` state corruption in `send()`'s internal `sock_is_sending` bookkeeping). Keeping `NetTask` as the sole SPI owner — even at the cost of one extra queue hop per reply — avoids reopening that entire class of bug for this change. If profiling later shows the queue hop is a real bottleneck, the mutex approach can be revisited as a separate, deliberately-scoped change.

### Decision: NetTask wakes immediately when a reply is queued, not just on its periodic timeout
`ScpiShortTask` signals `NetTask` (the same thread-flag mechanism already used for the W5500 EXTI wake) after pushing to `scpi_tx_queue`, so replies don't wait out NetTask's periodic link-check timeout (currently 5 s).

**Alternative considered**: Let `NetTask`'s existing periodic wake pick up `scpi_tx_queue` opportunistically. Rejected — a 5 s reply latency for every SCPI command would be a severe, user-visible regression (confirmed firsthand in this project's own testing history, where exactly this kind of delay was mistaken for a connectivity bug). Waking NetTask explicitly keeps reply latency close to what it is today.

### Decision: Rename `ScpiTask`/`app_scpi_task` → `ScpiLongTask`/`app_scpi_long_task`
Purely a naming fix matching actual responsibility (runs `led_bench_run()`, nothing SCPI-parsing-related). No behavior change.

## Risks / Trade-offs

- **[Risk] Added latency per SCPI command** (NetTask → queue → ScpiShortTask → queue → NetTask, two extra context switches vs. today's single inline call) → **Mitigation**: thread-flag-based immediate wake on both ends keeps the added delay to roughly the cost of two RTOS context switches (low microseconds range on this Cortex-M0+), negligible next to the already-documented SPI bus latency in this codebase.
- **[Risk] Queue sizing**: too small and a burst of short commands could stall `NetTask` on a full `scpi_rx_queue`, or stall `ScpiShortTask` on a full `scpi_tx_queue` → **Mitigation**: size both queues generously relative to `SCPI_RECV_BUF_LEN`/reply sizes (a handful of entries each) and treat a full queue as backpressure (`NetTask` simply doesn't drain the socket further that tick) rather than a dropped-byte condition — this matches the socket's own natural backpressure (data just waits in the W5500's RX FIFO).
- **[Risk] Reintroducing an SPI-ownership violation by accident** (e.g. a future contributor adds a direct ioLibrary call inside `ScpiShortTask` for convenience) → **Mitigation**: the existing `tools/check_layering.sh` pattern (which already blocks `L3_App` from including driver-layer headers) should be extended to also block `scpi_short_task`'s translation unit from including W5500/ioLibrary headers, making the violation a build-time failure, not a code-review-only convention.
- **[Trade-off] More moving parts** (2 new queues, 1 new task) for a system that has already proven fragile to get right (interrupt latching, SPI noise, `sock_is_sending` corruption) → accepted deliberately because the alternative (stacking HTTP/ping parsing into NetTask) makes the already-complex NetTask loop worse over time, not better.

## Migration Plan

1. Rename `app_scpi_task`/`ScpiTaskHandle`/CMSIS thread def to `app_scpi_long_task`/`ScpiLongTaskHandle` — no behavior change, safe to land and flash-test independently first.
2. Add `scpi_rx_queue`/`scpi_tx_queue` creation in `main.c` and the new `ScpiShortTask` thread, initially a no-op consumer, to validate task creation/stack sizing in isolation before wiring real traffic through it.
3. Move `scpi_t` context ownership, `SCPI_Input()` call, and command dispatch table from `scpi_server.c`'s `slot_poll()` into the new `ScpiShortTask` loop; change `scpi_server.c`'s `slot_poll()` (now NetTask-side only) to push received bytes to `scpi_rx_queue` and drain `scpi_tx_queue` for `send()`.
4. Update `scpi_write_cb()` to push to `scpi_tx_queue` instead of calling `send()`.
5. Flash and re-run the existing manual test flow (`manual_scpi_test.py`, interactive blink/abort testing) to confirm identical client-visible behavior before considering this change complete.

Rollback: revert the commit(s) for this change — there is no persisted state format change (flash config, DHCP, etc. untouched), so rollback is a plain code revert with no data migration concerns.

## Open Questions

- Should `scpi_tx_queue` carry whole-reply buffers (one queue entry per completed command reply) or raw byte chunks matching `scpi_rx_queue`'s symmetry? Leaning toward whole-reply buffers sized to `SCPI_RECV_BUF_LEN`/similar, since replies are generated in one shot by `scpi_write_cb()` and chunking them back apart on the NetTask side would add complexity with no benefit — confirm during implementation.
- Exact queue depths for `scpi_rx_queue`/`scpi_tx_queue` — start with a small fixed depth (e.g. 4) and revisit based on observed `STACK`/queue-full diagnostics if real usage demands more.
