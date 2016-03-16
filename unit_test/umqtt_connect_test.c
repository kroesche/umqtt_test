
#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>

#include "unity_fixture.h"
#include "umqtt/umqtt.h"

TEST_GROUP(BuildConnect);

static umqtt_Handle_t h = NULL;
static umqtt_Connect_Options_t options;
static umqtt_Data_t encbuf = {0, NULL};
extern uint8_t testBuf[512];

extern void umqttTest_EventCb(umqtt_Handle_t, umqtt_Event_t, void *, void *);

TEST_SETUP(BuildConnect)
{
    static umqtt_Instance_t inst;
    h = umqtt_InitInstance(&inst, umqttTest_EventCb, NULL);
    options = (umqtt_Connect_Options_t)CONNECT_OPTIONS_INITIALIZER;
    UMQTT_INIT_DATA_STATIC_BUF(encbuf, testBuf);
}

TEST_TEAR_DOWN(BuildConnect)
{
    h = NULL;
}

TEST(BuildConnect, NullParms)
{
    umqtt_Error_t err = umqtt_BuildConnect(h, NULL, &options);
    TEST_ASSERT_EQUAL(UMQTT_ERR_PARM, err);
    err = umqtt_BuildConnect(h, &encbuf, NULL);
    TEST_ASSERT_EQUAL(UMQTT_ERR_PARM, err);
}

TEST(BuildConnect, BadParms)
{
    // test various combinations of bad inputs to connect
    TEST_IGNORE_MESSAGE("IMPLEMENT ME");
}

TEST(BuildConnect, CheckCalcLength)
{
    umqtt_Error_t err;
    uint16_t expectedLen;

    // check that length is calculated correctly
    UMQTT_INIT_DATA_STR(options.clientId, "client");
    err = umqtt_BuildConnect(NULL, &encbuf, &options);
    TEST_ASSERT_EQUAL(UMQTT_ERR_RET_LEN, err);
    // header is type + len + 10, plus 2 bytes len field plus client string length
    expectedLen = 2 + 10 + 2 + 6;
    TEST_ASSERT_EQUAL(expectedLen, encbuf.len);

    // add a will topic
    // will topic without message is an error
    UMQTT_INIT_DATA_STR(options.willTopic, "topic");
    expectedLen += 2 + 5;
    err = umqtt_BuildConnect(NULL, &encbuf, &options);
    TEST_ASSERT_EQUAL(UMQTT_ERR_PARM, err);
    //TEST_ASSERT_EQUAL(expectedLen, databuf.len); // doesnt matter if error

    // add a will message
    UMQTT_INIT_DATA_STR(options.willMessage, "message");
    expectedLen += 2 + 7;
    err = umqtt_BuildConnect(NULL, &encbuf, &options);
    TEST_ASSERT_EQUAL(UMQTT_ERR_RET_LEN, err);
    TEST_ASSERT_EQUAL(expectedLen, encbuf.len);

    // add a username
    UMQTT_INIT_DATA_STR(options.username, "username");
    expectedLen += 2 + 8;
    err = umqtt_BuildConnect(NULL, &encbuf, &options);
    TEST_ASSERT_EQUAL(UMQTT_ERR_RET_LEN, err);
    TEST_ASSERT_EQUAL(expectedLen, encbuf.len);

    // add a password
    UMQTT_INIT_DATA_STR(options.password, "password");
    expectedLen += 2 + 8;
    err = umqtt_BuildConnect(NULL, &encbuf, &options);
    TEST_ASSERT_EQUAL(UMQTT_ERR_RET_LEN, err);
    TEST_ASSERT_EQUAL(expectedLen, encbuf.len);

    // check that too small a buffer returns error
    // min buffer that is not parm error is 12
    encbuf.len = 12;
    err = umqtt_BuildConnect(h, &encbuf, &options);
    TEST_ASSERT_EQUAL(UMQTT_ERR_BUFSIZE, err);
}

TEST(BuildConnect, EncodedLength)
{
    umqtt_Error_t err;
    uint16_t rem; // expected remaining

    // create a dummy clientId of known length
    options.clientId.data = testBuf;
    options.clientId.len = 64;
    rem = 10 + 2 + options.clientId.len;

    // create connect packet, verify return buffer length
    err = umqtt_BuildConnect(h, &encbuf, &options);
    TEST_ASSERT_EQUAL(UMQTT_ERR_OK, err);
    TEST_ASSERT_EQUAL(2 + rem, encbuf.len);
    // check encoded length in encoded packet
    TEST_ASSERT_EQUAL(rem, encbuf.data[1]);

    // check it for corner case of 127
    // 1 byte of 127
    UMQTT_INIT_DATA_STATIC_BUF(encbuf, testBuf); // reset encbuf
    options.clientId.len = 115;
    rem = 10 + 2 + options.clientId.len;
    err = umqtt_BuildConnect(h, &encbuf, &options);
    TEST_ASSERT_EQUAL(UMQTT_ERR_OK, err);
    TEST_ASSERT_EQUAL(2 + rem, encbuf.len);
    TEST_ASSERT_EQUAL(rem, encbuf.data[1]);

    // check it for corner case of 128
    // 2 bytes of 0x80, 1
    UMQTT_INIT_DATA_STATIC_BUF(encbuf, testBuf);
    options.clientId.len = 116;
    rem = 10 + 2 + options.clientId.len;
    err = umqtt_BuildConnect(h, &encbuf, &options);
    TEST_ASSERT_EQUAL(UMQTT_ERR_OK, err);
    TEST_ASSERT_EQUAL(3 + rem, encbuf.len);
    TEST_ASSERT_EQUAL(0x80, encbuf.data[1]);
    TEST_ASSERT_EQUAL(1, encbuf.data[2]);

    // check it for 135
    // 2 bytes of 0x87, 1
    UMQTT_INIT_DATA_STATIC_BUF(encbuf, testBuf);
    options.clientId.len = 123;
    rem = 10 + 2 + options.clientId.len;
    err = umqtt_BuildConnect(h, &encbuf, &options);
    TEST_ASSERT_EQUAL(UMQTT_ERR_OK, err);
    TEST_ASSERT_EQUAL(3 + rem, encbuf.len);
    TEST_ASSERT_EQUAL(0x87, encbuf.data[1]);
    TEST_ASSERT_EQUAL(1, encbuf.data[2]);
}

// basic connect with only client id
static uint8_t connectPacket0[] =
{
    0x10, 19, // fixed header
    0, 4, 'M', 'Q', 'T', 'T', // protocol name
    4, 0, // protocol level and connect flags
    0, 30, // keepalive
    0, 7, 'p','a','c','k','e','t','0',
};

// connect with all strings and some flags
static uint8_t connectPacket1[] =
{
    0x10, 65,
    0, 4, 'M', 'Q', 'T', 'T', // protocol name
    4, 0xee, // protocol level, connect flags user/pass retain qos1 will clean
    300 / 256, 300 % 256, // keepalive
    0, 7, 'p','a','c','k','e','t','1',
    0, 10, 'w','i','l','l','/','t','o','p','i','c',
    0, 12, 'w','i','l','l','-','m','e','s','s','a','g','e',
    0, 8, 'u','s','e','r','n','a','m','e',
    0, 8, 'p','a','s','s','w','o','r','d',
};

TEST(BuildConnect, Basic)
{
    options.keepAlive = 30;
    UMQTT_INIT_DATA_STR(options.clientId, "packet0");
    umqtt_Error_t err = umqtt_BuildConnect(h, &encbuf, &options);
    TEST_ASSERT_EQUAL(UMQTT_ERR_OK, err);
    TEST_ASSERT_EQUAL(sizeof(connectPacket0), encbuf.len);
    TEST_ASSERT_EQUAL_MEMORY(connectPacket0, encbuf.data, sizeof(connectPacket0));
}

TEST(BuildConnect, Features)
{
    options.cleanSession = true;
    options.willRetain = true;
    options.qos = 1;
    options.keepAlive = 300;
    UMQTT_INIT_DATA_STR(options.clientId, "packet1");
    UMQTT_INIT_DATA_STR(options.willTopic, "will/topic");
    UMQTT_INIT_DATA_STR(options.willMessage, "will-message");
    UMQTT_INIT_DATA_STR(options.username, "username");
    UMQTT_INIT_DATA_STR(options.password, "password");

    umqtt_Error_t err = umqtt_BuildConnect(h, &encbuf, &options);
    TEST_ASSERT_EQUAL(UMQTT_ERR_OK, err);
    TEST_ASSERT_EQUAL(sizeof(connectPacket1), encbuf.len);
    TEST_ASSERT_EQUAL_MEMORY(connectPacket1, encbuf.data, sizeof(connectPacket1));
}

TEST_GROUP_RUNNER(BuildConnect)
{
    RUN_TEST_CASE(BuildConnect, NullParms);
    RUN_TEST_CASE(BuildConnect, BadParms);
    RUN_TEST_CASE(BuildConnect, CheckCalcLength);
    RUN_TEST_CASE(BuildConnect, EncodedLength);
    RUN_TEST_CASE(BuildConnect, Basic);
    RUN_TEST_CASE(BuildConnect, Features);
}
