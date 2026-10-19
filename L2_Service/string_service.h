#ifndef L2_SERVICE_STRING_SERVICE_H
#define L2_SERVICE_STRING_SERVICE_H

#include <stdint.h>
#include <stddef.h>

#define STR_SVC_MAX 128

size_t str_svc_upper(const char *in, char *out, size_t cap);
size_t str_svc_reverse(const char *in, char *out, size_t cap);
uint32_t str_svc_crc32(const char *in, size_t len);
void str_svc_store(const char *in, size_t len);
size_t str_svc_fetch(char *out, size_t cap);

#endif
