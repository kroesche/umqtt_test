
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>

#include "unity_fixture.h"
#include "umqtt/umqtt.h"

TEST_GROUP(BuildSubscribe);

static umqtt_Handle_t h = NULL;
static umqtt_Subscribe_Options_t options;
static umqtt_Data_t encbuf = {0, NULL};
extern uint8_t testBuf[512];

extern void umqttTest_EventCb(umqtt_Handle_t, umqtt_Event_t, void *);

TEST_SETUP(BuildSubscribe)
{
    static umqtt_Instance_t inst;
    h = umqtt_InitInstance(&inst, umqttTest_EventCb);
    options = (umqtt_Subscribe_Options_t)SUBSCRIBE_OPTIONS_INITIALIZER;
    UMQTT_INIT_DATA_STATIC_BUF(encbuf, testBuf);
}

TEST_TEAR_DOWN(BuildSubscribe)
{
    h = NULL;
}

TEST(BuildSubscribe, NullParms)
{
    umqtt_Error_t err = umqtt_BuildSubscribe(h, NULL, &options);
    TEST_ASSERT_EQUAL(UMQTT_ERR_PARM, err);
    err = umqtt_BuildSubscribe(h, &encbuf, NULL);
    TEST_ASSERT_EQUAL(UMQTT_ERR_PARM, err);
}

TEST(BuildSubscribe, BadParms)
{
    // test various combinations of bad inputs to connect
    TEST_IGNORE_MESSAGE("IMPLEMENT ME");
}

TEST(BuildSubscribe, CalcLength)
{
    umqtt_Error_t err;
    uint16_t expectedLen;
    umqtt_Data_t topics[2];
    uint8_t qoss[2];

    // create a single topic/qos packet, check length calculation
    UMQTT_INIT_DATA_STR(topics[0], "topic");
    qoss[0] = 0;
    UMQTT_INIT_SUBSCRIBE(options, 1, topics, qoss);
    // type + len + packetId + topic len + topic + qos
    expectedLen = 1 + 1 + 2 + 2 + 5 + 1;
    err = umqtt_BuildSubscribe(NULL, &encbuf, &options);
    TEST_ASSERT_EQUAL(UMQTT_ERR_RET_LEN, err);
    TEST_ASSERT_EQUAL(expectedLen, encbuf.len);

    // add a second topic and qos
    UMQTT_INIT_DATA_STR(topics[1], "topic2");
    qoss[1] = 1;
    UMQTT_INIT_SUBSCRIBE(options, 2, topics, qoss);
    expectedLen += 2 + 6 + 1;
    err = umqtt_BuildSubscribe(NULL, &encbuf, &options);
    TEST_ASSERT_EQUAL(UMQTT_ERR_RET_LEN, err);
    TEST_ASSERT_EQUAL(expectedLen, encbuf.len);

    // check that too small buffer returns error
    encbuf.len = expectedLen - 1;
    err = umqtt_BuildSubscribe(h, &encbuf, &options);
    TEST_ASSERT_EQUAL(UMQTT_ERR_BUFSIZE, err);
}

TEST(BuildSubscribe, EncodedLength)
{
    umqtt_Error_t err;
    uint16_t rem;
    umqtt_Data_t topics[1];
    uint8_t qoss[1];

    // create a dummy subscribe topic of known length
    topics[0].data = testBuf;
    topics[0].len = 45;
    qoss[0] = 0;
    UMQTT_INIT_SUBSCRIBE(options, 1, topics, qoss);
    // remaining size is packetId + topic len + topic + qos
    rem = 2 + 2 + 45 + 1;

    // create a subscribe packet and verify encoded length
    err = umqtt_BuildSubscribe(h, &encbuf, &options);
    TEST_ASSERT_EQUAL(UMQTT_ERR_OK, err);
    TEST_ASSERT_EQUAL(2 + rem, encbuf.len);
    TEST_ASSERT_EQUAL(rem, encbuf.data[1]);

    // create one that uses two bytes for remaining length
    UMQTT_INIT_DATA_STATIC_BUF(encbuf, testBuf);
    topics[0].len = 200;
    rem = 2 + 2 + 200 + 1;
    err = umqtt_BuildSubscribe(h, &encbuf, &options);
    TEST_ASSERT_EQUAL(UMQTT_ERR_OK, err);
    TEST_ASSERT_EQUAL(3 + rem, encbuf.len);
    TEST_ASSERT_EQUAL(0xCD, encbuf.data[1]);
    TEST_ASSERT_EQUAL(1, encbuf.data[2]);
}

static const uint8_t subscribePacket0[] =
{
    0x82, 10, // fixed header
    0, 1, // packet Id
    0, 5, 't','o','p','i','c', // topic 0
    0, // qos
};

static const uint8_t subscribePacket1[] =
{
    0x82, 19,
    0, 1, // packet Id
    0, 5, 't','o','p','i','c', // topic 0
    0, // qos
    0, 6, 't','o','p','i','c','2', // topic 1
    1, // qos
};

TEST(BuildSubscribe, SingleTopic)
{
    umqtt_Error_t err;
    umqtt_Data_t topics[1];
    uint8_t qoss[1];

    UMQTT_INIT_DATA_STR(topics[0], "topic");
    qoss[0] = 0;
    UMQTT_INIT_SUBSCRIBE(options, 1, topics, qoss);
    err = umqtt_BuildSubscribe(h, &encbuf, &options);
    TEST_ASSERT_EQUAL(UMQTT_ERR_OK, err);
    TEST_ASSERT_EQUAL(sizeof(subscribePacket0), encbuf.len);
    TEST_ASSERT_EQUAL_MEMORY(subscribePacket0, encbuf.data, sizeof(subscribePacket0));
}

TEST(BuildSubscribe, MultiTopic)
{
    umqtt_Error_t err;
    umqtt_Data_t topics[2];
    uint8_t qoss[2];

    UMQTT_INIT_DATA_STR(topics[0], "topic");
    qoss[0] = 0;
    UMQTT_INIT_DATA_STR(topics[1], "topic2");
    qoss[1] = 1;
    UMQTT_INIT_SUBSCRIBE(options, 2, topics, qoss);
    err = umqtt_BuildSubscribe(h, &encbuf, &options);
    TEST_ASSERT_EQUAL(UMQTT_ERR_OK, err);
    TEST_ASSERT_EQUAL(sizeof(subscribePacket1), encbuf.len);
    TEST_ASSERT_EQUAL_MEMORY(subscribePacket1, encbuf.data, sizeof(subscribePacket1));
}

TEST(BuildSubscribe, PacketId)
{
    umqtt_Error_t err;
    umqtt_Data_t topics[1];
    uint8_t qoss[1];
    umqtt_Instance_t *pInst = h;

    // verify packet id is correct in packet
    UMQTT_INIT_DATA_STR(topics[0], "topic");
    qoss[0] = 0;
    UMQTT_INIT_SUBSCRIBE(options, 1, topics, qoss);
    err = umqtt_BuildSubscribe(h, &encbuf, &options);
    TEST_ASSERT_EQUAL(UMQTT_ERR_OK, err);
    TEST_ASSERT_EQUAL(0, encbuf.data[2]);
    TEST_ASSERT_EQUAL(1, encbuf.data[3]);

    // send another packet, verify packet id increments
    UMQTT_INIT_DATA_STATIC_BUF(encbuf, testBuf); // reset encbuf
    err = umqtt_BuildSubscribe(h, &encbuf, &options);
    TEST_ASSERT_EQUAL(UMQTT_ERR_OK, err);
    TEST_ASSERT_EQUAL(0, encbuf.data[2]);
    TEST_ASSERT_EQUAL(2, encbuf.data[3]);

    // check rollover to upper byte
    UMQTT_INIT_DATA_STATIC_BUF(encbuf, testBuf); // reset encbuf
    pInst->packetId = 255; // force boundary condition
    err = umqtt_BuildSubscribe(h, &encbuf, &options);
    TEST_ASSERT_EQUAL(UMQTT_ERR_OK, err);
    TEST_ASSERT_EQUAL(1, encbuf.data[2]);
    TEST_ASSERT_EQUAL(0, encbuf.data[3]);

    UMQTT_INIT_DATA_STATIC_BUF(encbuf, testBuf); // reset encbuf
    err = umqtt_BuildSubscribe(h, &encbuf, &options);
    TEST_ASSERT_EQUAL(UMQTT_ERR_OK, err);
    TEST_ASSERT_EQUAL(1, encbuf.data[2]);
    TEST_ASSERT_EQUAL(1, encbuf.data[3]);

    // force packet id rollover, verify works
    UMQTT_INIT_DATA_STATIC_BUF(encbuf, testBuf); // reset encbuf
    pInst->packetId = 65535; // force boundary condition
    err = umqtt_BuildSubscribe(h, &encbuf, &options);
    TEST_ASSERT_EQUAL(UMQTT_ERR_OK, err);
    TEST_ASSERT_EQUAL(0, encbuf.data[2]);
    TEST_ASSERT_EQUAL(1, encbuf.data[3]);
}

TEST_GROUP_RUNNER(BuildSubscribe)
{
    RUN_TEST_CASE(BuildSubscribe, NullParms);
    RUN_TEST_CASE(BuildSubscribe, BadParms);
    RUN_TEST_CASE(BuildSubscribe, CalcLength);
    RUN_TEST_CASE(BuildSubscribe, EncodedLength);
    RUN_TEST_CASE(BuildSubscribe, SingleTopic);
    RUN_TEST_CASE(BuildSubscribe, MultiTopic);
    RUN_TEST_CASE(BuildSubscribe, PacketId);
}
