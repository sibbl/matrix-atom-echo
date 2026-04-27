#include <assert.h>

#include "retry_policy.h"

int main(void)
{
    assert(retry_policy_next_delay_ms(0U, 250U, 2000U) == 250U);
    assert(retry_policy_next_delay_ms(1U, 250U, 2000U) == 500U);
    assert(retry_policy_next_delay_ms(2U, 250U, 2000U) == 1000U);
    assert(retry_policy_next_delay_ms(4U, 250U, 2000U) == 2000U);
    assert(retry_policy_next_delay_ms(3U, 0U, 2000U) == 0U);

    return 0;
}
