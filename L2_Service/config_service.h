#ifndef L2_SERVICE_CONFIG_SERVICE_H
#define L2_SERVICE_CONFIG_SERVICE_H

/* Loads persisted config from flash (or defaults if invalid/empty) and
 * applies it to net_service. Call once at boot, before net_service_init(). */
void config_service_load_and_apply(void);

/* Persists the current in-memory config (as loaded at boot) to flash.
 * Wired to SYSTem:CONFig:SAVE. */
void config_service_save(void);

#endif
