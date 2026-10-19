#ifndef L3_APP_APP_TASKS_H
#define L3_APP_APP_TASKS_H

void app_net_task(void *argument);
void app_scpi_task(void *argument);
void app_housekeep_task(void *argument);

/* Registered with net_service_register_int_wake(); wakes NetTask from the
 * W5500 INT EXTI ISR instead of waiting out its poll timeout. */
void app_net_wake_from_isr(void);

/* Defined in main.c (hand-added IWDG, see note by the hiwdg declaration
 * there). Kept as a plain wrapper so app_tasks.c doesn't need the HAL
 * include path this source folder's build config doesn't have wired in. */
void app_iwdg_refresh(void);

/* Defined in main.c. Raw instantaneous level of W5500_INT_N (0/1), bypassing
 * the EXTI/ISR path entirely — lets housekeeping tell apart "the pin never
 * toggles" (wiring fault) from "it toggles but no EXTI fires" (config fault)
 * when s_net_isr_wakes stays stuck at 0. */
int app_read_w5500_int_pin(void);

#endif
