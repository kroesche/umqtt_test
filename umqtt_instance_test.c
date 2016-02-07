
#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>

#include "unity_fixture.h"
#include "umqtt/umqtt.h"

TEST_GROUP(Instance);

extern void umqttTest_EventCb(umqtt_Handle_t, umqtt_Event_t, void *);

TEST_SETUP(Instance)
{}

TEST_TEAR_DOWN(Instance)
{}

TEST(Instance, InitNullInstance)
{
    umqtt_Handle_t h = umqtt_InitInstance(NULL, NULL);
    TEST_ASSERT_NULL(h);
}

TEST(Instance, InitNominal)
{
    umqtt_Instance_t inst;
    umqtt_Handle_t h = umqtt_InitInstance(&inst, umqttTest_EventCb);
    TEST_ASSERT_EQUAL_PTR(&inst, h);
    TEST_ASSERT_EQUAL_PTR(umqttTest_EventCb, inst.EventCb);
}

TEST(Instance, PacketId)
{
    umqtt_Instance_t inst;
    inst.packetId = 5;
    umqtt_Handle_t h = umqtt_InitInstance(&inst, umqttTest_EventCb);
    TEST_ASSERT_EQUAL_PTR(&inst, h);
    TEST_ASSERT_EQUAL(0, inst.packetId);
}

TEST_GROUP_RUNNER(Instance)
{
    RUN_TEST_CASE(Instance, InitNullInstance);
    RUN_TEST_CASE(Instance, InitNominal);
    RUN_TEST_CASE(Instance, PacketId);
}
