
#include <stdint.h>
#include <stdbool.h>

#include "unity_fixture.h"
#include "umqtt/umqtt.h"

uint8_t testBuf[512];

void
umqttTest_EventCb(umqtt_Handle_t handle, umqtt_Event_t event, void *pInfo)
{}

static void
RunAllTests(void)
{
    RUN_TEST_GROUP(Instance);
    RUN_TEST_GROUP(BuildConnect);
    RUN_TEST_GROUP(BuildPublish);
    RUN_TEST_GROUP(BuildSubscribe);
    RUN_TEST_GROUP(BuildUnsubscribe);
    RUN_TEST_GROUP(Decode);
}

int
main(int argc, const char *argv[])
{
    return UnityMain(argc, argv, RunAllTests);
}
