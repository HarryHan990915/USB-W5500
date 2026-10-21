#include "flash_store.h"
#include "main.h"
#include <string.h>
#include <stddef.h>

#define FLASH_STORE_MAGIC 0x53435049u /* "SCPI" */
#define FLASH_STORE_ADDR  0x0801F800u /* last 2 KB page of 128 KB flash */

/* Same CRC32 (poly 0xEDB88320, init/final 0xFFFFFFFF) as L2_Service/string_service,
 * duplicated here since L1_Driver doesn't depend on L2_Service. */
static uint32_t crc32_calc(const uint8_t *data, size_t len)
{
    uint32_t crc = 0xFFFFFFFFu;
    for (size_t i = 0; i < len; i++) {
        crc ^= data[i];
        for (int b = 0; b < 8; b++) {
            uint32_t mask = (uint32_t)(-(int32_t)(crc & 1u));
            crc = (crc >> 1) ^ (0xEDB88320u & mask);
        }
    }
    return crc ^ 0xFFFFFFFFu;
}

void flash_store_defaults(flash_config_t *cfg)
{
    memset(cfg, 0, sizeof(*cfg));
    cfg->magic = FLASH_STORE_MAGIC;
    uint8_t ip[4] = {192, 168, 2, 131};
    uint8_t sn[4] = {255, 255, 255, 0};
    uint8_t gw[4] = {192, 168, 2, 1};
    memcpy(cfg->ip, ip, 4);
    memcpy(cfg->sn, sn, 4);
    memcpy(cfg->gw, gw, 4);
    /* Shipping product needs a predictable address, not whatever a DHCP
     * server happens to hand out — see net_service.c's s_dhcp_enabled. */
    cfg->dhcp_enabled = 0;
}

int flash_store_read(flash_config_t *cfg)
{
    const flash_config_t *stored = (const flash_config_t *)FLASH_STORE_ADDR;
    flash_config_t tmp = *stored; /* flash is memory-mapped, plain read is fine */

    size_t crc_offset = offsetof(flash_config_t, crc32);
    uint32_t computed = crc32_calc((const uint8_t *)&tmp, crc_offset);

    if (tmp.magic == FLASH_STORE_MAGIC && computed == tmp.crc32) {
        *cfg = tmp;
        return 1;
    }
    flash_store_defaults(cfg);
    return 0;
}

int flash_store_write(const flash_config_t *cfg)
{
    flash_config_t tmp = *cfg;
    tmp.magic = FLASH_STORE_MAGIC;
    size_t crc_offset = offsetof(flash_config_t, crc32);
    tmp.crc32 = crc32_calc((const uint8_t *)&tmp, crc_offset);

    HAL_FLASH_Unlock();

    FLASH_EraseInitTypeDef erase = {0};
    erase.TypeErase = FLASH_TYPEERASE_PAGES;
    erase.Banks = FLASH_BANK_1;
    erase.Page = FLASH_PAGE_NB - 1;
    erase.NbPages = 1;
    uint32_t page_error = 0;

    HAL_StatusTypeDef st = HAL_FLASHEx_Erase(&erase, &page_error);
    if (st != HAL_OK) {
        HAL_FLASH_Lock();
        return 0;
    }

    uint8_t buf[(sizeof(flash_config_t) + 7U) & ~((size_t)7U)];
    memset(buf, 0xFF, sizeof(buf));
    memcpy(buf, &tmp, sizeof(tmp));

    for (size_t off = 0; off < sizeof(buf); off += 8U) {
        uint64_t dword;
        memcpy(&dword, &buf[off], 8U);
        st = HAL_FLASH_Program(FLASH_TYPEPROGRAM_DOUBLEWORD, FLASH_STORE_ADDR + off, dword);
        if (st != HAL_OK) {
            break;
        }
    }

    HAL_FLASH_Lock();
    return (st == HAL_OK) ? 1 : 0;
}
