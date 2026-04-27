#include "retry_policy.h"

uint32_t retry_policy_next_delay_ms(
    uint32_t attempt,
    uint32_t base_delay_ms,
    uint32_t max_delay_ms)
{
    uint32_t delay_ms = base_delay_ms;
    uint32_t index = 0U;

    if (base_delay_ms == 0U) {
        return 0U;
    }

    if (max_delay_ms != 0U && delay_ms > max_delay_ms) {
        delay_ms = max_delay_ms;
    }

    for (index = 0U; index < attempt; index += 1U) {
        if (max_delay_ms != 0U && delay_ms >= max_delay_ms) {
            return max_delay_ms;
        }

        if (delay_ms > UINT32_MAX / 2U) {
            return max_delay_ms == 0U ? UINT32_MAX : max_delay_ms;
        }

        delay_ms *= 2U;
    }

    if (max_delay_ms != 0U && delay_ms > max_delay_ms) {
        return max_delay_ms;
    }

    return delay_ms;
}
