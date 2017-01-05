
#ifndef __SWTIMER_H__
#define __SWTIMER_H__

typedef struct
{
    uint32_t timeoutTicks;
    uint32_t startTicks;
} SwTimer_t;

#ifdef __cplusplus
extern "C" {
#endif

extern void SwTimer_Tick(void);
extern void SwTimer_SetMsPerTick(uint32_t ms);
extern void SwTimer_SetTimeout(SwTimer_t *pTimer, uint32_t ms);
extern bool SwTimer_IsTimedOut(SwTimer_t *pTimer);

#ifdef __cplusplus
}
#endif

#endif
