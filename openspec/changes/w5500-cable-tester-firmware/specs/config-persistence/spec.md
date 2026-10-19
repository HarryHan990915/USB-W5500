## ADDED Requirements

### Requirement: Flash-backed configuration storage
`flash_store` SHALL persist network and test configuration in the last 2 KB flash page (0x0801F800), written using 64-bit double-word writes as required by the STM32G0 flash controller, guarded by erase-before-write of the whole page.

#### Scenario: Configuration survives a power cycle
- **WHEN** configuration is written via `flash_store_write()` and the device is power-cycled
- **THEN** `flash_store_read()` after reboot returns the same configuration that was written

### Requirement: CRC-validated configuration with default fallback
Stored configuration SHALL include a magic value and a CRC32 checksum (same CRC32 implementation used by `TEST:STRing:CRC?`). On read, if the magic or CRC does not validate, `flash_store_read()` SHALL return compiled-in default values rather than corrupted data.

#### Scenario: Corrupted flash page falls back to defaults
- **WHEN** the stored configuration's CRC32 does not match its contents (e.g. due to an interrupted write)
- **THEN** `flash_store_read()` returns the default configuration (including the default static IP 192.168.1.200/255.255.255.0) instead of the corrupted values

### Requirement: Explicit save command
`SYSTem:CONFig:SAVE` SHALL write the current in-memory network/test configuration to flash via `flash_store_write()`; configuration changes made via other SCPI commands SHALL NOT be persisted until this command is issued.

#### Scenario: Unsaved config change does not survive reboot
- **WHEN** `SYSTem:COMMunicate:LAN:ADDRess` is changed and applied, but `SYSTem:CONFig:SAVE` is never sent, and the device is power-cycled
- **THEN** the device boots with the previously saved (or default) configuration, not the unsaved change
