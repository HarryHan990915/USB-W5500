#include "scpi_commands.h"
#include "scpi_server.h"
#include "string_service.h"
#include "config_service.h"
#include "diag_service.h"
#include "led_bench.h"
#include "scpi/parser.h"
#include "scpi/minimal.h"
#include "scpi/ieee488.h"
#include "scpi/error.h"
#include <stdio.h>
#include <string.h>

static uint32_t s_cmd_count = 0;

static scpi_result_t TEST_EchoQ(scpi_t *ctx)
{
    s_cmd_count++;
    char in[STR_SVC_MAX * 2];
    size_t len;
    if (!SCPI_ParamCopyText(ctx, in, sizeof(in), &len, TRUE)) {
        return SCPI_RES_ERR;
    }
    if (len > STR_SVC_MAX) {
        SCPI_ErrorPush(ctx, SCPI_ERROR_TOO_MUCH_DATA);
        return SCPI_RES_ERR;
    }
    in[len] = '\0';
    SCPI_ResultText(ctx, in);
    return SCPI_RES_OK;
}

static scpi_result_t TEST_StrLenQ(scpi_t *ctx)
{
    s_cmd_count++;
    char in[STR_SVC_MAX * 2];
    size_t len;
    if (!SCPI_ParamCopyText(ctx, in, sizeof(in), &len, TRUE)) {
        return SCPI_RES_ERR;
    }
    if (len > STR_SVC_MAX) {
        SCPI_ErrorPush(ctx, SCPI_ERROR_TOO_MUCH_DATA);
        return SCPI_RES_ERR;
    }
    SCPI_ResultUInt32(ctx, (uint32_t)len);
    return SCPI_RES_OK;
}

static scpi_result_t TEST_StrUpperQ(scpi_t *ctx)
{
    s_cmd_count++;
    char in[STR_SVC_MAX * 2], out[STR_SVC_MAX + 1];
    size_t len;
    if (!SCPI_ParamCopyText(ctx, in, sizeof(in), &len, TRUE)) {
        return SCPI_RES_ERR;
    }
    if (len > STR_SVC_MAX) {
        SCPI_ErrorPush(ctx, SCPI_ERROR_TOO_MUCH_DATA);
        return SCPI_RES_ERR;
    }
    in[len] = '\0';
    str_svc_upper(in, out, sizeof(out));
    SCPI_ResultText(ctx, out);
    return SCPI_RES_OK;
}

static scpi_result_t TEST_StrReverseQ(scpi_t *ctx)
{
    s_cmd_count++;
    char in[STR_SVC_MAX * 2], out[STR_SVC_MAX + 1];
    size_t len;
    if (!SCPI_ParamCopyText(ctx, in, sizeof(in), &len, TRUE)) {
        return SCPI_RES_ERR;
    }
    if (len > STR_SVC_MAX) {
        SCPI_ErrorPush(ctx, SCPI_ERROR_TOO_MUCH_DATA);
        return SCPI_RES_ERR;
    }
    in[len] = '\0';
    str_svc_reverse(in, out, sizeof(out));
    SCPI_ResultText(ctx, out);
    return SCPI_RES_OK;
}

static scpi_result_t TEST_StrCrcQ(scpi_t *ctx)
{
    s_cmd_count++;
    char in[STR_SVC_MAX * 2];
    size_t len;
    if (!SCPI_ParamCopyText(ctx, in, sizeof(in), &len, TRUE)) {
        return SCPI_RES_ERR;
    }
    if (len > STR_SVC_MAX) {
        SCPI_ErrorPush(ctx, SCPI_ERROR_TOO_MUCH_DATA);
        return SCPI_RES_ERR;
    }
    uint32_t crc = str_svc_crc32(in, len);
    char out[16];
    snprintf(out, sizeof(out), "%08lX", (unsigned long)crc);
    SCPI_ResultText(ctx, out);
    return SCPI_RES_OK;
}

static scpi_result_t TEST_StrStore(scpi_t *ctx)
{
    s_cmd_count++;
    char in[STR_SVC_MAX * 2];
    size_t len;
    if (!SCPI_ParamCopyText(ctx, in, sizeof(in), &len, TRUE)) {
        return SCPI_RES_ERR;
    }
    if (len > STR_SVC_MAX) {
        SCPI_ErrorPush(ctx, SCPI_ERROR_TOO_MUCH_DATA);
        return SCPI_RES_ERR;
    }
    str_svc_store(in, len);
    return SCPI_RES_OK;
}

static scpi_result_t TEST_StrStoreQ(scpi_t *ctx)
{
    s_cmd_count++;
    char out[STR_SVC_MAX + 1];
    str_svc_fetch(out, sizeof(out));
    SCPI_ResultText(ctx, out);
    return SCPI_RES_OK;
}

static scpi_result_t TEST_CountQ(scpi_t *ctx)
{
    s_cmd_count++;
    SCPI_ResultUInt32(ctx, s_cmd_count);
    return SCPI_RES_OK;
}

static scpi_result_t CoreTstQ(scpi_t *ctx)
{
    s_cmd_count++;
    SCPI_ResultInt32(ctx, diag_run_full_selftest());
    return SCPI_RES_OK;
}

static scpi_result_t SYSTem_ConfigSave(scpi_t *ctx)
{
    s_cmd_count++;
    config_service_save();
    return SCPI_RES_OK;
}

static scpi_result_t DIAGnostic_LatencyQ(scpi_t *ctx)
{
    s_cmd_count++;
    SCPI_ResultUInt32(ctx, diag_latency_get_last_us());
    return SCPI_RES_OK;
}

static scpi_result_t TEST_LedBlink(scpi_t *ctx)
{
    s_cmd_count++;
    int32_t n;
    if (!SCPI_ParamInt32(ctx, &n, TRUE)) {
        return SCPI_RES_ERR;
    }
    /* Not a query — returns immediately. The actual blinking runs in
     * ScpiTask (see led_bench.c/app_tasks.c), so this never blocks NetTask,
     * and TEST:LED:STATus? can be polled for progress while it runs. Queued
     * (not just stashed), so back-to-back requests don't clobber each other
     * while ScpiTask is still working through an earlier one. */
    if (!led_bench_request_start((int)n)) {
        SCPI_ErrorPush(ctx, SCPI_ERROR_QUEUE_OVERFLOW);
        return SCPI_RES_ERR;
    }
    return SCPI_RES_OK;
}

static scpi_result_t TEST_LedAbort(scpi_t *ctx)
{
    s_cmd_count++;
    led_bench_request_abort();
    return SCPI_RES_OK;
}

static scpi_result_t TEST_LedInterval(scpi_t *ctx)
{
    s_cmd_count++;
    int32_t ms;
    if (!SCPI_ParamInt32(ctx, &ms, TRUE)) {
        return SCPI_RES_ERR;
    }
    led_bench_set_interval_ms((int)ms);
    return SCPI_RES_OK;
}

static scpi_result_t TEST_LedIntervalQ(scpi_t *ctx)
{
    s_cmd_count++;
    SCPI_ResultInt32(ctx, led_bench_get_interval_ms());
    return SCPI_RES_OK;
}

static scpi_result_t TEST_LedStatusQ(scpi_t *ctx)
{
    s_cmd_count++;
    int running, done, total;
    led_bench_get_status(&running, &done, &total);
    char out[32];
    snprintf(out, sizeof(out), "%d,%d,%d", running, done, total);
    SCPI_ResultText(ctx, out);
    return SCPI_RES_OK;
}

static const scpi_command_t s_scpi_commands[] = {
    {.pattern = "*IDN?", .callback = SCPI_CoreIdnQ},
    {.pattern = "*RST", .callback = SCPI_CoreRst},
    {.pattern = "*CLS", .callback = SCPI_CoreCls},
    {.pattern = "*TST?", .callback = CoreTstQ},
    {.pattern = "SYSTem:ERRor?", .callback = SCPI_SystemErrorNextQ},
    {.pattern = "SYSTem:ERRor:NEXT?", .callback = SCPI_SystemErrorNextQ},

    {.pattern = "TEST:ECHO?", .callback = TEST_EchoQ},
    {.pattern = "TEST:STRing:LENgth?", .callback = TEST_StrLenQ},
    {.pattern = "TEST:STRing:UPPer?", .callback = TEST_StrUpperQ},
    {.pattern = "TEST:STRing:REVerse?", .callback = TEST_StrReverseQ},
    {.pattern = "TEST:STRing:CRC?", .callback = TEST_StrCrcQ},
    {.pattern = "TEST:STRing:STORe", .callback = TEST_StrStore},
    {.pattern = "TEST:STRing:STORe?", .callback = TEST_StrStoreQ},
    {.pattern = "TEST:COUNt?", .callback = TEST_CountQ},
    {.pattern = "TEST:LED:BLINk", .callback = TEST_LedBlink},
    {.pattern = "TEST:LED:ABORt", .callback = TEST_LedAbort},
    {.pattern = "TEST:LED:INTerval", .callback = TEST_LedInterval},
    {.pattern = "TEST:LED:INTerval?", .callback = TEST_LedIntervalQ},
    {.pattern = "TEST:LED:STATus?", .callback = TEST_LedStatusQ},

    {.pattern = "SYSTem:CONFig:SAVE", .callback = SYSTem_ConfigSave},
    {.pattern = "DIAGnostic:LATency?", .callback = DIAGnostic_LatencyQ},

    SCPI_CMD_LIST_END,
};

void scpi_commands_register(void)
{
    scpi_server_init(s_scpi_commands, "Company", "CableTester", "SN001", "FW1.0");
}
