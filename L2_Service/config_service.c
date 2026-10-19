#include "config_service.h"
#include "flash_store.h"
#include "net_service.h"
#include "uart_log.h"
#include <string.h>

static flash_config_t s_cfg;

void config_service_load_and_apply(void)
{
    int valid = flash_store_read(&s_cfg);
    const char *msg = valid ? "CONFIG: loaded from flash\r\n"
                             : "CONFIG: using defaults (flash invalid/empty)\r\n";
    uart_log_write(msg, (uint16_t)strlen(msg));

    net_service_set_static_fallback(s_cfg.ip, s_cfg.sn, s_cfg.gw);
}

void config_service_save(void)
{
    int ok = flash_store_write(&s_cfg);
    const char *msg = ok ? "CONFIG: saved to flash\r\n" : "CONFIG: flash write FAILED\r\n";
    uart_log_write(msg, (uint16_t)strlen(msg));
}
