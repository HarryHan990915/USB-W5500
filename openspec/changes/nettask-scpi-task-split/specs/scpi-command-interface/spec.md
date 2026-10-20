## MODIFIED Requirements

### Requirement: SCPI TCP server
The device SHALL listen on TCP port 5025 and accept line-terminated SCPI commands. `NetTask` SHALL own the TCP socket (accept/read/write bytes only); parsing with `scpi-parser` and dispatching to the appropriate service-layer function SHALL happen on `ScpiShortTask`, which receives raw bytes from `NetTask` via `scpi_rx_queue` and returns reply bytes via `scpi_tx_queue`. The connection SHALL remain reachable by standard SCPI clients (e.g. `pyvisa` via `TCPIP::<ip>::5025::SOCKET`) with identical wire behavior to before this split.

#### Scenario: pyvisa client queries identity
- **WHEN** a `pyvisa` client connects to `TCPIP::<ip>::5025::SOCKET` and sends `*IDN?`
- **THEN** the device replies with `Company,CableTester,SN001,FW1.0`, produced by `ScpiShortTask` and transmitted by `NetTask`

#### Scenario: Short command executes entirely on ScpiShortTask
- **WHEN** a client sends a short/quick command (e.g. `*IDN?`, `TEST:LED:STATus?`, `SYSTem:ERRor?`)
- **THEN** `ScpiShortTask` parses the command, runs its callback, and produces the reply, without `NetTask` ever invoking `SCPI_Input()` or any command callback

### Requirement: Long-running commands do not block command processing
Long-running commands (`TEST:LED:BLINk`, `TEST:LED:ABORt`) SHALL be forwarded by `ScpiShortTask` to `ScpiLongTask` (the task formerly named `ScpiTask`) via the existing `led_bench` request queue, unchanged from prior behavior, so that neither `NetTask` nor `ScpiShortTask` blocks while a long-running command executes.

#### Scenario: Blink command does not block subsequent short commands
- **WHEN** `TEST:LED:BLINk 500` is sent and is still running on `ScpiLongTask`
- **THEN** `ScpiShortTask` can still parse and reply to a subsequent `TEST:LED:STATus?` or `*IDN?` without waiting for the blink sequence to finish

#### Scenario: Abort command reaches ScpiLongTask promptly
- **WHEN** `TEST:LED:ABORt` is sent while `ScpiLongTask` is mid-sequence on a prior `TEST:LED:BLINk`
- **THEN** `ScpiShortTask` forwards the abort request via the existing `led_bench` request queue and `ScpiLongTask` observes it on its next check, same as before this split
