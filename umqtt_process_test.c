
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>

#include "unity_fixture.h"
#include "umqtt/umqtt.h"

TEST_GROUP(Decode);

static umqtt_Handle_t h = NULL;
static umqtt_Data_t encbuf = {0, NULL};
extern uint8_t testBuf[512];

static umqtt_Handle_t cb_h;
static umqtt_Event_t cb_event;
static void *cb_pInfo;
static uint32_t cb_infosize;
static bool cb_wasCalled = false;
static umqtt_Data_t replybuf = {0, NULL};

static void
Decode_EventCb(umqtt_Handle_t handle, umqtt_Event_t event, void *pInfo)
{
    TEST_ASSERT_EQUAL(cb_h, handle);
    if (event == UMQTT_EVENT_REPLY)
    {
        TEST_ASSERT_NOT_NULL(replybuf.data);
        TEST_ASSERT_NOT_EQUAL(replybuf.len, 0);
        umqtt_Data_t *puback = pInfo;
        TEST_ASSERT_EQUAL(replybuf.len, puback->len);
        TEST_ASSERT_EQUAL_MEMORY(replybuf.data, puback->data, replybuf.len);
        replybuf.data = NULL;
        replybuf.len = 0;
    }
    else
    {
        TEST_ASSERT_EQUAL(cb_event, event);
        if (cb_pInfo)
        {
            TEST_ASSERT_NOT_NULL(pInfo);
        }
        if (event == UMQTT_EVENT_PUBLISH)
        {
            umqtt_Publish_Options_t *pExpected = cb_pInfo;
            umqtt_Publish_Options_t *pActual = pInfo;
            TEST_ASSERT_EQUAL(pExpected->dup, pActual->dup);
            TEST_ASSERT_EQUAL(pExpected->qos, pActual->qos);
            TEST_ASSERT_EQUAL(pExpected->retain, pActual->retain);
            TEST_ASSERT_EQUAL(pExpected->topic.len, pActual->topic.len);
            TEST_ASSERT_EQUAL_MEMORY(pExpected->topic.data, pActual->topic.data,
                                     pExpected->topic.len);
            if (pExpected->message.len)
            {
                TEST_ASSERT_EQUAL(pExpected->message.len, pActual->message.len);
                TEST_ASSERT_EQUAL_MEMORY(pExpected->message.data, pActual->message.data,
                                         pExpected->message.len);
            }
        }
        else if (event == UMQTT_EVENT_SUBACK)
        {
            umqtt_Data_t *pExpected = cb_pInfo;
            umqtt_Data_t *pActual = pInfo;
            TEST_ASSERT_EQUAL(pExpected->len, pActual->len);
            TEST_ASSERT_EQUAL_MEMORY(pExpected->data, pActual->data, pExpected->len);
        }
        else if (pInfo)
        {
            TEST_ASSERT_EQUAL_MEMORY(cb_pInfo, pInfo, cb_infosize);
        }
    }
    cb_wasCalled = true;
}

TEST_SETUP(Decode)
{
    static umqtt_Instance_t inst;
    h = umqtt_InitInstance(&inst, Decode_EventCb);
    UMQTT_INIT_DATA_STATIC_BUF(encbuf, testBuf);
    cb_h = NULL;
    cb_event = 0;
    cb_pInfo = NULL;
    cb_wasCalled = false;
    replybuf.len = 0;
    replybuf.data = NULL;
}

TEST_TEAR_DOWN(Decode)
{
    h = NULL;
}

TEST(Decode, NullParms)
{
    umqtt_Error_t err;
    err = umqtt_DecodePacket(NULL, &encbuf);
    TEST_ASSERT_EQUAL(UMQTT_ERR_PARM, err);
    err = umqtt_DecodePacket(h, NULL);
    TEST_ASSERT_EQUAL(UMQTT_ERR_PARM, err);
}

// packet with bad type (invalid)
TEST(Decode, BadPacket)
{
    umqtt_Error_t err;

    encbuf.data[0] = 0xFF;
    encbuf.len = 2;
    err = umqtt_DecodePacket(h, &encbuf);
    TEST_ASSERT_EQUAL(UMQTT_ERR_PACKET_ERROR, err);
    TEST_IGNORE_MESSAGE("NEED TO ADD PACKET LENGTH 0 TEST");
}

// connack
TEST(Decode, Connack)
{
    umqtt_Error_t err;
    umqtt_Connect_Result_t res;

    // packet with bad length field
    encbuf.data[0] = 0x20;
    encbuf.data[1] = 4; // invalid length field
    encbuf.data[2] = 0; // flags
    encbuf.data[3] = 0; // rc
    encbuf.len = 4;
    cb_wasCalled = false;
    cb_h = (umqtt_Handle_t)1; // bread crumb if failure
    err = umqtt_DecodePacket(h, &encbuf);
    TEST_ASSERT_EQUAL(UMQTT_ERR_PACKET_ERROR, err);
    TEST_ASSERT_FALSE(cb_wasCalled);

    // bad packet length
    encbuf.data[1] = 2; // valid length field
    encbuf.len = 5; // bad overall length
    cb_wasCalled = false;
    cb_h = (umqtt_Handle_t)2; // bread crumb if failure
    err = umqtt_DecodePacket(h, &encbuf);
    TEST_ASSERT_EQUAL(UMQTT_ERR_PACKET_ERROR, err);
    TEST_ASSERT_FALSE(cb_wasCalled);

    // valid nominal packet
    encbuf.len = 4;
    // expected callback values
    cb_h = h;
    cb_event = UMQTT_EVENT_CONNECTED;
    res.returnCode = 0;
    res.sessionPresent = false;
    cb_pInfo = &res;
    cb_infosize = sizeof(umqtt_Connect_Result_t);
    cb_wasCalled = false;
    err = umqtt_DecodePacket(h, &encbuf);
    TEST_ASSERT_EQUAL(UMQTT_ERR_OK, err);
    TEST_ASSERT_TRUE(cb_wasCalled);

    // set the options and verify they are decoded
    encbuf.data[2] = 1; // session present
    encbuf.data[3] = 3; // rc
    res.returnCode = 3;
    res.sessionPresent = true;
    cb_pInfo = &res;
    cb_infosize = sizeof(umqtt_Connect_Result_t);
    cb_wasCalled = false;
    err = umqtt_DecodePacket(h, &encbuf);
    TEST_ASSERT_EQUAL(UMQTT_ERR_OK, err);
    TEST_ASSERT_TRUE(cb_wasCalled);
}

// possible publish packet errors
// - remaining mismatch with packet length
// - bad qos

// remaining count mismatch with packet length
static uint8_t publishPacket0[] =
{
    0x30, 7, // fixed header
    0, 5, 't','o','p','i','c',
    0,
};
// bad qos
static uint8_t publishPacket1[] =
{
    0x36, 7, // fixed header
    0, 5, 't','o','p','i','c',
};
// nominal simple
static uint8_t publishPacket2[] =
{
    0x30, 7, // fixed header
    0, 5, 't','o','p','i','c',
};
// features: topic with message and flags
static uint8_t publishPacket3[] =
{
    0x3B, 18, // dup, qos 1, retain
    0, 5, 't','o','p','i','c',
    0, 1, // packet id
    0, 7, 'm','e','s','s','a','g','e',
};

static uint8_t pubackPacket[] =
{
    0x40, 2, 0, 1,
};

// publish
TEST(Decode, Publish)
{
    umqtt_Error_t err;
    umqtt_Publish_Options_t opt;

    // verify bad packet is detected
    cb_h = (umqtt_Handle_t)1; // bread crumb
    cb_wasCalled = false;
    UMQTT_INIT_DATA_STATIC_BUF(encbuf, publishPacket0);
    err = umqtt_DecodePacket(h, &encbuf);
    TEST_ASSERT_EQUAL(UMQTT_ERR_PACKET_ERROR, err);
    TEST_ASSERT_FALSE(cb_wasCalled);

    // verify bad packet is detected
    cb_h = (umqtt_Handle_t)2; // bread crumb
    cb_wasCalled = false;
    UMQTT_INIT_DATA_STATIC_BUF(encbuf, publishPacket1);
    err = umqtt_DecodePacket(h, &encbuf);
    TEST_ASSERT_EQUAL(UMQTT_ERR_PACKET_ERROR, err);
    TEST_ASSERT_FALSE(cb_wasCalled);

    // simple good packet
    opt.dup = false;
    opt.qos = 0;
    opt.retain = false;
    opt.topic.data = (uint8_t *)"topic";
    opt.topic.len = 5;
    opt.message.data = NULL;
    opt.message.len = 0;
    cb_h = h;
    cb_event = UMQTT_EVENT_PUBLISH;
    cb_wasCalled = false;
    cb_pInfo = &opt;
    cb_infosize = sizeof(opt);
    UMQTT_INIT_DATA_STATIC_BUF(encbuf, publishPacket2);
    err = umqtt_DecodePacket(h, &encbuf);
    TEST_ASSERT_EQUAL(UMQTT_ERR_OK, err);
    TEST_ASSERT_TRUE(cb_wasCalled);

    // publish message with all features
    opt.dup = true;
    opt.qos = 1;
    opt.retain = true;
    opt.topic.data = (uint8_t *)"topic";
    opt.topic.len = 5;
    opt.message.data = (uint8_t *)"message";
    opt.message.len = 7;
    cb_h = h;
    cb_event = UMQTT_EVENT_PUBLISH;
    cb_wasCalled = false;
    cb_pInfo = &opt;
    cb_infosize = sizeof(opt);
    UMQTT_INIT_DATA_STATIC_BUF(replybuf, pubackPacket);
    UMQTT_INIT_DATA_STATIC_BUF(encbuf, publishPacket3);
    err = umqtt_DecodePacket(h, &encbuf);
    TEST_ASSERT_EQUAL(UMQTT_ERR_OK, err);
    TEST_ASSERT_TRUE(cb_wasCalled);
    // confirm that puback was received
    TEST_ASSERT_EQUAL(replybuf.len, 0);
    TEST_ASSERT_NULL(replybuf.data);
}

static uint8_t pubackBadPacket[] =
{
    0x40, 3, 0, 1,
};

// puback
TEST(Decode, Puback)
{
    umqtt_Error_t err;

    // verify bad packet is detected
    cb_h = (umqtt_Handle_t)1; // bread crumb
    cb_wasCalled = false;
    UMQTT_INIT_DATA_STATIC_BUF(encbuf, pubackBadPacket);
    err = umqtt_DecodePacket(h, &encbuf);
    TEST_ASSERT_EQUAL(UMQTT_ERR_PACKET_ERROR, err);
    TEST_ASSERT_FALSE(cb_wasCalled);

    // good puback packet
    cb_h = h;
    cb_event = UMQTT_EVENT_PUBACK;
    cb_wasCalled = false;
    cb_pInfo = NULL;
    cb_infosize = 0;
    UMQTT_INIT_DATA_STATIC_BUF(encbuf, pubackPacket);
    err = umqtt_DecodePacket(h, &encbuf);
    TEST_ASSERT_EQUAL(UMQTT_ERR_OK, err);
    TEST_ASSERT_TRUE(cb_wasCalled);
}

static uint8_t subackPacket[] =
{
    0x90, 5, 0, 1, 1, 0, 2,
};
static uint8_t subackBadPacket[] =
{
    0x90, 5, 0, 2, 0, 1, 2, 2,
};

// suback
TEST(Decode, Suback)
{
    umqtt_Error_t err;
    // verify bad packet is detected
    cb_h = (umqtt_Handle_t)1; // bread crumb
    cb_wasCalled = false;
    UMQTT_INIT_DATA_STATIC_BUF(encbuf, subackBadPacket);
    err = umqtt_DecodePacket(h, &encbuf);
    TEST_ASSERT_EQUAL(UMQTT_ERR_PACKET_ERROR, err);
    TEST_ASSERT_FALSE(cb_wasCalled);

    // good suback packet
    cb_h = h;
    cb_event = UMQTT_EVENT_SUBACK;
    cb_wasCalled = false;
    uint8_t rc[3] = { 1, 0, 2 };
    UMQTT_INIT_DATA_STATIC_BUF(replybuf, rc);
    cb_pInfo = &replybuf;
    UMQTT_INIT_DATA_STATIC_BUF(encbuf, subackPacket);
    err = umqtt_DecodePacket(h, &encbuf);
    TEST_ASSERT_EQUAL(UMQTT_ERR_OK, err);
    TEST_ASSERT_TRUE(cb_wasCalled);
}

static uint8_t unsubackBadPacket[] = { 0xB0, 2, 0 };
static uint8_t unsubackPacket[] = { 0xB0, 2, 0, 1 };

// unsuback
TEST(Decode, Unsuback)
{
    umqtt_Error_t err;

    // verify bad packet is detected
    cb_h = (umqtt_Handle_t)1; // bread crumb
    cb_wasCalled = false;
    UMQTT_INIT_DATA_STATIC_BUF(encbuf, unsubackBadPacket);
    err = umqtt_DecodePacket(h, &encbuf);
    TEST_ASSERT_EQUAL(UMQTT_ERR_PACKET_ERROR, err);
    TEST_ASSERT_FALSE(cb_wasCalled);

    // good unsuback packet
    cb_h = h;
    cb_event = UMQTT_EVENT_UNSUBACK;
    cb_wasCalled = false;
    cb_pInfo = NULL;
    cb_infosize = 0;
    UMQTT_INIT_DATA_STATIC_BUF(encbuf, unsubackPacket);
    err = umqtt_DecodePacket(h, &encbuf);
    TEST_ASSERT_EQUAL(UMQTT_ERR_OK, err);
    TEST_ASSERT_TRUE(cb_wasCalled);
}

static uint8_t pingrespBadPacket[] = { 0xD0, 1 };
static uint8_t pingrespPacket[] = { 0xD0, 0 };

// pingresp
TEST(Decode, Pingresp)
{
    umqtt_Error_t err;

    // verify bad packet is detected
    cb_h = (umqtt_Handle_t)1; // bread crumb
    cb_wasCalled = false;
    UMQTT_INIT_DATA_STATIC_BUF(encbuf, pingrespBadPacket);
    err = umqtt_DecodePacket(h, &encbuf);
    TEST_ASSERT_EQUAL(UMQTT_ERR_PACKET_ERROR, err);
    TEST_ASSERT_FALSE(cb_wasCalled);

    // good pingresp packet
    cb_h = h;
    cb_event = UMQTT_EVENT_PINGRESP;
    cb_wasCalled = false;
    cb_pInfo = NULL;
    cb_infosize = 0;
    UMQTT_INIT_DATA_STATIC_BUF(encbuf, pingrespPacket);
    err = umqtt_DecodePacket(h, &encbuf);
    TEST_ASSERT_EQUAL(UMQTT_ERR_OK, err);
    TEST_ASSERT_TRUE(cb_wasCalled);
}

TEST_GROUP_RUNNER(Decode)
{
    RUN_TEST_CASE(Decode, NullParms);
    RUN_TEST_CASE(Decode, BadPacket);
    RUN_TEST_CASE(Decode, Connack);
    RUN_TEST_CASE(Decode, Publish);
    RUN_TEST_CASE(Decode, Puback);
    RUN_TEST_CASE(Decode, Suback);
    RUN_TEST_CASE(Decode, Unsuback);
    RUN_TEST_CASE(Decode, Pingresp);
}

// bad parms

