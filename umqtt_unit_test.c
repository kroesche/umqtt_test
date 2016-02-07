
#include <stdint.h>
#include <stdbool.h>

#include "unity_fixture.h"
#include "umqtt.h"

uint8_t testBuf[512];

void
MqttTest_EventCb(Mqtt_Handle_t handle, Mqtt_Event_t event, void *pInfo)
{}

static void
RunAllTests(void)
{
    RUN_TEST_GROUP(Instance);
    RUN_TEST_GROUP(Connect);
    RUN_TEST_GROUP(Publish);
    RUN_TEST_GROUP(Subscribe);
    RUN_TEST_GROUP(Unsubscribe);
    RUN_TEST_GROUP(Process);
}

int
main(int argc, const char *argv[])
{
    return UnityMain(argc, argv, RunAllTests);
}
