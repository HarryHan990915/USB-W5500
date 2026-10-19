#!/usr/bin/env bash
# Fails if L3_App reaches past L2_Service into the driver layer or HAL directly.
# Backstops the CDT per-folder include-path restriction (.cproject), in case
# project settings are edited independently of this check.
set -euo pipefail

cd "$(dirname "$0")/.."

if grep -rEn '#include\s*"(w5500|stm32[a-z0-9]*_hal)' L3_App/ 2>/dev/null; then
    echo "FAIL: L3_App/ must not include driver-layer or HAL headers directly (go through L2_Service)." >&2
    exit 1
fi

echo "OK: L3_App layering check passed."
