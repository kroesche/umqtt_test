
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>

#include "unity_fixture.h"
#include "umqtt/umqtt.h"

TEST_GROUP(BuildUnsubscribe);

static umqtt_Handle_t h = NULL;
static umqtt_Unsubscribe_Options_t options;
static umqtt_Data_t encbuf = {0, NULL};
extern uint8_t testBuf[512];

extern void umqttTest_EventCb(umqtt_Handle_t, umqtt_Event_t, void *);

TEST_SETUP(BuildUnsubscribe)
{
    static umqtt_Instance_t inst;
    h = umqtt_InitInstance(&inst, umqttTest_EventCb);
    options = (umqtt_Unsubscribe_Options_t)UNSUBSCRIBE_OPTIONS_INITIALIZER;
    UMQTT_INIT_DATA_STATIC_BUF(encbuf, testBuf);
}

TEST_TEAR_DOWN(BuildUnsubscribe)
{
    h = NULL;
}

TEST(BuildUnsubscribe, NullParms)
{
    umqtt_Error_t err = umqtt_BuildUnsubscribe(h, NULL, &options);
    TEST_ASSERT_EQUAL(UMQTT_ERR_PARM, err);
    err = umqtt_BuildUnsubscribe(h, &encbuf, NULL);
    TEST_ASSERT_EQUAL(UMQTT_ERR_PARM, err);
}

TEST(BuildUnsubscribe, BadParms)
{
    // test various combinations of bad inputs to connect
    umqtt_Error_t err;
    umqtt_Data_t topics[1];
    UMQTT_INIT_DATA_STR(topics[0], "topic");

    UMQTT_INIT_UNSUBSCRIBE(options, 1, topics);
    options.pTopics = NULL;
    err = umqtt_BuildUnsubscribe(h, &encbuf, &options);
    TEST_ASSERT_EQUAL(UMQTT_ERR_PARM, err);

    UMQTT_INIT_UNSUBSCRIBE(options, 1, topics);
    options.count = 0;
    err = umqtt_BuildUnsubscribe(h, &encbuf, &options);
    TEST_ASSERT_EQUAL(UMQTT_ERR_PARM, err);

    UMQTT_INIT_UNSUBSCRIBE(options, 1, topics);
    topics[0].data = NULL;
    err = umqtt_BuildUnsubscribe(h, &encbuf, &options);
    TEST_ASSERT_EQUAL(UMQTT_ERR_PARM, err);

    UMQTT_INIT_UNSUBSCRIBE(options, 1, topics);
    topics[0].len = 0;
    err = umqtt_BuildUnsubscribe(h, &encbuf, &options);
    TEST_ASSERT_EQUAL(UMQTT_ERR_PARM, err);

    UMQTT_INIT_UNSUBSCRIBE(options, 1, topics);
    encbuf.data = NULL;
    err = umqtt_BuildUnsubscribe(h, &encbuf, &options);
    TEST_ASSERT_EQUAL(UMQTT_ERR_PARM, err);

    UMQTT_INIT_UNSUBSCRIBE(options, 1, topics);
    encbuf.len = 0;
    err = umqtt_BuildUnsubscribe(h, &encbuf, &options);
    TEST_ASSERT_EQUAL(UMQTT_ERR_PARM, err);
}

TEST(BuildUnsubscribe, CalcLength)
{
    umqtt_Error_t err;
    uint16_t expectedLen;
    umqtt_Data_t topics[2];

    // create a single topic packet, check length calculation
    UMQTT_INIT_DATA_STR(topics[0], "topic");
    UMQTT_INIT_UNSUBSCRIBE(options, 1, topics);
    // type + len + packetId + topic len + topic
    expectedLen = 1 + 1 + 2 + 2 + 5;
    err = umqtt_BuildUnsubscribe(NULL, &encbuf, &options);
    TEST_ASSERT_EQUAL(UMQTT_ERR_RET_LEN, err);
    TEST_ASSERT_EQUAL(expectedLen, encbuf.len);

    // add a second topic
    UMQTT_INIT_DATA_STR(topics[1], "topic2");
    UMQTT_INIT_UNSUBSCRIBE(options, 2, topics);
    expectedLen += 2 + 6;
    err = umqtt_BuildUnsubscribe(NULL, &encbuf, &options);
    TEST_ASSERT_EQUAL(UMQTT_ERR_RET_LEN, err);
    TEST_ASSERT_EQUAL(expectedLen, encbuf.len);

    // check that too small buffer returns error
    encbuf.len = expectedLen - 1;
    err = umqtt_BuildUnsubscribe(h, &encbuf, &options);
    TEST_ASSERT_EQUAL(UMQTT_ERR_BUFSIZE, err);
}

TEST(BuildUnsubscribe, EncodedLength)
{
    umqtt_Error_t err;
    uint16_t rem;
    umqtt_Data_t topics[1];

    // create a dummy subscribe topic of known length
    topics[0].data = testBuf;
    topics[0].len = 45;
    UMQTT_INIT_UNSUBSCRIBE(options, 1, topics);
    // remaining size is packetId + topic len + topic + qos
    rem = 2 + 2 + 45;

    // create a subscribe packet and verify encoded length
    err = umqtt_BuildUnsubscribe(h, &encbuf, &options);
    TEST_ASSERT_EQUAL(UMQTT_ERR_OK, err);
    TEST_ASSERT_EQUAL(2 + rem, encbuf.len);
    TEST_ASSERT_EQUAL(rem, encbuf.data[1]);

    // create one that uses two bytes for remaining length
    UMQTT_INIT_DATA_STATIC_BUF(encbuf, testBuf);
    topics[0].len = 201;
    rem = 2 + 2 + 201;
    err = umqtt_BuildUnsubscribe(h, &encbuf, &options);
    TEST_ASSERT_EQUAL(UMQTT_ERR_OK, err);
    TEST_ASSERT_EQUAL(3 + rem, encbuf.len);
    TEST_ASSERT_EQUAL(0xCD, encbuf.data[1]);
    TEST_ASSERT_EQUAL(1, encbuf.data[2]);
}

static const uint8_t unsubscribePacket0[] =
{
    0xA2, 9, // fixed header
    0, 1, // packet Id
    0, 5, 't','o','p','i','c', // topic 0
};

static const uint8_t unsubscribePacket1[] =
{
    0xA2, 17,
    0, 1, // packet Id
    0, 5, 't','o','p','i','c', // topic 0
    0, 6, 't','o','p','i','c','2', // topic 1
};

TEST(BuildUnsubscribe, SingleTopic)
{
    umqtt_Error_t err;
    umqtt_Data_t topics[1];

    UMQTT_INIT_DATA_STR(topics[0], "topic");
    UMQTT_INIT_UNSUBSCRIBE(options, 1, topics);
    err = umqtt_BuildUnsubscribe(h, &encbuf, &options);
    TEST_ASSERT_EQUAL(UMQTT_ERR_OK, err);
    TEST_ASSERT_EQUAL(sizeof(unsubscribePacket0), encbuf.len);
    TEST_ASSERT_EQUAL_MEMORY(unsubscribePacket0, encbuf.data, sizeof(unsubscribePacket0));
}

TEST(BuildUnsubscribe, MultiTopic)
{
    umqtt_Error_t err;
    umqtt_Data_t topics[2];

    UMQTT_INIT_DATA_STR(topics[0], "topic");
    UMQTT_INIT_DATA_STR(topics[1], "topic2");
    UMQTT_INIT_UNSUBSCRIBE(options, 2, topics);
    err = umqtt_BuildUnsubscribe(h, &encbuf, &options);
    TEST_ASSERT_EQUAL(UMQTT_ERR_OK, err);
    TEST_ASSERT_EQUAL(sizeof(unsubscribePacket1), encbuf.len);
    TEST_ASSERT_EQUAL_MEMORY(unsubscribePacket1, encbuf.data, sizeof(unsubscribePacket1));
}

TEST(BuildUnsubscribe, PacketId)
{
    umqtt_Error_t err;
    umqtt_Data_t topics[1];
    umqtt_Instance_t *pInst = h;

    // verify packet id is correct in packet
    UMQTT_INIT_DATA_STR(topics[0], "topic");
    UMQTT_INIT_UNSUBSCRIBE(options, 1, topics);
    err = umqtt_BuildUnsubscribe(h, &encbuf, &options);
    TEST_ASSERT_EQUAL(UMQTT_ERR_OK, err);
    TEST_ASSERT_EQUAL(0, encbuf.data[2]);
    TEST_ASSERT_EQUAL(1, encbuf.data[3]);

    // send another packet, verify packet id increments
    UMQTT_INIT_DATA_STATIC_BUF(encbuf, testBuf); // reset encbuf
    err = umqtt_BuildUnsubscribe(h, &encbuf, &options);
    TEST_ASSERT_EQUAL(UMQTT_ERR_OK, err);
    TEST_ASSERT_EQUAL(0, encbuf.data[2]);
    TEST_ASSERT_EQUAL(2, encbuf.data[3]);

    // check rollover to upper byte
    UMQTT_INIT_DATA_STATIC_BUF(encbuf, testBuf); // reset encbuf
    pInst->packetId = 255; // force boundary condition
    err = umqtt_BuildUnsubscribe(h, &encbuf, &options);
    TEST_ASSERT_EQUAL(UMQTT_ERR_OK, err);
    TEST_ASSERT_EQUAL(1, encbuf.data[2]);
    TEST_ASSERT_EQUAL(0, encbuf.data[3]);

    UMQTT_INIT_DATA_STATIC_BUF(encbuf, testBuf); // reset encbuf
    err = umqtt_BuildUnsubscribe(h, &encbuf, &options);
    TEST_ASSERT_EQUAL(UMQTT_ERR_OK, err);
    TEST_ASSERT_EQUAL(1, encbuf.data[2]);
    TEST_ASSERT_EQUAL(1, encbuf.data[3]);

    // force packet id rollover, verify works
    UMQTT_INIT_DATA_STATIC_BUF(encbuf, testBuf); // reset encbuf
    pInst->packetId = 65535; // force boundary condition
    err = umqtt_BuildUnsubscribe(h, &encbuf, &options);
    TEST_ASSERT_EQUAL(UMQTT_ERR_OK, err);
    TEST_ASSERT_EQUAL(0, encbuf.data[2]);
    TEST_ASSERT_EQUAL(1, encbuf.data[3]);
}

TEST_GROUP_RUNNER(BuildUnsubscribe)
{
    RUN_TEST_CASE(BuildUnsubscribe, NullParms);
    RUN_TEST_CASE(BuildUnsubscribe, BadParms);
    RUN_TEST_CASE(BuildUnsubscribe, CalcLength);
    RUN_TEST_CASE(BuildUnsubscribe, EncodedLength);
    RUN_TEST_CASE(BuildUnsubscribe, SingleTopic);
    RUN_TEST_CASE(BuildUnsubscribe, MultiTopic);
    RUN_TEST_CASE(BuildUnsubscribe, PacketId);
}
