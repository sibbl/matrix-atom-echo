#ifndef RETRY_POLICY_H
#define RETRY_POLICY_H

#include <stdint.h>

uint32_t retry_policy_next_delay_ms(
    uint32_t attempt,
    uint32_t base_delay_ms,
    uint32_t max_delay_ms);

#endif
