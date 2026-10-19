#include "string_service.h"
#include <string.h>

size_t str_svc_upper(const char *in, char *out, size_t cap)
{
    size_t n = strlen(in);
    if (n >= cap) {
        n = cap - 1;
    }
    for (size_t i = 0; i < n; i++) {
        char c = in[i];
        out[i] = (c >= 'a' && c <= 'z') ? (char)(c - 'a' + 'A') : c;
    }
    out[n] = '\0';
    return n;
}

size_t str_svc_reverse(const char *in, char *out, size_t cap)
{
    size_t n = strlen(in);
    if (n >= cap) {
        n = cap - 1;
    }
    for (size_t i = 0; i < n; i++) {
        out[i] = in[n - 1 - i];
    }
    out[n] = '\0';
    return n;
}

uint32_t str_svc_crc32(const char *in, size_t len)
{
    uint32_t crc = 0xFFFFFFFFu;
    for (size_t i = 0; i < len; i++) {
        crc ^= (uint8_t)in[i];
        for (int b = 0; b < 8; b++) {
            uint32_t mask = (uint32_t)(-(int32_t)(crc & 1u));
            crc = (crc >> 1) ^ (0xEDB88320u & mask);
        }
    }
    return crc ^ 0xFFFFFFFFu;
}

static char s_store_buf[STR_SVC_MAX + 1];
static size_t s_store_len = 0;

void str_svc_store(const char *in, size_t len)
{
    if (len > STR_SVC_MAX) {
        len = STR_SVC_MAX;
    }
    memcpy(s_store_buf, in, len);
    s_store_buf[len] = '\0';
    s_store_len = len;
}

size_t str_svc_fetch(char *out, size_t cap)
{
    size_t n = s_store_len;
    if (n >= cap) {
        n = cap - 1;
    }
    memcpy(out, s_store_buf, n);
    out[n] = '\0';
    return n;
}
