
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>

#include "unity_fixture.h"
#include "umqtt.h"

TEST_GROUP(Unsubscribe);

static Mqtt_Handle_t h = NULL;
static Mqtt_Unsubscribe_Options_t options;
static Mqtt_Data_t encbuf = {0, NULL};
extern uint8_t testBuf[512];

extern void MqttTest_EventCb(Mqtt_Handle_t, Mqtt_Event_t, void *);

TEST_SETUP(Unsubscribe)
{
    static Mqtt_Instance_t inst;
    h = Mqtt_InitInstance(&inst, MqttTest_EventCb);
    options = (Mqtt_Unsubscribe_Options_t)UNSUBSCRIBE_OPTIONS_INITIALIZER;
    MQTT_INIT_DATA_STATIC_BUF(encbuf, testBuf);
}

TEST_TEAR_DOWN(Unsubscribe)
{
    h = NULL;
}

TEST(Unsubscribe, NullParms)
{
    Mqtt_Error_t err = Mqtt_Unsubscribe(h, NULL, &options);
    TEST_ASSERT_EQUAL(MQTT_ERR_PARM, err);
    err = Mqtt_Unsubscribe(h, &encbuf, NULL);
    TEST_ASSERT_EQUAL(MQTT_ERR_PARM, err);
}

TEST(Unsubscribe, BadParms)
{
    // test various combinations of bad inputs to connect
    Mqtt_Error_t err;
    Mqtt_Data_t topics[1];
    MQTT_INIT_DATA_STR(topics[0], "topic");

    MQTT_INIT_UNSUBSCRIBE(options, 1, topics);
    options.pTopics = NULL;
    err = Mqtt_Unsubscribe(h, &encbuf, &options);
    TEST_ASSERT_EQUAL(MQTT_ERR_PARM, err);

    MQTT_INIT_UNSUBSCRIBE(options, 1, topics);
    options.count = 0;
    err = Mqtt_Unsubscribe(h, &encbuf, &options);
    TEST_ASSERT_EQUAL(MQTT_ERR_PARM, err);

    MQTT_INIT_UNSUBSCRIBE(options, 1, topics);
    topics[0].data = NULL;
    err = Mqtt_Unsubscribe(h, &encbuf, &options);
    TEST_ASSERT_EQUAL(MQTT_ERR_PARM, err);

    MQTT_INIT_UNSUBSCRIBE(options, 1, topics);
    topics[0].len = 0;
    err = Mqtt_Unsubscribe(h, &encbuf, &options);
    TEST_ASSERT_EQUAL(MQTT_ERR_PARM, err);

    MQTT_INIT_UNSUBSCRIBE(options, 1, topics);
    encbuf.data = NULL;
    err = Mqtt_Unsubscribe(h, &encbuf, &options);
    TEST_ASSERT_EQUAL(MQTT_ERR_PARM, err);

    MQTT_INIT_UNSUBSCRIBE(options, 1, topics);
    encbuf.len = 0;
    err = Mqtt_Unsubscribe(h, &encbuf, &options);
    TEST_ASSERT_EQUAL(MQTT_ERR_PARM, err);
}

TEST(Unsubscribe, CalcLength)
{
    Mqtt_Error_t err;
    uint16_t expectedLen;
    Mqtt_Data_t topics[2];

    // create a single topic packet, check length calculation
    MQTT_INIT_DATA_STR(topics[0], "topic");
    MQTT_INIT_UNSUBSCRIBE(options, 1, topics);
    // type + len + packetId + topic len + topic
    expectedLen = 1 + 1 + 2 + 2 + 5;
    err = Mqtt_Unsubscribe(NULL, &encbuf, &options);
    TEST_ASSERT_EQUAL(MQTT_ERR_RET_LEN, err);
    TEST_ASSERT_EQUAL(expectedLen, encbuf.len);

    // add a second topic
    MQTT_INIT_DATA_STR(topics[1], "topic2");
    MQTT_INIT_UNSUBSCRIBE(options, 2, topics);
    expectedLen += 2 + 6;
    err = Mqtt_Unsubscribe(NULL, &encbuf, &options);
    TEST_ASSERT_EQUAL(MQTT_ERR_RET_LEN, err);
    TEST_ASSERT_EQUAL(expectedLen, encbuf.len);

    // check that too small buffer returns error
    encbuf.len = expectedLen - 1;
    err = Mqtt_Unsubscribe(h, &encbuf, &options);
    TEST_ASSERT_EQUAL(MQTT_ERR_BUFSIZE, err);
}

TEST(Unsubscribe, EncodedLength)
{
    Mqtt_Error_t err;
    uint16_t rem;
    Mqtt_Data_t topics[1];

    // create a dummy subscribe topic of known length
    topics[0].data = testBuf;
    topics[0].len = 45;
    MQTT_INIT_UNSUBSCRIBE(options, 1, topics);
    // remaining size is packetId + topic len + topic + qos
    rem = 2 + 2 + 45;

    // create a subscribe packet and verify encoded length
    err = Mqtt_Unsubscribe(h, &encbuf, &options);
    TEST_ASSERT_EQUAL(MQTT_ERR_OK, err);
    TEST_ASSERT_EQUAL(2 + rem, encbuf.len);
    TEST_ASSERT_EQUAL(rem, encbuf.data[1]);

    // create one that uses two bytes for remaining length
    MQTT_INIT_DATA_STATIC_BUF(encbuf, testBuf);
    topics[0].len = 201;
    rem = 2 + 2 + 201;
    err = Mqtt_Unsubscribe(h, &encbuf, &options);
    TEST_ASSERT_EQUAL(MQTT_ERR_OK, err);
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

TEST(Unsubscribe, SingleTopic)
{
    Mqtt_Error_t err;
    Mqtt_Data_t topics[1];

    MQTT_INIT_DATA_STR(topics[0], "topic");
    MQTT_INIT_UNSUBSCRIBE(options, 1, topics);
    err = Mqtt_Unsubscribe(h, &encbuf, &options);
    TEST_ASSERT_EQUAL(MQTT_ERR_OK, err);
    TEST_ASSERT_EQUAL(sizeof(unsubscribePacket0), encbuf.len);
    TEST_ASSERT_EQUAL_MEMORY(unsubscribePacket0, encbuf.data, sizeof(unsubscribePacket0));
}

TEST(Unsubscribe, MultiTopic)
{
    Mqtt_Error_t err;
    Mqtt_Data_t topics[2];

    MQTT_INIT_DATA_STR(topics[0], "topic");
    MQTT_INIT_DATA_STR(topics[1], "topic2");
    MQTT_INIT_UNSUBSCRIBE(options, 2, topics);
    err = Mqtt_Unsubscribe(h, &encbuf, &options);
    TEST_ASSERT_EQUAL(MQTT_ERR_OK, err);
    TEST_ASSERT_EQUAL(sizeof(unsubscribePacket1), encbuf.len);
    TEST_ASSERT_EQUAL_MEMORY(unsubscribePacket1, encbuf.data, sizeof(unsubscribePacket1));
}

TEST(Unsubscribe, PacketId)
{
    Mqtt_Error_t err;
    Mqtt_Data_t topics[1];
    Mqtt_Instance_t *pInst = h;

    // verify packet id is correct in packet
    MQTT_INIT_DATA_STR(topics[0], "topic");
    MQTT_INIT_UNSUBSCRIBE(options, 1, topics);
    err = Mqtt_Unsubscribe(h, &encbuf, &options);
    TEST_ASSERT_EQUAL(MQTT_ERR_OK, err);
    TEST_ASSERT_EQUAL(0, encbuf.data[2]);
    TEST_ASSERT_EQUAL(1, encbuf.data[3]);

    // send another packet, verify packet id increments
    MQTT_INIT_DATA_STATIC_BUF(encbuf, testBuf); // reset encbuf
    err = Mqtt_Unsubscribe(h, &encbuf, &options);
    TEST_ASSERT_EQUAL(MQTT_ERR_OK, err);
    TEST_ASSERT_EQUAL(0, encbuf.data[2]);
    TEST_ASSERT_EQUAL(2, encbuf.data[3]);

    // check rollover to upper byte
    MQTT_INIT_DATA_STATIC_BUF(encbuf, testBuf); // reset encbuf
    pInst->packetId = 255; // force boundary condition
    err = Mqtt_Unsubscribe(h, &encbuf, &options);
    TEST_ASSERT_EQUAL(MQTT_ERR_OK, err);
    TEST_ASSERT_EQUAL(1, encbuf.data[2]);
    TEST_ASSERT_EQUAL(0, encbuf.data[3]);

    MQTT_INIT_DATA_STATIC_BUF(encbuf, testBuf); // reset encbuf
    err = Mqtt_Unsubscribe(h, &encbuf, &options);
    TEST_ASSERT_EQUAL(MQTT_ERR_OK, err);
    TEST_ASSERT_EQUAL(1, encbuf.data[2]);
    TEST_ASSERT_EQUAL(1, encbuf.data[3]);

    // force packet id rollover, verify works
    MQTT_INIT_DATA_STATIC_BUF(encbuf, testBuf); // reset encbuf
    pInst->packetId = 65535; // force boundary condition
    err = Mqtt_Unsubscribe(h, &encbuf, &options);
    TEST_ASSERT_EQUAL(MQTT_ERR_OK, err);
    TEST_ASSERT_EQUAL(0, encbuf.data[2]);
    TEST_ASSERT_EQUAL(1, encbuf.data[3]);
}

TEST_GROUP_RUNNER(Unsubscribe)
{
    RUN_TEST_CASE(Unsubscribe, NullParms);
    RUN_TEST_CASE(Unsubscribe, BadParms);
    RUN_TEST_CASE(Unsubscribe, CalcLength);
    RUN_TEST_CASE(Unsubscribe, EncodedLength);
    RUN_TEST_CASE(Unsubscribe, SingleTopic);
    RUN_TEST_CASE(Unsubscribe, MultiTopic);
    RUN_TEST_CASE(Unsubscribe, PacketId);
}
