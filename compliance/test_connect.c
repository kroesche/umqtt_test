/******************************************************************************
 * test_connect.c - umqtt compliance test
 *
 * Copyright (c) 2016, Joseph Kroesche (tronics.kroesche.io)
 * All rights reserved.
 *
 * This software is released under the FreeBSD license, found in the
 * accompanying file LICENSE.txt and at the following URL:
 *      http://www.freebsd.org/copyright/freebsd-license.html
 *
 * This software is provided as-is and without warranty.
 */

#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <time.h>
#include <sys/time.h>
#include <unistd.h>

#include "unity_fixture.h"
#include "umqtt/umqtt.h"

// host network functions - must be provided by test suite
extern int ConnectToServer(void);
extern int WriteToServer(int socket, const uint8_t *buf, uint32_t len);
extern int ReadFromServer(int socket, const uint8_t *buf, uint32_t len);
extern void DisconnectServer(int socket);

// globals used by the test
static int sock = 0; // network socket
static umqtt_Handle_t u = NULL; // umqtt handle
static time_t secs0 = 0; // starting time of the test

// CONNACK handler stub
static umqtt_Handle_t Connack_h;
static void *Connack_pUser;
static bool Connack_sessionPresent;
static uint8_t Connack_retCode;
static void
ConnackCb(umqtt_Handle_t h, void *pUser, bool sessionPresent, uint8_t retCode)
{
    Connack_h = h;
    Connack_pUser = pUser;
    Connack_sessionPresent = sessionPresent;
    Connack_retCode = retCode;
}
static void Connack_Reset(void) { ConnackCb(NULL, NULL, false, 0); }

// PUBLISH handler stub
static umqtt_Handle_t Publish_h;
static void *Publish_pUser;
static bool Publish_dup;
static bool Publish_retain;
static uint8_t Publish_qos;
static char Publish_topic[80];
static uint16_t Publish_topicLen;
static uint8_t Publish_msg[80];
static uint16_t Publish_msgLen;
static void
PublishCb(umqtt_Handle_t h, void *pUser, bool dup, bool retain,
               uint8_t qos, const char *pTopic, uint16_t topicLen,
               const uint8_t *pMsg, uint16_t msgLen)
{
    Publish_h = h; Publish_pUser = pUser; Publish_dup = dup; Publish_retain = retain;
    Publish_qos = qos; Publish_topicLen = topicLen;
    Publish_msgLen = msgLen;
    if (pTopic)
    {
        strncpy(Publish_topic, pTopic, sizeof(Publish_topic));
    }
    if (pMsg)
    {
        msgLen = msgLen > sizeof(Publish_msg) ? sizeof(Publish_msg) : msgLen;
        memcpy(Publish_msg, pMsg, msgLen);
    }
}
static void Publish_Reset(void)
{ PublishCb(NULL, NULL, false, false, 0, NULL, 0, NULL, 0); }

// PUBACK handler stub
static umqtt_Handle_t Puback_h;
static void *Puback_pUser;
static uint16_t Puback_pktId;
static void
PubackCb(umqtt_Handle_t h, void *pUser, uint16_t pktId)
{   Puback_h = h; Puback_pUser = pUser; Puback_pktId = pktId; }
static void Puback_Reset(void) { PubackCb(NULL, NULL, 0); }

// SUBACK handler stub
static umqtt_Handle_t Suback_h;
static void *Suback_pUser;
static uint16_t Suback_retCount;
static uint16_t Suback_pktId;
static uint8_t Suback_retCodes[8];
static void
SubackCb(umqtt_Handle_t h, void *pUser, const uint8_t *retCodes,
         uint16_t retCount, uint16_t pktId)
{
    Suback_h = h;
    Suback_pUser = pUser;
    Suback_retCount = retCount;
    Suback_pktId = pktId;
    if (retCodes)
    {
        retCount = retCount > 8 ? 8 : retCount;
        memcpy(Suback_retCodes, retCodes, retCount);
    }
}
static void Suback_Reset(void) { SubackCb(NULL, NULL, NULL, 0, 0); }

// UNSUBACK handler stub
static umqtt_Handle_t Unsuback_h;
static void *Unsuback_pUser;
static uint16_t Unsuback_pktId;
static void
UnsubackCb(umqtt_Handle_t h, void *pUser, uint16_t pktId)
{ Unsuback_h = h; Unsuback_pUser = pUser; Unsuback_pktId = pktId; }
static void Unsuback_Reset(void) { UnsubackCb(NULL, NULL, 0); }

// PINGRESP handler stub
static umqtt_Handle_t Pingresp_h;
static void *Pingresp_pUser;
static void
PingrespCb(umqtt_Handle_t h, void *pUser)
{ Pingresp_h = h; Pingresp_pUser = pUser; }
static void Pingresp_Reset(void) { PingrespCb(NULL, NULL); }

// callbacks structure needed for umqtt init
static umqtt_Callbacks_t callbacks =
{   ConnackCb, PublishCb, PubackCb, SubackCb, UnsubackCb, PingrespCb };

// implementation of malloc needed for umqtt
// this could be instrumented if needed for additional checks or
// for debug
static void *
testMalloc(size_t size)
{
    return malloc(size);
}

// implementation of free needed for umqtt
static void
testFree(void *ptr)
{
    free(ptr);
}

// implementation of umqtt network read function
static int
netReadPacket(void *hNet, uint8_t **ppBuf)
{
    int socket = *(int *)hNet;
    uint8_t *buf = testMalloc(512);
    int ret = ReadFromServer(socket, buf, 512);
    if (ret <= 0)
    {
        testFree(buf);
    }
    *ppBuf = buf;
    return ret;
}

// implementation of umqtt network write function
static int
netWritePacket(void *hNet, const uint8_t *pBuf, uint32_t len, bool isMore)
{
    int socket = *(int *)hNet;
    int ret = WriteToServer(socket, pBuf, len);
    return ret;
}

// transport structure needed for umqtt init
static umqtt_TransportConfig_t transport =
{   &sock, testMalloc, testFree, netReadPacket, netWritePacket };

// test helper that gets time ticks in milliseconds
// this is relative not absolute
static time_t
getTestTicksMs(void)
{
    struct timeval tv;
    gettimeofday(&tv, NULL);
    uint32_t ticks = tv.tv_sec - secs0;
    ticks *= 1000;
    ticks += tv.tv_usec / 1000;
    return ticks;
}

// test helper
// execute umqtt Run loop until pointer is non-null or time expires
static void
RunUntil(void **wait_h, unsigned int seconds)
{
    time_t time0 = time(NULL);
    do
    {
        time_t ticks = getTestTicksMs();
        umqtt_Error_t err = umqtt_Run(u, ticks);
        TEST_ASSERT_EQUAL(UMQTT_ERR_OK, err);
        if (*wait_h)
        {
            break;
        }
    } while ((time(NULL) - time0) < seconds);
}

TEST_GROUP(Connect);

// Create a client instance, init global state and connect to server
TEST_SETUP(Connect)
{
    // reset all the callback stubs
    Connack_Reset();
    Publish_Reset();
    Puback_Reset();
    Suback_Reset();
    Unsuback_Reset();
    Pingresp_Reset();
    sock = 0;

    // init umqtt instance
    u = umqtt_New(&transport, &callbacks, NULL);
    TEST_ASSERT_NOT_NULL(u);

    // give it an initial ticks value
    struct timeval tv;
    gettimeofday(&tv, NULL);
    secs0 = tv.tv_sec;
    time_t ticks = getTestTicksMs();
    umqtt_Error_t err = umqtt_Run(u, ticks);
    TEST_ASSERT_EQUAL(UMQTT_ERR_OK, err);

    // initiate network connection
    sock = ConnectToServer();
    TEST_ASSERT(sock > 0);
}

// Clear handles and disconnect from server
TEST_TEAR_DOWN(Connect)
{
    // if connected, then attempt disconnect
    umqtt_Error_t err = umqtt_GetConnectedStatus(u);
    if (err == UMQTT_ERR_CONNECTED)
    {
        umqtt_Error_t err = umqtt_Disconnect(u);
        TEST_ASSERT_EQUAL(UMQTT_ERR_OK, err);
    }
    // de-allocate the umqtt instance
    umqtt_Delete(u);
    u = NULL;
    // attempt network disconnect
    DisconnectServer(sock);
}

// perform a simple connection to the MQTT broker and validate response.
// This connection uses minimal options.
TEST(Connect, SimpleConnect)
{
    umqtt_Error_t err;
    err = umqtt_Connect(u, true, false, 0, 30, "umqtt0", NULL, NULL, 0, NULL, NULL);
    TEST_ASSERT_EQUAL(UMQTT_ERR_OK, err);
    err = umqtt_GetConnectedStatus(u);
    TEST_ASSERT_EQUAL(UMQTT_ERR_CONNECT_PENDING, err);

    // give MQTT server time to respond to connection
    RunUntil(&Connack_h, 10);
    TEST_ASSERT_EQUAL_PTR(u, Connack_h);
    TEST_ASSERT_EQUAL(0, Connack_retCode);
    err = umqtt_GetConnectedStatus(u);
    TEST_ASSERT_EQUAL(UMQTT_ERR_CONNECTED, err);
}

// perform a more complex connection and validate response.
// This connection uses session options and a will topic.
TEST(Connect, ComplexConnect)
{
    umqtt_Error_t err;
    char *client = "umqtt0";
    char *willTopic = "umqtt/willTopic";
    uint8_t *willMessage = (uint8_t *)"willMessage";
    uint32_t msgLen = strlen((char *)willMessage);
    char *username = "username";
    char *password = "password";
    err = umqtt_Connect(u, false, true, 1, 30, client, willTopic,
                        willMessage, msgLen, username, password);
    TEST_ASSERT_EQUAL(UMQTT_ERR_OK, err);
    err = umqtt_GetConnectedStatus(u);
    TEST_ASSERT_EQUAL(UMQTT_ERR_CONNECT_PENDING, err);

    // give MQTT server time to respond to connection
    RunUntil(&Connack_h, 10);
    TEST_ASSERT_EQUAL_PTR(u, Connack_h);
    TEST_ASSERT_EQUAL(0, Connack_retCode);
    err = umqtt_GetConnectedStatus(u);
    TEST_ASSERT_EQUAL(UMQTT_ERR_CONNECTED, err);
}

// let enough time elapse to verify ping gets sent and ack'ed
TEST(Connect, Ping)
{
    umqtt_Error_t err;
    // do the simple connect scenario first
    TEST_Connect_SimpleConnect_();

    err = umqtt_GetConnectedStatus(u);
    TEST_ASSERT_EQUAL(UMQTT_ERR_CONNECTED, err);

    // go to run loop for long enough to give time for ping to happen
    RunUntil(&Pingresp_h, 20);
    TEST_ASSERT_EQUAL_PTR(u, Pingresp_h);
    err = umqtt_GetConnectedStatus(u);
    TEST_ASSERT_EQUAL(UMQTT_ERR_CONNECTED, err);
}

// publish multiple topics, verify expected response
// then publish each back to the broker, and verify broker
// publishes back to us
TEST(Connect, SubPub)
{
    uint16_t msgId;
    umqtt_Error_t err;

    // do the simple connect scenario first
    TEST_Connect_SimpleConnect_();

    err = umqtt_GetConnectedStatus(u);
    TEST_ASSERT_EQUAL(UMQTT_ERR_CONNECTED, err);

    // set up subscribe parameters
    const char *topics[2];
    topics[0] = "umqtt/topic0";
    topics[1] = "umqtt/topic1";
    uint8_t qoss[2];
    qoss[0] = 0;
    qoss[1] = 1;

    // do the subscribe
    TEST_ASSERT_NULL(Suback_h);
    err = umqtt_Subscribe(u, 2, topics, qoss, &msgId);
    TEST_ASSERT_EQUAL(UMQTT_ERR_OK, err);

    // go to run loop to wait for suback - subscribe complete
    RunUntil(&Suback_h, 5);
    TEST_ASSERT_EQUAL_PTR(u, Suback_h);
    TEST_ASSERT_EQUAL(2, Suback_retCount);
    TEST_ASSERT_EQUAL(msgId, Suback_pktId);
    TEST_ASSERT_EQUAL(0, Suback_retCodes[0]);
    TEST_ASSERT_EQUAL(1, Suback_retCodes[1]);

    // now publish to topic0 and check we get a publish back
    TEST_ASSERT_NULL(Puback_h);
    TEST_ASSERT_NULL(Publish_h);
    uint8_t *msg = (uint8_t *)"topic0message";
    uint32_t msgLen = strlen((char *)msg);
    err = umqtt_Publish(u, topics[0], msg, msgLen, 0, false, NULL);
    RunUntil(&Publish_h, 5);

    // Since qos was 0 there should not be a Puback
    TEST_ASSERT_NULL(Puback_h);
    // should have received a publish
    TEST_ASSERT_EQUAL_PTR(u, Publish_h);
    TEST_ASSERT_FALSE(Publish_dup);
    TEST_ASSERT_FALSE(Publish_retain);
    TEST_ASSERT_EQUAL(0, Publish_qos);
    TEST_ASSERT_EQUAL_STRING_LEN(topics[0], Publish_topic, Publish_topicLen);
    TEST_ASSERT_EQUAL(strlen(topics[0]), Publish_topicLen);
    TEST_ASSERT_EQUAL_UINT8_ARRAY(msg, Publish_msg, msgLen);
    TEST_ASSERT_EQUAL(msgLen, Publish_msgLen);

    // publish the second topic and verify it gets published
    Publish_Reset();
    Puback_Reset();
    TEST_ASSERT_NULL(Puback_h);
    TEST_ASSERT_NULL(Publish_h);
    msg = (uint8_t *)"topic1msg";
    msgLen = strlen((char *)msg);
    msgId = 0;
    err = umqtt_Publish(u, topics[1], msg, msgLen, 1, false, &msgId);
    TEST_ASSERT_NOT_EQUAL(0, msgId);
    RunUntil(&Puback_h, 5);

    // Since qos was 1 there should be a Puback
    TEST_ASSERT_EQUAL_PTR(u, Puback_h);
    TEST_ASSERT_EQUAL(msgId, Puback_pktId);
    // should have received a publish
    TEST_ASSERT_EQUAL_PTR(u, Publish_h);
    TEST_ASSERT_FALSE(Publish_dup);
    TEST_ASSERT_FALSE(Publish_retain);
    TEST_ASSERT_EQUAL(1, Publish_qos);
    TEST_ASSERT_EQUAL_STRING(topics[1], Publish_topic);
    TEST_ASSERT_EQUAL(strlen(topics[1]), Publish_topicLen);
    TEST_ASSERT_EQUAL_UINT8_ARRAY(msg, Publish_msg, msgLen);
    TEST_ASSERT_EQUAL(msgLen, Publish_msgLen);
}

TEST(Connect, Unsubscribe)
{
    // need to add test
    TEST_IGNORE();
}

TEST_GROUP_RUNNER(Connect)
{
    RUN_TEST_CASE(Connect, SimpleConnect);
    RUN_TEST_CASE(Connect, ComplexConnect);
    printf("[Next test takes about 20 seconds]\n");
    RUN_TEST_CASE(Connect, Ping);
    RUN_TEST_CASE(Connect, SubPub);
    RUN_TEST_CASE(Connect, Unsubscribe);
}

