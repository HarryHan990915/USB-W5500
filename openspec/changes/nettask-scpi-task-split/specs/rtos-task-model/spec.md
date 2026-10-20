## MODIFIED Requirements

### Requirement: Four-task architecture with fixed ownership
The firmware SHALL run the following FreeRTOS tasks: `MeasureTask` (Realtime priority, queue-triggered, timing-critical measurement execution, 512 B initial stack), `NetTask` (High priority, sole owner of the W5500/SPI and all socket I/O, woken by W5500 INT via EXTI, 1 KB initial stack), `ScpiShortTask` (AboveNormal priority, owns the `scpi_t` parser context and the command dispatch table, woken when raw bytes are queued from NetTask, executes short/quick command callbacks directly), `ScpiLongTask` (Normal priority, executes long-running commands — e.g. `TEST:LED:BLINk` — forwarded to it by `ScpiShortTask`; this is the task formerly named `ScpiTask`/`app_scpi_task`, renamed because it never parsed SCPI), and `HousekeepTask` (Low priority, 1-second period: watchdog, flash save, stack high-water-mark logging, async log draining, 512 B initial stack). `NetTask` SHALL be the only task that accesses W5500/ioLibrary functions; `ScpiShortTask` and `ScpiLongTask` SHALL NOT call ioLibrary or `w5500_port` functions directly.

#### Scenario: Only NetTask touches the W5500
- **WHEN** any task other than `NetTask` needs network or socket state
- **THEN** it obtains that state via a service-layer call that internally routes through `NetTask` (queue/notification), never by calling ioLibrary or `w5500_port` functions directly

#### Scenario: ScpiShortTask never performs SPI transactions
- **WHEN** `ScpiShortTask` parses a command or produces a reply
- **THEN** it reads inbound bytes from `scpi_rx_queue` and writes reply bytes to `scpi_tx_queue` only — it never calls `send()`, `recv()`, or any W5500 register function directly

### Requirement: Logging does not block hot paths
Hot paths (ISRs, `NetTask`'s SPI-active sections, `ScpiShortTask`'s command-processing path) SHALL NOT call blocking/formatting I/O (e.g. `printf` to UART) directly; log messages SHALL be queued and drained asynchronously by `HousekeepTask`.

#### Scenario: Logging during SCPI command processing does not add UART-transmit latency to the reply
- **WHEN** `ScpiShortTask` logs a message while processing a SCPI command
- **THEN** the log message is enqueued and the SCPI reply is sent without waiting for the UART transmission of that log message to complete

### Requirement: Latency instrumentation
`ScpiShortTask` SHALL capture a timestamp at command-line receipt and another at reply hand-off to `scpi_tx_queue`; the difference SHALL be exposed in microseconds via `DIAGnostic:LATency?`.

#### Scenario: Latency value reflects actual processing time
- **WHEN** a command is received, processed, and replied to
- **THEN** `DIAGnostic:LATency?` subsequently reports a microsecond value consistent with the timestamp delta between those two points for that command, for that command's parse+execute duration (independent of how long the reply bytes subsequently waited in `scpi_tx_queue` before NetTask transmitted them)

## ADDED Requirements

### Requirement: NetTask/ScpiShortTask byte-level queue boundary
Raw bytes read by `NetTask` off a protocol socket SHALL be handed to the owning protocol task via a dedicated inbound message queue, never parsed by `NetTask` itself. Reply bytes produced by a protocol task SHALL be handed back to `NetTask` via a dedicated outbound message queue for transmission; protocol tasks SHALL NOT call `send()` directly. For SCPI, these are `scpi_rx_queue` (NetTask → ScpiShortTask) and `scpi_tx_queue` (ScpiShortTask → NetTask). This boundary SHALL be the pattern future protocol tasks (e.g. HTTP, ping) also use, so NetTask's core loop never needs protocol-specific changes to add a new protocol.

#### Scenario: NetTask forwards inbound SCPI bytes without interpreting them
- **WHEN** NetTask successfully reads bytes off the SCPI TCP socket via `w5500_hardened_recv()`
- **THEN** those bytes are placed on `scpi_rx_queue` unmodified and NetTask takes no SCPI-specific action on their content

#### Scenario: ScpiShortTask's reply is transmitted by NetTask, not by ScpiShortTask
- **WHEN** `ScpiShortTask`'s SCPI write callback produces reply bytes for a connected client
- **THEN** those bytes are placed on `scpi_tx_queue`, and `NetTask` is the task that subsequently calls `send()` to transmit them on the SCPI socket

#### Scenario: NetTask wakes promptly when a reply is queued
- **WHEN** `ScpiShortTask` places reply bytes on `scpi_tx_queue` while `NetTask` is blocked waiting for its next event
- **THEN** `NetTask` is woken to transmit the reply without waiting for its periodic link-check timeout to elapse
