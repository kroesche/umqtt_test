
#include <stdint.h>
#include <stdbool.h>

#include "swtimer.h"

#ifndef DEFAULT_MS_PER_TICK
#define DEFAULT_MS_PER_TICK 100
#endif

static uint32_t ticks;
static uint32_t msPerTick = DEFAULT_MS_PER_TICK;

void
SwTimer_Tick(void)
{
    ++ticks;
}

void
SwTimer_SetMsPerTick(uint32_t ms)
{
    msPerTick = ms;
}

void
SwTimer_SetTimeout(SwTimer_t *pTimer, uint32_t ms)
{
    pTimer->startTicks = ticks;
    pTimer->timeoutTicks = ms / msPerTick;
}

bool
SwTimer_IsTimedOut(SwTimer_t *pTimer)
{
    return (ticks - pTimer->startTicks) >= pTimer->timeoutTicks ? true: false;
}
