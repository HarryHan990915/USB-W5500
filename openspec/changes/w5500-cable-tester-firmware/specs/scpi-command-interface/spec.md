## ADDED Requirements

### Requirement: SCPI TCP server
The device SHALL listen on TCP port 5025 and accept line-terminated SCPI commands, parsing them with `scpi-parser` and dispatching to the appropriate service-layer function. The connection SHALL be reachable by standard SCPI clients (e.g. `pyvisa` via `TCPIP::<ip>::5025::SOCKET`).

#### Scenario: Raw TCP echo before SCPI parsing is wired in
- **WHEN** a TCP client connects to port 5025 and sends a line of text
- **THEN** the device echoes the line back (bring-up milestone, prior to `scpi-parser` integration)

#### Scenario: pyvisa client queries identity
- **WHEN** a `pyvisa` client connects to `TCPIP::<ip>::5025::SOCKET` and sends `*IDN?`
- **THEN** the device replies with `Company,CableTester,SN001,FW1.0`

### Requirement: Core SCPI command set
The device SHALL implement `*IDN?`, `*RST`, `*TST?` (returns 0 on full self-test pass), `SYSTem:ERRor?` (pops the error queue), `DIAGnostic:DRIVer:SPI?`, `DIAGnostic:NET:LINK?` (1/0), `DIAGnostic:NET:DHCP?` (BOUND/FAILED/STATIC), `DIAGnostic:LATency?` (microseconds), `SYSTem:COMMunicate:LAN:DHCP ON|OFF`, `:ADDRess`/`:SMASk`/`:GATeway`/`:HOSTname`, `:APPLy`, `SYSTem:CONFig:SAVE`, `CONFigure:PINS <n>`, `CONFigure:THReshold:RESistance <ohms>`, `MEASure:CONTinuity?`, and `FETCh:RESult?`.

#### Scenario: Full self-test command
- **WHEN** `*TST?` is sent after all layers are healthy
- **THEN** the device replies `0`

#### Scenario: Latency query reflects recent command processing time
- **WHEN** `DIAGnostic:LATency?` is sent
- **THEN** the device replies with the microsecond duration (measured via TIM2) between receipt and reply of the most recently processed command

### Requirement: UART CLI mirrors the SCPI command table
The USART2 debug CLI SHALL accept and respond to the identical command set as the SCPI TCP server, so the device remains controllable when the network is unavailable.

#### Scenario: Network-down diagnosis via UART
- **WHEN** the Ethernet link is down and `*TST?` is sent over the USART2 CLI
- **THEN** the device responds identically to how it would over TCP 5025 were the network available

### Requirement: Phase-1 string validation commands
The device SHALL implement `TEST:ECHO?`, `TEST:STRing:LENgth?`, `TEST:STRing:UPPer?`, `TEST:STRing:REVerse?`, `TEST:STRing:CRC?`, `TEST:STRing:STORe`/`STORe?`, and `TEST:COUNt?` as specified, to validate the TCP/SCPI pipeline before tester-fixture hardware exists. String parameters SHALL be limited to 128 bytes.

#### Scenario: Echo with quoted string
- **WHEN** `TEST:ECHO? "hello"` is sent
- **THEN** the device replies `"hello"`

#### Scenario: CRC32 matches standard test vector
- **WHEN** `TEST:STRing:CRC? "123456789"` is sent
- **THEN** the device replies `"CBF43926"`

#### Scenario: Store/fetch round-trips across commands
- **WHEN** `TEST:STRing:STORe "abc"` is sent, followed by `TEST:STRing:STORe?`
- **THEN** the second command replies `"abc"`

### Requirement: SCPI error queue behavior
Missing a required parameter SHALL push error `-109,"Missing parameter"` onto the error queue with no command reply; exceeding the 128-byte string parameter limit SHALL push error `-223,"Too much data"` with no command reply. `SYSTem:ERRor?` SHALL return queued errors in FIFO order, and SHALL return `0,"No error"`-equivalent when the queue is empty.

#### Scenario: Missing parameter is reported
- **WHEN** `TEST:STRing:LENgth?` is sent with no string argument
- **THEN** the device sends no reply to that command, and a subsequent `SYSTem:ERRor?` returns `-109,"Missing parameter"`

#### Scenario: Oversized parameter is rejected
- **WHEN** `TEST:ECHO?` is sent with a quoted string longer than 128 bytes
- **THEN** the device sends no reply to that command, and a subsequent `SYSTem:ERRor?` returns `-223,"Too much data"`
