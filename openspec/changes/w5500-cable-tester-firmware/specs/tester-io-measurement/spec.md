## ADDED Requirements

### Requirement: Simulated tester I/O behind a swappable interface
`tester_io_sim.c` SHALL implement the same interface as `tester_io.h` (`tester_io_drive(pin, level)`, `tester_io_read_all()`, `tester_io_adc(pin)`), simulating an 8-wire straight-through cable where each pin is electrically shorted only to its same-numbered peer. It SHALL be selected at compile time via a `TESTER_IO_SIM=1` build flag, and swapping it for a real hardware implementation later SHALL require no changes to `L2_Service/` or `L3_App/` code.

#### Scenario: Simulated straight-through cable reports continuity on matching pins
- **WHEN** `tester_io_drive(pin=1, level=HIGH)` is called and then `tester_io_read_all()` is read back
- **THEN** only pin 1 on the simulated far end reads HIGH, matching a straight-through (non-crossed, non-shorted) cable

### Requirement: Continuity test execution
`tester_service` SHALL drive each pin in turn, read back all pins, and classify the result as open, short, miswire, or continuity-resistance-pass/fail against the configured pin count and resistance threshold, storing the result for later retrieval.

#### Scenario: Good cable passes continuity test
- **WHEN** `MEASure:CONTinuity?` is run against the simulated straight-through cable with `CONFigure:PINS 8`
- **THEN** the result indicates all 8 pins pass with no open/short/miswire detected

### Requirement: Result retrieval
`FETCh:RESult?` SHALL return the detailed per-pin result of the most recently completed `MEASure:CONTinuity?` run, without re-running the test.

#### Scenario: Fetch after measure returns the same result twice
- **WHEN** `MEASure:CONTinuity?` is run, then `FETCh:RESult?` is called twice in a row
- **THEN** both `FETCh:RESult?` calls return identical data reflecting that single measurement

### Requirement: Configurable pin count and resistance threshold
`CONFigure:PINS <n>` SHALL set the number of pins tested (default 8) and `CONFigure:THReshold:RESistance <ohms>` SHALL set the continuity resistance threshold (default 2.0 Ω) used to classify a connection as passing.

#### Scenario: Reduced pin count narrows the test
- **WHEN** `CONFigure:PINS 4` is set and `MEASure:CONTinuity?` is run
- **THEN** only pins 1–4 are driven and evaluated
