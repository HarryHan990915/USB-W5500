#ifndef L2_SERVICE_SCPI_SERVER_H
#define L2_SERVICE_SCPI_SERVER_H

#include "scpi/types.h"

void scpi_server_init(const scpi_command_t *commands,
                       const char *idn1, const char *idn2, const char *idn3, const char *idn4);
void scpi_server_poll(void);

#endif
