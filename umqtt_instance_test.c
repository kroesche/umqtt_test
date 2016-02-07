
#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>

#include "unity_fixture.h"
#include "umqtt.h"

TEST_GROUP(Instance);

extern void MqttTest_EventCb(Mqtt_Handle_t, Mqtt_Event_t, void *);

TEST_SETUP(Instance)
{}

TEST_TEAR_DOWN(Instance)
{}

TEST(Instance, InitNullInstance)
{
    Mqtt_Handle_t h = Mqtt_InitInstance(NULL, NULL);
    TEST_ASSERT_NULL(h);
}

TEST(Instance, InitNominal)
{
    Mqtt_Instance_t inst;
    Mqtt_Handle_t h = Mqtt_InitInstance(&inst, MqttTest_EventCb);
    TEST_ASSERT_EQUAL_PTR(&inst, h);
    TEST_ASSERT_EQUAL_PTR(MqttTest_EventCb, inst.EventCb);
}

TEST(Instance, PacketId)
{
    Mqtt_Instance_t inst;
    inst.packetId = 5;
    Mqtt_Handle_t h = Mqtt_InitInstance(&inst, MqttTest_EventCb);
    TEST_ASSERT_EQUAL_PTR(&inst, h);
    TEST_ASSERT_EQUAL(0, inst.packetId);
}

TEST_GROUP_RUNNER(Instance)
{
    RUN_TEST_CASE(Instance, InitNullInstance);
    RUN_TEST_CASE(Instance, InitNominal);
    RUN_TEST_CASE(Instance, PacketId);
}
