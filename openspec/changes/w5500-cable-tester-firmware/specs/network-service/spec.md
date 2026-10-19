## ADDED Requirements

### Requirement: Network state machine
`net_service` SHALL implement the state machine WAIT_LINK → DHCP_RUNNING → (BOUND | STATIC_FALLBACK) → SERVING. From any state, a detected link-down event SHALL transition back to WAIT_LINK and SHALL close all open sockets.

#### Scenario: Normal bring-up to serving
- **WHEN** the W5500 link comes up, DHCP subsequently succeeds
- **THEN** the state progresses WAIT_LINK → DHCP_RUNNING → BOUND → SERVING and the SCPI socket(s) are opened

#### Scenario: Link loss closes sockets and resets state
- **WHEN** the Ethernet cable is unplugged while in SERVING state
- **THEN** all open sockets are closed and the state returns to WAIT_LINK

### Requirement: DHCP failure falls back to static IP
If DHCP does not succeed, `net_service` SHALL transition to STATIC_FALLBACK using the static IP/subnet mask stored in flash configuration (default 192.168.1.200/255.255.255.0 if no valid configuration is stored), and SHALL still proceed to SERVING.

#### Scenario: DHCP timeout falls back to configured static IP
- **WHEN** DHCP does not complete within its configured timeout
- **THEN** the device configures itself with the static IP from flash config (or the default 192.168.1.200/255.255.255.0 if unset) and proceeds to SERVING

### Requirement: DHCP timer driven by software timer
The DHCP time-tick handler SHALL be invoked by a 1-second software timer, not by polling in a tight loop.

#### Scenario: DHCP state machine advances on timer tick
- **WHEN** one second elapses while in DHCP_RUNNING
- **THEN** the DHCP time handler is invoked exactly once via the software timer callback

### Requirement: SCPI socket allocation
Of the W5500's 8 hardware sockets, 2–3 SHALL be reserved for the SCPI TCP server to support multiple simultaneous SCPI clients.

#### Scenario: A second SCPI client can connect while one is already connected
- **WHEN** one SCPI client is already connected on port 5025 and a second client connects
- **THEN** the second connection is accepted on a different reserved socket, and both clients can send/receive independently
