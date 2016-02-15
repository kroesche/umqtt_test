
#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>

#include "unity_fixture.h"
#include "umqtt/umqtt.h"

extern int ConnectToServer(void);
extern int WriteToServer(int socket, const uint8_t *buf, uint32_t len);
extern int ReadFromServer(int socket, const uint8_t *buf, uint32_t len);
extern void DisconnectServer(int socket);

static int sock;
static umqtt_Handle_t h = NULL;
static umqtt_Connect_Options_t options;
static umqtt_Data_t encbuf = {0, NULL};
static uint8_t testBuf[512];
static uint8_t cbBuf[256];
static uint32_t cbBufLen = 0;
static umqtt_Event_t cbEvent;
static uint8_t disconnectPacket[] = DISCONNECT_PACKET_INITIALIZER;

static void
umqttTest_EventCb(umqtt_Handle_t h, umqtt_Event_t event, void *pInfo)
{
    cbEvent = event;
    switch (event)
    {
        case UMQTT_EVENT_CONNECTED:
        {
            memcpy(cbBuf, pInfo, sizeof(umqtt_Connect_Result_t));
            break;
        }
        case UMQTT_EVENT_PINGRESP:
        {
            break;
        }
        case UMQTT_EVENT_SUBACK:
        {
            umqtt_Data_t *pDat = pInfo;
            memcpy(cbBuf, pDat->data, pDat->len);
            cbBufLen = pDat->len;
            break;
        }
        case UMQTT_EVENT_PUBLISH:
        {
            umqtt_Publish_Options_t *pDat = pInfo;
            memcpy(cbBuf, pDat, sizeof(umqtt_Publish_Options_t));
            memcpy(&cbBuf[64], pDat->topic.data, pDat->topic.len);
            memcpy(&cbBuf[128], pDat->message.data, pDat->message.len);
            break;
        }
        default:
        {
            TEST_FAIL_MESSAGE("unexpected event");
            break;
        }
    }
}

TEST_GROUP(Connect);

TEST_SETUP(Connect)
{
    static umqtt_Instance_t inst;
    h = umqtt_InitInstance(&inst, umqttTest_EventCb);
    options = CONNECT_OPTIONS_INITIALIZER;
    UMQTT_INIT_DATA_STATIC_BUF(encbuf, testBuf);
    cbEvent = 0;
    cbBufLen = 0;

    sock = ConnectToServer();
    if (sock <= 0)
    {
        TEST_FAIL_MESSAGE("Test setup failed (connect)\n");
    }
}

TEST_TEAR_DOWN(Connect)
{
    h = NULL;
    cbEvent = 0;
    memset(cbBuf, 0, sizeof(cbBuf));
    DisconnectServer(sock);
}

static void
SimpleConnect(void)
{
    // build a connect packet
    options.keepAlive = 30;
    options.cleanSession = true;
    UMQTT_INIT_DATA_STR(options.clientId, "umqtt0");
    umqtt_Error_t err = umqtt_BuildConnect(h, &encbuf, &options);
    TEST_ASSERT_EQUAL(UMQTT_ERR_OK, err);

    // send packet to server and verify sent ok
    int cnt = WriteToServer(sock, encbuf.data, encbuf.len);
    TEST_ASSERT_EQUAL(encbuf.len, cnt);

    // attempt read from server
    cnt = ReadFromServer(sock, testBuf, sizeof(testBuf));
    TEST_ASSERT_NOT_EQUAL(0, cnt);

    // decode the result
    encbuf.data = testBuf;
    encbuf.len = cnt;
    err = umqtt_DecodePacket(h, &encbuf);

    // verify expected result
    TEST_ASSERT_EQUAL(UMQTT_EVENT_CONNECTED, cbEvent);
    umqtt_Connect_Result_t *pRes = (umqtt_Connect_Result_t*)cbBuf;
    TEST_ASSERT_EQUAL(0, pRes->returnCode);
    TEST_ASSERT_FALSE(pRes->sessionPresent);

    // reset encbuf and callbacks
    UMQTT_INIT_DATA_STATIC_BUF(encbuf, testBuf);
    cbEvent = 0;
    cbBufLen = 0;
}

static void
ComplexConnect(void)
{
    // build a connect packet
    options.keepAlive = 30;
    options.cleanSession = false;
    options.willRetain = true;
    options.qos = 1;
    UMQTT_INIT_DATA_STR(options.clientId, "umqtt0");
    UMQTT_INIT_DATA_STR(options.willTopic, "umqtt/willTopic");
    UMQTT_INIT_DATA_STR(options.willMessage, "willMessage");
    UMQTT_INIT_DATA_STR(options.username, "username");
    UMQTT_INIT_DATA_STR(options.password, "password");
    umqtt_Error_t err = umqtt_BuildConnect(h, &encbuf, &options);
    TEST_ASSERT_EQUAL(UMQTT_ERR_OK, err);

    // send packet to server and verify sent ok
    int cnt = WriteToServer(sock, encbuf.data, encbuf.len);
    TEST_ASSERT_EQUAL(encbuf.len, cnt);

    // attempt read from server
    cnt = ReadFromServer(sock, testBuf, sizeof(testBuf));
    TEST_ASSERT_NOT_EQUAL(0, cnt);

    // decode the result
    encbuf.data = testBuf;
    encbuf.len = cnt;
    err = umqtt_DecodePacket(h, &encbuf);
    TEST_ASSERT_EQUAL(UMQTT_ERR_OK, err);

    // verify expected result
    TEST_ASSERT_EQUAL(UMQTT_EVENT_CONNECTED, cbEvent);
    umqtt_Connect_Result_t *pRes = (umqtt_Connect_Result_t*)cbBuf;
    TEST_ASSERT_EQUAL(0, pRes->returnCode);
    TEST_ASSERT_FALSE(pRes->sessionPresent);

    UMQTT_INIT_DATA_STATIC_BUF(encbuf, testBuf);
    cbEvent = 0;
    cbBufLen = 0;
}

static void
Disconnect(void)
{
    // send a disconnect packet
    int cnt = WriteToServer(sock, disconnectPacket, sizeof(disconnectPacket));
    TEST_ASSERT_EQUAL(sizeof(disconnectPacket), cnt);
}

TEST(Connect, SimpleConnect)
{
    SimpleConnect();
    Disconnect();
}

TEST(Connect, ComplexConnect)
{
    ComplexConnect();
    Disconnect();
}

TEST(Connect, Ping)
{
    uint8_t pingPacket[] = PINGREQ_PACKET_INITIALIZER;
    SimpleConnect();
    int cnt = WriteToServer(sock, pingPacket, sizeof(pingPacket));
    TEST_ASSERT_EQUAL(sizeof(pingPacket), cnt);
    cnt = ReadFromServer(sock, testBuf, sizeof(testBuf));
    TEST_ASSERT_NOT_EQUAL(0, cnt);
    encbuf.data = testBuf;
    encbuf.len = cnt;
    umqtt_Error_t err = umqtt_DecodePacket(h, &encbuf);
    TEST_ASSERT_EQUAL(UMQTT_ERR_OK, err);
    TEST_ASSERT_EQUAL(UMQTT_EVENT_PINGRESP, cbEvent);
    Disconnect();
}

TEST(Connect, SubPub)
{
    SimpleConnect();

    // subscribe to two topics
    umqtt_Data_t topics[2];
    uint8_t qoss[2];
    UMQTT_INIT_DATA_STR(topics[0], "umqtt/topic0");
    UMQTT_INIT_DATA_STR(topics[1], "umqtt/topic1");
    qoss[0] = 1;
    qoss[1] = 0;
    umqtt_Subscribe_Options_t options = SUBSCRIBE_OPTIONS_INITIALIZER;
    UMQTT_INIT_SUBSCRIBE(options, 2, topics, qoss);
    umqtt_Error_t err = umqtt_BuildSubscribe(h, &encbuf, &options);
    TEST_ASSERT_EQUAL(UMQTT_ERR_OK, err);
    int cnt = WriteToServer(sock, encbuf.data, encbuf.len);
    TEST_ASSERT_EQUAL(encbuf.len, cnt);
    cnt = ReadFromServer(sock, testBuf, sizeof(testBuf));
    TEST_ASSERT_NOT_EQUAL(0, cnt);
    encbuf.data = testBuf;
    encbuf.len = cnt;
    err = umqtt_DecodePacket(h, &encbuf);
    TEST_ASSERT_EQUAL(UMQTT_ERR_OK, err);
    TEST_ASSERT_EQUAL(UMQTT_EVENT_SUBACK, cbEvent);
    TEST_ASSERT_EQUAL(2, cbBufLen);
    TEST_ASSERT_EQUAL(1, cbBuf[0]);
    TEST_ASSERT_EQUAL(0, cbBuf[1]);

    // reset encbuf and callbacks
    UMQTT_INIT_DATA_STATIC_BUF(encbuf, testBuf);
    cbEvent = 0;
    cbBufLen = 0;

    // publish topic 0
    // we should see publish back to us
    // verifies puback from us
    umqtt_Publish_Options_t pubopts = PUBLISH_OPTIONS_INITIALIZER;
    UMQTT_INIT_DATA_STR(pubopts.topic, "umqtt/topic0");
    UMQTT_INIT_DATA_STR(pubopts.message, "topic0message");
    err = umqtt_BuildPublish(h, &encbuf, &pubopts);
    TEST_ASSERT_EQUAL(UMQTT_ERR_OK, err);
    cnt = WriteToServer(sock, encbuf.data, encbuf.len);
    TEST_ASSERT_EQUAL(encbuf.len, cnt);

    // published qos 0 so should not get puback
    // now server should publish back to us since we are subscribed
    UMQTT_INIT_DATA_STATIC_BUF(encbuf, testBuf); // reset buf
    cnt = ReadFromServer(sock, testBuf, sizeof(testBuf));
    TEST_ASSERT_NOT_EQUAL(0, cnt);
    encbuf.data = testBuf;
    encbuf.len = cnt;
    err = umqtt_DecodePacket(h, &encbuf);
    TEST_ASSERT_EQUAL(UMQTT_ERR_OK, err);
    TEST_ASSERT_EQUAL(UMQTT_EVENT_PUBLISH, cbEvent);
    umqtt_Publish_Options_t *pPubOpts = (umqtt_Publish_Options_t*)cbBuf;
    TEST_ASSERT_FALSE(pPubOpts->dup);
    TEST_ASSERT_FALSE(pPubOpts->retain);
    TEST_ASSERT_EQUAL(1, pPubOpts->qos); // we asked for qos 1 in subscription
    TEST_ASSERT_EQUAL(12, pPubOpts->topic.len);
    TEST_ASSERT_EQUAL_STRING_LEN(&cbBuf[64], "umqtt/topic0", 12);
    TEST_ASSERT_EQUAL(13, pPubOpts->message.len);
    TEST_ASSERT_EQUAL_STRING_LEN(&cbBuf[128], "topic0message", 13);

    Disconnect();
}

TEST_GROUP_RUNNER(Connect)
{
    RUN_TEST_CASE(Connect, SimpleConnect);
    RUN_TEST_CASE(Connect, ComplexConnect);
    RUN_TEST_CASE(Connect, Ping);
    RUN_TEST_CASE(Connect, SubPub);
}

