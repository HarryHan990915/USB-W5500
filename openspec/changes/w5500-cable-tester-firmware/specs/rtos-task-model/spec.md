## ADDED Requirements

### Requirement: Four-task architecture with fixed ownership
The firmware SHALL run four FreeRTOS tasks: `MeasureTask` (Realtime priority, queue-triggered, timing-critical measurement execution, 512 B initial stack), `NetTask` (High priority, sole owner of the W5500/SPI, woken by W5500 INT via EXTI and `vTaskNotifyGiveFromISR`, 1 KB initial stack), `ScpiTask` (AboveNormal priority, woken when a full command line is queued, parses SCPI and calls service-layer functions, 1.5 KB initial stack), and `HousekeepTask` (Low priority, 1-second period: watchdog, flash save, stack high-water-mark logging, async log draining, 512 B initial stack). `NetTask` SHALL be the only task that accesses W5500/ioLibrary functions.

#### Scenario: Only NetTask touches the W5500
- **WHEN** any task other than `NetTask` needs network or socket state
- **THEN** it obtains that state via a service-layer call that internally routes through `NetTask` (queue/notification), never by calling ioLibrary or `w5500_port` functions directly

### Requirement: Interrupt and notification driven, not polled
W5500 INT SHALL wake `NetTask` via EXTI falling-edge interrupt and `vTaskNotifyGiveFromISR`; SPI burst transfers SHALL use DMA with the initiating task blocked on `ulTaskNotifyTake()`; inter-task communication SHALL prefer direct-to-task notifications over queues where a 1:1 producer/consumer relationship exists.

#### Scenario: NetTask is idle (not polling) while waiting for W5500 activity
- **WHEN** no W5500 interrupt has fired and no outstanding request is queued to `NetTask`
- **THEN** `NetTask` is blocked (not consuming CPU in a polling loop) until the next notification arrives

### Requirement: Short critical sections and ISRs
Given the Cortex-M0+ has no BASEPRI and FreeRTOS critical sections mask all interrupts globally, ISRs and critical sections SHALL be kept minimal, with substantive work deferred to task context via notifications or queues.

#### Scenario: Heavy processing is deferred out of ISR context
- **WHEN** the W5500 INT ISR fires
- **THEN** the ISR only calls `vTaskNotifyGiveFromISR` (and requests a context switch if needed) and returns; all register reads/socket processing happen afterward in `NetTask`

### Requirement: Logging does not block hot paths
Hot paths (ISRs, `NetTask`'s SPI-active sections, `ScpiTask`'s command-processing path) SHALL NOT call blocking/formatting I/O (e.g. `printf` to UART) directly; log messages SHALL be queued and drained asynchronously by `HousekeepTask`.

#### Scenario: Logging during SCPI command processing does not add UART-transmit latency to the reply
- **WHEN** `ScpiTask` logs a message while processing a SCPI command
- **THEN** the log message is enqueued and the SCPI reply is sent without waiting for the UART transmission of that log message to complete

### Requirement: Latency instrumentation
`ScpiTask` SHALL capture a TIM2 timestamp at command-line receipt and another at reply transmission; the difference SHALL be exposed in microseconds via `DIAGnostic:LATency?`.

#### Scenario: Latency value reflects actual processing time
- **WHEN** a command is received, processed, and replied to
- **THEN** `DIAGnostic:LATency?` subsequently reports a microsecond value consistent with the TIM2 delta between those two timestamps for that command

### Requirement: Stack usage is measured, not assumed
`HousekeepTask` SHALL periodically sample `uxTaskGetStackHighWaterMark()` for all tasks, given the initial stack sizes in this spec are starting points to be tuned against the G071RB's 36 KB total RAM.

#### Scenario: Stack headroom is observable during bring-up
- **WHEN** the system has been running under normal load for some time
- **THEN** each task's minimum observed stack headroom is available via the housekeeping/log output for use in tuning stack sizes
