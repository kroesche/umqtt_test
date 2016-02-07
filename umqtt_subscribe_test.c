
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>

#include "unity_fixture.h"
#include "umqtt/umqtt.h"

TEST_GROUP(Subscribe);

static Mqtt_Handle_t h = NULL;
static Mqtt_Subscribe_Options_t options;
static Mqtt_Data_t encbuf = {0, NULL};
extern uint8_t testBuf[512];

extern void MqttTest_EventCb(Mqtt_Handle_t, Mqtt_Event_t, void *);

TEST_SETUP(Subscribe)
{
    static Mqtt_Instance_t inst;
    h = Mqtt_InitInstance(&inst, MqttTest_EventCb);
    options = (Mqtt_Subscribe_Options_t)SUBSCRIBE_OPTIONS_INITIALIZER;
    MQTT_INIT_DATA_STATIC_BUF(encbuf, testBuf);
}

TEST_TEAR_DOWN(Subscribe)
{
    h = NULL;
}

TEST(Subscribe, NullParms)
{
    Mqtt_Error_t err = Mqtt_Subscribe(h, NULL, &options);
    TEST_ASSERT_EQUAL(MQTT_ERR_PARM, err);
    err = Mqtt_Subscribe(h, &encbuf, NULL);
    TEST_ASSERT_EQUAL(MQTT_ERR_PARM, err);
}

TEST(Subscribe, BadParms)
{
    // test various combinations of bad inputs to connect
    TEST_IGNORE_MESSAGE("IMPLEMENT ME");
}

TEST(Subscribe, CalcLength)
{
    Mqtt_Error_t err;
    uint16_t expectedLen;
    Mqtt_Data_t topics[2];
    uint8_t qoss[2];

    // create a single topic/qos packet, check length calculation
    MQTT_INIT_DATA_STR(topics[0], "topic");
    qoss[0] = 0;
    MQTT_INIT_SUBSCRIBE(options, 1, topics, qoss);
    // type + len + packetId + topic len + topic + qos
    expectedLen = 1 + 1 + 2 + 2 + 5 + 1;
    err = Mqtt_Subscribe(NULL, &encbuf, &options);
    TEST_ASSERT_EQUAL(MQTT_ERR_RET_LEN, err);
    TEST_ASSERT_EQUAL(expectedLen, encbuf.len);

    // add a second topic and qos
    MQTT_INIT_DATA_STR(topics[1], "topic2");
    qoss[1] = 1;
    MQTT_INIT_SUBSCRIBE(options, 2, topics, qoss);
    expectedLen += 2 + 6 + 1;
    err = Mqtt_Subscribe(NULL, &encbuf, &options);
    TEST_ASSERT_EQUAL(MQTT_ERR_RET_LEN, err);
    TEST_ASSERT_EQUAL(expectedLen, encbuf.len);

    // check that too small buffer returns error
    encbuf.len = expectedLen - 1;
    err = Mqtt_Subscribe(h, &encbuf, &options);
    TEST_ASSERT_EQUAL(MQTT_ERR_BUFSIZE, err);
}

TEST(Subscribe, EncodedLength)
{
    Mqtt_Error_t err;
    uint16_t rem;
    Mqtt_Data_t topics[1];
    uint8_t qoss[1];

    // create a dummy subscribe topic of known length
    topics[0].data = testBuf;
    topics[0].len = 45;
    qoss[0] = 0;
    MQTT_INIT_SUBSCRIBE(options, 1, topics, qoss);
    // remaining size is packetId + topic len + topic + qos
    rem = 2 + 2 + 45 + 1;

    // create a subscribe packet and verify encoded length
    err = Mqtt_Subscribe(h, &encbuf, &options);
    TEST_ASSERT_EQUAL(MQTT_ERR_OK, err);
    TEST_ASSERT_EQUAL(2 + rem, encbuf.len);
    TEST_ASSERT_EQUAL(rem, encbuf.data[1]);

    // create one that uses two bytes for remaining length
    MQTT_INIT_DATA_STATIC_BUF(encbuf, testBuf);
    topics[0].len = 200;
    rem = 2 + 2 + 200 + 1;
    err = Mqtt_Subscribe(h, &encbuf, &options);
    TEST_ASSERT_EQUAL(MQTT_ERR_OK, err);
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

TEST(Subscribe, SingleTopic)
{
    Mqtt_Error_t err;
    Mqtt_Data_t topics[1];
    uint8_t qoss[1];

    MQTT_INIT_DATA_STR(topics[0], "topic");
    qoss[0] = 0;
    MQTT_INIT_SUBSCRIBE(options, 1, topics, qoss);
    err = Mqtt_Subscribe(h, &encbuf, &options);
    TEST_ASSERT_EQUAL(MQTT_ERR_OK, err);
    TEST_ASSERT_EQUAL(sizeof(subscribePacket0), encbuf.len);
    TEST_ASSERT_EQUAL_MEMORY(subscribePacket0, encbuf.data, sizeof(subscribePacket0));
}

TEST(Subscribe, MultiTopic)
{
    Mqtt_Error_t err;
    Mqtt_Data_t topics[2];
    uint8_t qoss[2];

    MQTT_INIT_DATA_STR(topics[0], "topic");
    qoss[0] = 0;
    MQTT_INIT_DATA_STR(topics[1], "topic2");
    qoss[1] = 1;
    MQTT_INIT_SUBSCRIBE(options, 2, topics, qoss);
    err = Mqtt_Subscribe(h, &encbuf, &options);
    TEST_ASSERT_EQUAL(MQTT_ERR_OK, err);
    TEST_ASSERT_EQUAL(sizeof(subscribePacket1), encbuf.len);
    TEST_ASSERT_EQUAL_MEMORY(subscribePacket1, encbuf.data, sizeof(subscribePacket1));
}

TEST(Subscribe, PacketId)
{
    Mqtt_Error_t err;
    Mqtt_Data_t topics[1];
    uint8_t qoss[1];
    Mqtt_Instance_t *pInst = h;

    // verify packet id is correct in packet
    MQTT_INIT_DATA_STR(topics[0], "topic");
    qoss[0] = 0;
    MQTT_INIT_SUBSCRIBE(options, 1, topics, qoss);
    err = Mqtt_Subscribe(h, &encbuf, &options);
    TEST_ASSERT_EQUAL(MQTT_ERR_OK, err);
    TEST_ASSERT_EQUAL(0, encbuf.data[2]);
    TEST_ASSERT_EQUAL(1, encbuf.data[3]);

    // send another packet, verify packet id increments
    MQTT_INIT_DATA_STATIC_BUF(encbuf, testBuf); // reset encbuf
    err = Mqtt_Subscribe(h, &encbuf, &options);
    TEST_ASSERT_EQUAL(MQTT_ERR_OK, err);
    TEST_ASSERT_EQUAL(0, encbuf.data[2]);
    TEST_ASSERT_EQUAL(2, encbuf.data[3]);

    // check rollover to upper byte
    MQTT_INIT_DATA_STATIC_BUF(encbuf, testBuf); // reset encbuf
    pInst->packetId = 255; // force boundary condition
    err = Mqtt_Subscribe(h, &encbuf, &options);
    TEST_ASSERT_EQUAL(MQTT_ERR_OK, err);
    TEST_ASSERT_EQUAL(1, encbuf.data[2]);
    TEST_ASSERT_EQUAL(0, encbuf.data[3]);

    MQTT_INIT_DATA_STATIC_BUF(encbuf, testBuf); // reset encbuf
    err = Mqtt_Subscribe(h, &encbuf, &options);
    TEST_ASSERT_EQUAL(MQTT_ERR_OK, err);
    TEST_ASSERT_EQUAL(1, encbuf.data[2]);
    TEST_ASSERT_EQUAL(1, encbuf.data[3]);

    // force packet id rollover, verify works
    MQTT_INIT_DATA_STATIC_BUF(encbuf, testBuf); // reset encbuf
    pInst->packetId = 65535; // force boundary condition
    err = Mqtt_Subscribe(h, &encbuf, &options);
    TEST_ASSERT_EQUAL(MQTT_ERR_OK, err);
    TEST_ASSERT_EQUAL(0, encbuf.data[2]);
    TEST_ASSERT_EQUAL(1, encbuf.data[3]);
}

TEST_GROUP_RUNNER(Subscribe)
{
    RUN_TEST_CASE(Subscribe, NullParms);
    RUN_TEST_CASE(Subscribe, BadParms);
    RUN_TEST_CASE(Subscribe, CalcLength);
    RUN_TEST_CASE(Subscribe, EncodedLength);
    RUN_TEST_CASE(Subscribe, SingleTopic);
    RUN_TEST_CASE(Subscribe, MultiTopic);
    RUN_TEST_CASE(Subscribe, PacketId);
}
