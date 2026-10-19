#ifndef L1_DRIVER_FLASH_STORE_H
#define L1_DRIVER_FLASH_STORE_H

#include <stdint.h>

typedef struct {
    uint32_t magic;
    uint8_t ip[4];
    uint8_t sn[4];
    uint8_t gw[4];
    uint8_t dhcp_enabled;
    uint32_t crc32;
} flash_config_t;

void flash_store_defaults(flash_config_t *cfg);
/* Returns 1 if a valid (magic+CRC match) config was read, 0 if defaults were used instead. */
int flash_store_read(flash_config_t *cfg);
/* Returns 1 on successful erase+program, 0 on failure. */
int flash_store_write(const flash_config_t *cfg);

#endif
