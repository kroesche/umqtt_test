
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>

#include "unity_fixture.h"
#include "umqtt/umqtt.h"

// TODO: put const on constancts and where it makes sense

TEST_GROUP(Publish);

static Mqtt_Handle_t h = NULL;
static Mqtt_Publish_Options_t options;
static Mqtt_Data_t encbuf = {0, NULL };
extern uint8_t testBuf[512];

extern void MqttTest_EventCb(Mqtt_Handle_t, Mqtt_Event_t, void *);

TEST_SETUP(Publish)
{
    static Mqtt_Instance_t inst;
    h = Mqtt_InitInstance(&inst, MqttTest_EventCb);
    options = (Mqtt_Publish_Options_t)PUBLISH_OPTIONS_INITIALIZER;
    MQTT_INIT_DATA_STATIC_BUF(encbuf, testBuf);
}

TEST_TEAR_DOWN(Publish)
{
    h = NULL;
}

TEST(Publish, NullParms)
{
    Mqtt_Error_t err;
    err = Mqtt_Publish(h, NULL, &options);
    TEST_ASSERT_EQUAL(MQTT_ERR_PARM, err);
    err = Mqtt_Publish(h, &encbuf, NULL);
    TEST_ASSERT_EQUAL(MQTT_ERR_PARM, err);
}

TEST(Publish, BadParms)
{
    // test various combinations of bad inputs to connect
    TEST_IGNORE_MESSAGE("IMPLEMENT ME");
}

TEST(Publish, CalcLength)
{
    Mqtt_Error_t err;
    uint16_t expectedLen;

    // topic only
    MQTT_INIT_DATA_STR(options.topic, "topic");
    expectedLen = 2 + 2 + 5;
    err = Mqtt_Publish(NULL, &encbuf, &options);
    TEST_ASSERT_EQUAL(MQTT_ERR_RET_LEN, err);
    TEST_ASSERT_EQUAL(expectedLen, encbuf.len);

    // topic and payload
    MQTT_INIT_DATA_STR(options.message, "message");
    expectedLen += 2 + 7;
    err = Mqtt_Publish(NULL, &encbuf, &options);
    TEST_ASSERT_EQUAL(MQTT_ERR_RET_LEN, err);
    TEST_ASSERT_EQUAL(expectedLen, encbuf.len);

    // add qos which causes packetId field to be added
    options.qos = 1;
    expectedLen += 2;
    err = Mqtt_Publish(NULL, &encbuf, &options);
    TEST_ASSERT_EQUAL(MQTT_ERR_RET_LEN, err);
    TEST_ASSERT_EQUAL(expectedLen, encbuf.len);

    // verify that buffer too small returns error
    // min buffer for publish is 5
    encbuf.len = expectedLen - 1;
    err = Mqtt_Publish(h, &encbuf, &options);
    TEST_ASSERT_EQUAL(MQTT_ERR_BUFSIZE, err);
}

TEST(Publish, EncodedLength)
{
    Mqtt_Error_t err;
    uint16_t rem;

    // create dummy topic of known length
    options.topic.data = testBuf;
    options.topic.len = 65;
    rem = 2 + options.topic.len;

    // create a publish packet and verify the encoded length
    err = Mqtt_Publish(h, &encbuf, &options);
    TEST_ASSERT_EQUAL(MQTT_ERR_OK, err);
    TEST_ASSERT_EQUAL(2 + rem, encbuf.len);
    TEST_ASSERT_EQUAL(rem, encbuf.data[1]);

    // verify 127 remaining length
    MQTT_INIT_DATA_STATIC_BUF(encbuf, testBuf); // reset encbuf
    options.topic.len = 125;
    rem = 2 + options.topic.len;
    err = Mqtt_Publish(h, &encbuf, &options);
    TEST_ASSERT_EQUAL(MQTT_ERR_OK, err);
    TEST_ASSERT_EQUAL(2 + rem, encbuf.len);
    TEST_ASSERT_EQUAL(rem, encbuf.data[1]);

    // verify 128 remaining length
    MQTT_INIT_DATA_STATIC_BUF(encbuf, testBuf); // reset encbuf
    options.topic.len = 126;
    rem = 2 + options.topic.len;
    err = Mqtt_Publish(h, &encbuf, &options);
    TEST_ASSERT_EQUAL(MQTT_ERR_OK, err);
    TEST_ASSERT_EQUAL(3 + rem, encbuf.len);
    TEST_ASSERT_EQUAL(0x80, encbuf.data[1]);
    TEST_ASSERT_EQUAL(1, encbuf.data[2]);

    // verify 141 remaining length
    MQTT_INIT_DATA_STATIC_BUF(encbuf, testBuf); // reset encbuf
    options.topic.len = 139;
    rem = 2 + options.topic.len;
    err = Mqtt_Publish(h, &encbuf, &options);
    TEST_ASSERT_EQUAL(MQTT_ERR_OK, err);
    TEST_ASSERT_EQUAL(3 + rem, encbuf.len);
    TEST_ASSERT_EQUAL(0x8D, encbuf.data[1]);
    TEST_ASSERT_EQUAL(1, encbuf.data[2]);
}

static uint8_t publishPacket0[] =
{
    0x30, 7, // fixed header
    0, 5, 't','o','p','i','c',
};

static uint8_t publishPacket1[] =
{
    0x3B, 18, // dup, qos 1, retain
    0, 5, 't','o','p','i','c',
    0, 1, // packet id
    0, 7, 'm','e','s','s','a','g','e',
};

TEST(Publish, Basic)
{
    Mqtt_Error_t err;

    MQTT_INIT_DATA_STR(options.topic, "topic");
    err = Mqtt_Publish(h, &encbuf, &options);
    TEST_ASSERT_EQUAL(MQTT_ERR_OK, err);
    TEST_ASSERT_EQUAL(sizeof(publishPacket0), encbuf.len);
    TEST_ASSERT_EQUAL_MEMORY(publishPacket0, encbuf.data, sizeof(publishPacket0));
}

TEST(Publish, Features)
{
    Mqtt_Error_t err;

    options.dup = true;
    options.retain = true;
    options.qos = true;
    MQTT_INIT_DATA_STR(options.topic, "topic");
    MQTT_INIT_DATA_STR(options.message, "message");
    err = Mqtt_Publish(h, &encbuf, &options);
    TEST_ASSERT_EQUAL(MQTT_ERR_OK, err);
    TEST_ASSERT_EQUAL(sizeof(publishPacket1), encbuf.len);
    TEST_ASSERT_EQUAL_MEMORY(publishPacket1, encbuf.data, sizeof(publishPacket1));
}

TEST(Publish, PacketId)
{
    Mqtt_Error_t err;
    Mqtt_Instance_t *pInst = h;
    options.qos = 1; // forces packet Id to be in packet

    // verify packet id is correct in packet
    MQTT_INIT_DATA_STR(options.topic, "topic");
    err = Mqtt_Publish(h, &encbuf, &options);
    TEST_ASSERT_EQUAL(MQTT_ERR_OK, err);
    TEST_ASSERT_EQUAL(0, encbuf.data[9]);
    TEST_ASSERT_EQUAL(1, encbuf.data[10]);

    // send another packet, verify packet id increments
    MQTT_INIT_DATA_STATIC_BUF(encbuf, testBuf); // reset encbuf
    err = Mqtt_Publish(h, &encbuf, &options);
    TEST_ASSERT_EQUAL(MQTT_ERR_OK, err);
    TEST_ASSERT_EQUAL(0, encbuf.data[9]);
    TEST_ASSERT_EQUAL(2, encbuf.data[10]);

    // check rollover to upper byte
    MQTT_INIT_DATA_STATIC_BUF(encbuf, testBuf); // reset encbuf
    pInst->packetId = 255; // force boundary condition
    err = Mqtt_Publish(h, &encbuf, &options);
    TEST_ASSERT_EQUAL(MQTT_ERR_OK, err);
    TEST_ASSERT_EQUAL(1, encbuf.data[9]);
    TEST_ASSERT_EQUAL(0, encbuf.data[10]);

    MQTT_INIT_DATA_STATIC_BUF(encbuf, testBuf); // reset encbuf
    err = Mqtt_Publish(h, &encbuf, &options);
    TEST_ASSERT_EQUAL(MQTT_ERR_OK, err);
    TEST_ASSERT_EQUAL(1, encbuf.data[9]);
    TEST_ASSERT_EQUAL(1, encbuf.data[10]);

    // force packet id rollover, verify works
    MQTT_INIT_DATA_STATIC_BUF(encbuf, testBuf); // reset encbuf
    pInst->packetId = 65535; // force boundary condition
    err = Mqtt_Publish(h, &encbuf, &options);
    TEST_ASSERT_EQUAL(MQTT_ERR_OK, err);
    TEST_ASSERT_EQUAL(0, encbuf.data[9]);
    TEST_ASSERT_EQUAL(1, encbuf.data[10]);
}

TEST_GROUP_RUNNER(Publish)
{
    RUN_TEST_CASE(Publish, NullParms);
    RUN_TEST_CASE(Publish, BadParms);
    RUN_TEST_CASE(Publish, CalcLength);
    RUN_TEST_CASE(Publish, EncodedLength);
    RUN_TEST_CASE(Publish, Basic);
    RUN_TEST_CASE(Publish, Features);
    RUN_TEST_CASE(Publish, PacketId);
}
