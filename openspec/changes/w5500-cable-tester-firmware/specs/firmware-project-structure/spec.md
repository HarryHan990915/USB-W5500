## ADDED Requirements

### Requirement: Layered project folder structure
The project SHALL be organized as `Core/` and `Drivers/` (CubeMX-generated, not hand-edited), `ThirdParty/{ioLibrary_Driver, scpi-parser}` (vendored open-source), `L1_Driver/`, `L2_Service/`, `L3_App/`. Application code SHALL live only under `L1_Driver/`, `L2_Service/`, `L3_App/`, and `main.c` SHALL call into this code only via a single `app_init()` invocation inside a `USER CODE` region.

#### Scenario: CubeMX regeneration does not destroy application code
- **WHEN** the `.ioc` file is regenerated in CubeIDE
- **THEN** no files under `L1_Driver/`, `L2_Service/`, `L3_App/`, or `ThirdParty/` are overwritten or deleted

### Requirement: Compiler-enforced layer isolation
Each layer SHALL only be able to include headers from the layer directly below it: `L3_App/` may include `L2_Service/` headers only, `L2_Service/` may include `L1_Driver/` headers only, `L1_Driver/` may include HAL/CMSIS headers only. This SHALL be enforced via per-folder include paths configured in CubeIDE (`C/C++ Build → Settings`), not by convention alone.

#### Scenario: App code cannot directly reach the driver layer
- **WHEN** a file under `L3_App/` contains `#include "w5500_port.h"` or any `L1_Driver/` header
- **THEN** the build fails with an unresolved-include error, because `L3_App/`'s include path does not contain `L1_Driver/`

#### Scenario: App code cannot directly reach HAL
- **WHEN** a file under `L3_App/` contains `#include "stm32g0xx_hal.h"`
- **THEN** the build fails with an unresolved-include error

### Requirement: CI layering check
A CI script SHALL scan `L3_App/` for forbidden include patterns (`w5500`, `stm32xx_hal` or equivalent HAL header names) as a backstop to the compiler-enforced include paths, since IDE project settings can be edited independently of source.

#### Scenario: CI catches a layering violation introduced via project-settings edit
- **WHEN** a developer adds `L1_Driver/` to `L3_App/`'s include path in `.cproject` and then adds a forbidden include in an `L3_App/` source file
- **THEN** the CI script detects the forbidden include string and fails the check, independent of whether the local build happens to succeed

### Requirement: CubeMX peripheral configuration
CubeMX SHALL configure: SPI1 (Full-Duplex Master, 8-bit, Mode 0, software NSS, PA5/PA6/PA7) with DMA1 (one TX channel, one RX channel, Normal mode) for burst transfers; GPIO outputs for W5500 CS (default high) and RST (default high); a GPIO EXTI input (falling edge, pull-up) for W5500 INT; USART2 on PA2/PA3 at 115200 8N1; TIM2 as a 32-bit free-running counter prescaled to 1 MHz; TIM6 as the FreeRTOS SysTick source; NVIC priorities limited to levels 0–3 with EXTI/DMA at 1–2 and SysTick/PendSV at 3.

#### Scenario: Generated project matches the intended pin/peripheral configuration
- **WHEN** the `.ioc` file is generated into CubeIDE project code
- **THEN** the generated `Core/Inc`/`Core/Src` files configure SPI1, DMA1, the three GPIOs, USART2, TIM2, TIM6, and NVIC priorities exactly as specified above

### Requirement: Vendored third-party libraries
`ThirdParty/ioLibrary_Driver` (WIZnet) and `ThirdParty/scpi-parser` (j123b567, BSD) SHALL be vendored as unmodified upstream source trees; project-specific glue code SHALL live in `L1_Driver/`/`L2_Service/`/`L3_App/`, not by editing vendored files in place.

#### Scenario: Vendored library can be updated without touching application code
- **WHEN** a newer version of `ioLibrary_Driver` is vendored in to replace the existing one
- **THEN** no changes are required in `L1_Driver/w5500_port.c` beyond re-registering the same three callback groups (`cs`, `spi`, `spiburst`)
