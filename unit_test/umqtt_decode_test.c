
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>

#include "unity_fixture.h"
#include "umqtt/umqtt.h"
#include "umqtt_mocks.h"
#include "umqtt_wrapper.h"

TEST_GROUP(Decode);

static umqtt_Handle_t h = NULL;
static void *instBuf = NULL;
#define SIZE_INSTBUF 200
static uint8_t *pktBuf = NULL;
#define SIZE_PKTBUF 1024
static uint8_t *pktBuf2 = NULL;

static uint8_t mock_user = 1;
static void *mock_pUser = &mock_user;

static umqtt_Handle_t Connack_h;
static void *Connack_pUser;
static bool Connack_sessionPresent;
static uint8_t Connack_returnCode;
static void Connack_Reset(void)
{Connack_h = NULL; Connack_pUser = NULL; Connack_sessionPresent = false; Connack_returnCode = 0;}
static void
ConnackCb(umqtt_Handle_t h, void *pUser, bool sessionPresent, uint8_t returnCode)
{   Connack_h = h; Connack_pUser = pUser;
    Connack_sessionPresent = sessionPresent; Connack_returnCode = returnCode; }

static umqtt_Handle_t Publish_h;
static void *Publish_pUser;
static bool Publish_dup;
static bool Publish_retain;
static uint8_t Publish_qos;
static const char *Publish_pTopic;
static uint16_t Publish_topicLen;
static const uint8_t *Publish_pMsg;
static uint16_t Publish_msgLen;
static void Publish_Reset(void)
{   Publish_h = NULL; Publish_pUser = NULL; Publish_dup = false; Publish_retain = false;
    Publish_qos = 0; Publish_pTopic = NULL; Publish_topicLen = 0; Publish_pMsg = NULL; Publish_msgLen = 0; }
static void
PublishCb(umqtt_Handle_t h, void *pUser, bool dup, bool retain, uint8_t qos,
          const char *pTopic, uint16_t topicLen, const uint8_t *pMsg, uint16_t msgLen)
{   Publish_h = h; Publish_pUser = pUser; Publish_dup = dup; Publish_retain = retain;
    Publish_qos = qos; Publish_pTopic = pTopic; Publish_topicLen = topicLen, Publish_pMsg = pMsg; Publish_msgLen = msgLen; }

static umqtt_Handle_t Puback_h;
static void *Puback_pUser;
static uint16_t Puback_msgId;
static void Puback_Reset(void) { Puback_h = NULL; Puback_pUser = NULL; Puback_msgId = 0; }
static void
PubackCb(umqtt_Handle_t h, void *pUser, uint16_t msgId)
{   Puback_h = h; Puback_pUser = pUser; Puback_msgId = msgId; }

static umqtt_Handle_t Suback_h;
static void *Suback_pUser;
static const uint8_t *Suback_pRetCodes;
static uint16_t Suback_retCount;
static uint16_t Suback_msgId;
static void Suback_Reset(void)
{ Suback_h = NULL; Suback_pUser = NULL; Suback_pRetCodes = NULL; Suback_retCount = 0; Suback_msgId = 0; }
static void
SubackCb(umqtt_Handle_t h, void *pUser, const uint8_t *pRetCodes, uint16_t retCount, uint16_t msgId)
{   Suback_h = h; Suback_pUser = pUser; Suback_pRetCodes = pRetCodes; Suback_retCount = retCount; Suback_msgId = msgId; }

static umqtt_Handle_t Unsuback_h;
static void *Unsuback_pUser;
static uint16_t Unsuback_msgId;
static void Unsuback_Reset(void) { Unsuback_h = NULL; Unsuback_pUser = NULL; Unsuback_msgId = 0; }
static void
UnsubackCb(umqtt_Handle_t h, void *pUser, uint16_t msgId)
{   Unsuback_h = h; Unsuback_pUser = pUser; Unsuback_msgId = msgId; }

static umqtt_Handle_t Pingresp_h;
static void *Pingresp_pUser;
static void Pingresp_Reset(void) { Pingresp_h = NULL; Pingresp_pUser = NULL; }
static void
PingrespCb(umqtt_Handle_t h, void *pUser)
{   Pingresp_h = h; Pingresp_pUser = pUser; }

static umqtt_Callbacks_t callbacks =
{
    ConnackCb, PublishCb, PubackCb, SubackCb, UnsubackCb, PingrespCb
};

TEST_SETUP(Decode)
{
    // create an instance
    mock_malloc_Reset();
    instBuf = malloc(SIZE_INSTBUF);
    mock_hNet = &mock_hNet;
    transportConfig.hNet = mock_hNet;
    mock_malloc_shouldReturn[0] = instBuf;
    h = umqtt_New(&transportConfig, &callbacks, mock_pUser);
    TEST_ASSERT_NOT_NULL(h);
    // clear all mocks
    mock_malloc_Reset();
    mock_free_Reset();
    mock_NetRead_Reset();
    mock_NetWrite_Reset();
    Connack_Reset();
    Publish_Reset();
    Puback_Reset();
    Suback_Reset();
    Unsuback_Reset();
    Pingresp_Reset();
    // ready a buffer that can be used for packet allocation
    pktBuf = malloc(SIZE_PKTBUF);
    TEST_ASSERT_NOT_NULL(pktBuf);
    pktBuf2 = malloc(SIZE_PKTBUF);
    TEST_ASSERT_NOT_NULL(pktBuf2);
}

TEST_TEAR_DOWN(Decode)
{
    if (instBuf) { free(instBuf); instBuf = NULL; }
    if (pktBuf) { free(pktBuf); pktBuf = NULL; }
    if (pktBuf2) { free(pktBuf2); pktBuf2 = NULL; }
    h = NULL;
}

TEST(Decode, NullParms)
{
    umqtt_Error_t err;
    err = umqtt_DecodePacket(NULL, pktBuf, 1);
    TEST_ASSERT_EQUAL(UMQTT_ERR_PARM, err);
    err = umqtt_DecodePacket(h, NULL, 1);
    TEST_ASSERT_EQUAL(UMQTT_ERR_PARM, err);
    err = umqtt_DecodePacket(h, pktBuf, 0);
    TEST_ASSERT_EQUAL(UMQTT_ERR_PARM, err);
}

TEST(Decode, LengthMismatch)
{
    umqtt_Error_t err;
    pktBuf[0] = 4 << 4; // puback
    pktBuf[1] = 3; // rem len, should be 2
    pktBuf[2] = 1; // fake pkt id
    pktBuf[3] = 2;
    err = umqtt_DecodePacket(h, pktBuf, 4);
    TEST_ASSERT_EQUAL(UMQTT_ERR_PACKET_ERROR, err);
}

// packet with bad type (invalid)
TEST(Decode, BadType)
{
    umqtt_Error_t err;
    pktBuf[0] = 5 << 4; // bogus packet type
    pktBuf[1] = 2; // rem len
    pktBuf[2] = 1; // fake pkt id
    pktBuf[3] = 2;
    err = umqtt_DecodePacket(h, pktBuf, 4);
    TEST_ASSERT_EQUAL(UMQTT_ERR_PACKET_ERROR, err);
}

TEST(Decode, ConnackBadLen)
{
    umqtt_Error_t err;
    pktBuf[0] = 2 << 4; // connack
    pktBuf[1] = 3; // rem len
    pktBuf[2] = 1; // ack flags
    pktBuf[3] = 5; // ret code - not auth
    pktBuf[4] = 0; // extra byte
    err = umqtt_DecodePacket(h, pktBuf, 5);
    TEST_ASSERT_EQUAL(UMQTT_ERR_PACKET_ERROR, err);
    TEST_ASSERT_NULL(Connack_h);
    err = umqtt_GetConnectedStatus(h);
    TEST_ASSERT_EQUAL(UMQTT_ERR_DISCONNECTED, err);
}

TEST(Decode, ConnackBadRet)
{
    umqtt_Error_t err;
    pktBuf[0] = 2 << 4; // connack
    pktBuf[1] = 2; // rem len
    pktBuf[2] = 1; // ack flags
    pktBuf[3] = 5; // ret code - not auth
    err = umqtt_DecodePacket(h, pktBuf, 4);
    TEST_ASSERT_EQUAL(UMQTT_ERR_OK, err);
    TEST_ASSERT_EQUAL_PTR(instBuf, Connack_h);
    TEST_ASSERT_EQUAL_PTR(mock_pUser, Connack_pUser);
    TEST_ASSERT_TRUE(Connack_sessionPresent);
    TEST_ASSERT_EQUAL(5, Connack_returnCode);
    err = umqtt_GetConnectedStatus(h);
    TEST_ASSERT_EQUAL(UMQTT_ERR_DISCONNECTED, err);
}

TEST(Decode, Connack)
{
    umqtt_Error_t err;
    pktBuf[0] = 2 << 4; // connack
    pktBuf[1] = 2; // rem len
    pktBuf[2] = 1; // ack flags
    pktBuf[3] = 0; // ret code - ok
    err = umqtt_DecodePacket(h, pktBuf, 4);
    TEST_ASSERT_EQUAL(UMQTT_ERR_OK, err);
    TEST_ASSERT_EQUAL_PTR(instBuf, Connack_h);
    TEST_ASSERT_EQUAL_PTR(mock_pUser, Connack_pUser);
    TEST_ASSERT_TRUE(Connack_sessionPresent);
    TEST_ASSERT_EQUAL(0, Connack_returnCode);
    err = umqtt_GetConnectedStatus(h);
    TEST_ASSERT_EQUAL(UMQTT_ERR_CONNECTED, err);
}

TEST(Decode, ConnackWithDequeue)
{
    umqtt_Error_t err;
    PktBuf_t *pkt;
    uint8_t *buf;

    // make a dummy connect packet and enqueue it
    mock_malloc_shouldReturn[0] = pktBuf2;
    buf = wrap_newPacket(h, 30);
    buf[0] = 1 << 4;
    wrap_enqueuePacket(h, buf, 123, 456);
    pkt = wrap_getNextPktBuf(h);
    TEST_ASSERT_EQUAL_PTR(pktBuf2, pkt);

    // now do the decode again and verify the connect packet
    // was dequeued and freed
    pktBuf[0] = 2 << 4; // connack
    pktBuf[1] = 2; // rem len
    pktBuf[2] = 0; // ack flags
    pktBuf[3] = 4; // ret code
    err = umqtt_DecodePacket(h, pktBuf, 4);
    TEST_ASSERT_EQUAL(UMQTT_ERR_OK, err);
    TEST_ASSERT_EQUAL_PTR(instBuf, Connack_h);
    TEST_ASSERT_EQUAL_PTR(mock_pUser, Connack_pUser);
    TEST_ASSERT_FALSE(Connack_sessionPresent);
    TEST_ASSERT_EQUAL(4, Connack_returnCode);

    // verify packet is no longer enqueued
    pkt = wrap_getNextPktBuf(h);
    TEST_ASSERT_NULL(pkt);

    // verify it was freed
    TEST_ASSERT_TRUE(mock_free_wasCalled);
    TEST_ASSERT_EQUAL_PTR(pktBuf2, mock_free_in_ptr);
}

TEST(Decode, PublishBadTopicLen)
{
    // - topic length longer than remaining size
    static uint8_t pubPkt[] =
    {
        0x30, 6, // one byte less that will fit the topic
        0, 5, 't', 'o', 'p', 'i'
    };
    umqtt_Error_t err = umqtt_DecodePacket(h, pubPkt, sizeof(pubPkt));
    TEST_ASSERT_EQUAL(UMQTT_ERR_PACKET_ERROR, err);
}

TEST(Decode, PublishBadLenQos)
{
    // - qos != 0 and not enough remaining bytes
    static uint8_t pubPkt[] =
    {
        0x32, 7, // qos level 1 but no packet ID
        0, 5, 't', 'o', 'p', 'i', 'c'
    };
    umqtt_Error_t err = umqtt_DecodePacket(h, pubPkt, sizeof(pubPkt));
    TEST_ASSERT_EQUAL(UMQTT_ERR_PACKET_ERROR, err);
}

TEST(Decode, PublishBadQos)
{
    // - qos = 3, not valid
    static uint8_t pubPkt[] =
    {
        0x36, 9, // qos level 3
        0, 5, 't', 'o', 'p', 'i', 'c',
        1, 2 // packet ID
    };
    umqtt_Error_t err = umqtt_DecodePacket(h, pubPkt, sizeof(pubPkt));
    TEST_ASSERT_EQUAL(UMQTT_ERR_PACKET_ERROR, err);
}

TEST(Decode, PublishBadMsgLen)
{
    // - msg len longer than remaining size
    static uint8_t pubPkt[] =
    {
        0x30, 13, // one byte less than will fit message
        0, 5, 't', 'o', 'p', 'i', 'c',
        'm', 'e', 's', 's', 'a', 'g', 'e'
    };
    umqtt_Error_t err = umqtt_DecodePacket(h, pubPkt, sizeof(pubPkt));
    TEST_ASSERT_EQUAL(UMQTT_ERR_PACKET_ERROR, err);
}

TEST(Decode, PublishExtraBytes)
{
    // - msg len longer than remaining size
    static uint8_t pubPkt[] =
    {
        0x30, 15, // one byte less than will fit message
        0, 5, 't', 'o', 'p', 'i', 'c',
        'm', 'e', 's', 's', 'a', 'g', 'e',
        0
    };
    umqtt_Error_t err = umqtt_DecodePacket(h, pubPkt, sizeof(pubPkt));
    TEST_ASSERT_EQUAL(UMQTT_ERR_PACKET_ERROR, err);
}

TEST(Decode, PublishTopicBasic)
{
    static uint8_t pubPkt[] =
    {
        0x30, 7,
        0, 5, 't', 'o', 'p', 'i', 'c',
    };
    umqtt_Error_t err = umqtt_DecodePacket(h, pubPkt, sizeof(pubPkt));
    TEST_ASSERT_EQUAL(UMQTT_ERR_OK, err);
    TEST_ASSERT_EQUAL_PTR(h, Publish_h);
    TEST_ASSERT_EQUAL_PTR(mock_pUser, Publish_pUser);
    TEST_ASSERT_FALSE(Publish_dup);
    TEST_ASSERT_FALSE(Publish_retain);
    TEST_ASSERT_EQUAL(0, Publish_qos);
    TEST_ASSERT_EQUAL(strlen("topic"), Publish_topicLen);
    TEST_ASSERT_EQUAL_STRING_LEN("topic", Publish_pTopic, Publish_topicLen);
    TEST_ASSERT_NULL(Publish_pMsg);
    TEST_ASSERT_EQUAL(0, Publish_msgLen);
    TEST_ASSERT_FALSE(mock_NetWrite_wasCalled);
}

TEST(Decode, PublishTopicMsg)
{
    static uint8_t pubPkt[] =
    {
        0x31, 14, // add retain
        0, 5, 't', 'o', 'p', 'i', 'c',
        'm', 'e', 's', 's', 'a', 'g', 'e',
    };
    umqtt_Error_t err = umqtt_DecodePacket(h, pubPkt, sizeof(pubPkt));
    TEST_ASSERT_EQUAL(UMQTT_ERR_OK, err);
    TEST_ASSERT_EQUAL_PTR(h, Publish_h);
    TEST_ASSERT_EQUAL_PTR(mock_pUser, Publish_pUser);
    TEST_ASSERT_FALSE(Publish_dup);
    TEST_ASSERT_TRUE(Publish_retain);
    TEST_ASSERT_EQUAL(0, Publish_qos);
    TEST_ASSERT_EQUAL(strlen("topic"), Publish_topicLen);
    TEST_ASSERT_EQUAL_STRING_LEN("topic", Publish_pTopic, Publish_topicLen);
    TEST_ASSERT_EQUAL(7, Publish_msgLen);
    TEST_ASSERT_EQUAL_MEMORY("message", Publish_pMsg, Publish_msgLen);
    TEST_ASSERT_FALSE(mock_NetWrite_wasCalled);
}

TEST(Decode, PublishQos1)
{
    static uint8_t pubPkt[] =
    {
        0x3A, 16, // dup and qos=1
        0, 5, 't', 'o', 'p', 'i', 'c',
        0x34, 0x56, // packet ID
        'm', 'e', 's', 's', 'a', 'g', 'e',
    };
    static uint8_t puback[] = { 0x40, 2, 0x34, 0x56 }; // expected puback
    mock_NetWrite_shouldReturn = 4;
    mock_NetWrite_pCopy = pktBuf;
    mock_NetWrite_copyLen = SIZE_PKTBUF;
    umqtt_Error_t err = umqtt_DecodePacket(h, pubPkt, sizeof(pubPkt));
    TEST_ASSERT_EQUAL(UMQTT_ERR_OK, err);
    TEST_ASSERT_EQUAL_PTR(h, Publish_h);
    TEST_ASSERT_EQUAL_PTR(mock_pUser, Publish_pUser);
    TEST_ASSERT_TRUE(Publish_dup);
    TEST_ASSERT_FALSE(Publish_retain);
    TEST_ASSERT_EQUAL(1, Publish_qos);
    TEST_ASSERT_EQUAL(strlen("topic"), Publish_topicLen);
    TEST_ASSERT_EQUAL_STRING_LEN("topic", Publish_pTopic, Publish_topicLen);
    TEST_ASSERT_EQUAL(7, Publish_msgLen);
    TEST_ASSERT_EQUAL_MEMORY("message", Publish_pMsg, Publish_msgLen);
    TEST_ASSERT_TRUE(mock_NetWrite_wasCalled);
    TEST_ASSERT_EQUAL_PTR(mock_hNet, mock_NetWrite_in_hNet);
    TEST_ASSERT_EQUAL(4, mock_NetWrite_in_len);
    TEST_ASSERT_FALSE(mock_NetWrite_in_isMore);
    TEST_ASSERT_NOT_NULL(mock_NetWrite_in_pBuf);
    TEST_ASSERT_EQUAL_MEMORY(puback, mock_NetWrite_pCopy, 4);
}

TEST(Decode, PubackBadLen)
{
    umqtt_Error_t err;
    pktBuf[0] = 4 << 4; // puback
    pktBuf[1] = 3; // rem len
    pktBuf[2] = 1; // pkt id
    pktBuf[3] = 2;
    pktBuf[4] = 0; // extra byte
    err = umqtt_DecodePacket(h, pktBuf, 5);
    TEST_ASSERT_EQUAL(UMQTT_ERR_PACKET_ERROR, err);
    TEST_ASSERT_NULL(Puback_h);
}

TEST(Decode, Puback)
{
    umqtt_Error_t err;
    pktBuf[0] = 4 << 4; // puback
    pktBuf[1] = 2; // rem len
    pktBuf[2] = 3; // pkt id
    pktBuf[3] = 4;
    err = umqtt_DecodePacket(h, pktBuf, 4);
    TEST_ASSERT_EQUAL(UMQTT_ERR_OK, err);
    TEST_ASSERT_EQUAL_PTR(instBuf, Puback_h);
    TEST_ASSERT_EQUAL_PTR(mock_pUser, Puback_pUser);
    TEST_ASSERT_EQUAL(0x0304, Puback_msgId);
}

TEST(Decode, PubackWithDequeue)
{
    umqtt_Error_t err;
    PktBuf_t *pkt;
    uint8_t *buf;

    // make a dummy publish packet and enqueue it
    mock_malloc_shouldReturn[0] = pktBuf2;
    buf = wrap_newPacket(h, 30);
    buf[0] = 3 << 4;
    wrap_enqueuePacket(h, buf, 0x1234, 456);
    pkt = wrap_getNextPktBuf(h);
    TEST_ASSERT_EQUAL_PTR(pktBuf2, pkt);

    // now do the decode again and verify the publish packet
    // was dequeued and freed
    pktBuf[0] = 4 << 4; // puback
    pktBuf[1] = 2; // rem len
    pktBuf[2] = 0x12; // pkt id
    pktBuf[3] = 0x34;
    err = umqtt_DecodePacket(h, pktBuf, 4);
    TEST_ASSERT_EQUAL(UMQTT_ERR_OK, err);
    TEST_ASSERT_EQUAL_PTR(instBuf, Puback_h);
    TEST_ASSERT_EQUAL_PTR(mock_pUser, Puback_pUser);
    TEST_ASSERT_EQUAL(0x1234, Puback_msgId);

    // verify packet is no longer enqueued
    pkt = wrap_getNextPktBuf(h);
    TEST_ASSERT_NULL(pkt);

    // verify it was freed
    TEST_ASSERT_TRUE(mock_free_wasCalled);
    TEST_ASSERT_EQUAL_PTR(pktBuf2, mock_free_in_ptr);
}

TEST(Decode, SubackBadLen)
{
    umqtt_Error_t err;
    pktBuf[0] = 9 << 4; // suback
    pktBuf[1] = 2; // rem len
    pktBuf[2] = 1; // pkt id
    pktBuf[3] = 2;
//    pktBuf[4] = 0; // ret code
    err = umqtt_DecodePacket(h, pktBuf, 4);
    TEST_ASSERT_EQUAL(UMQTT_ERR_PACKET_ERROR, err);
    TEST_ASSERT_NULL(Puback_h);
}

TEST(Decode, Suback)
{
    umqtt_Error_t err;
    pktBuf[0] = 9 << 4; // suback
    pktBuf[1] = 3; // rem len
    pktBuf[2] = 3; // pkt id
    pktBuf[3] = 5;
    pktBuf[4] = 0; // ret code
    err = umqtt_DecodePacket(h, pktBuf, 5);
    TEST_ASSERT_EQUAL(UMQTT_ERR_OK, err);
    TEST_ASSERT_EQUAL_PTR(instBuf, Suback_h);
    TEST_ASSERT_EQUAL_PTR(mock_pUser, Suback_pUser);
    TEST_ASSERT_EQUAL_PTR(&pktBuf[4], Suback_pRetCodes);
    TEST_ASSERT_EQUAL(1, Suback_retCount);
    TEST_ASSERT_EQUAL(0, Suback_pRetCodes[0]);
    TEST_ASSERT_EQUAL(0x0305, Suback_msgId);
}

TEST(Decode, SubackMultiTopic)
{
    umqtt_Error_t err;
    pktBuf[0] = 9 << 4; // suback
    pktBuf[1] = 4; // rem len
    pktBuf[2] = 6; // pkt id
    pktBuf[3] = 4;
    pktBuf[4] = 2; // ret code
    pktBuf[5] = 0x80; // fail code
    err = umqtt_DecodePacket(h, pktBuf, 6);
    TEST_ASSERT_EQUAL(UMQTT_ERR_OK, err);
    TEST_ASSERT_EQUAL_PTR(instBuf, Suback_h);
    TEST_ASSERT_EQUAL_PTR(mock_pUser, Suback_pUser);
    TEST_ASSERT_EQUAL_PTR(&pktBuf[4], Suback_pRetCodes);
    TEST_ASSERT_EQUAL(2, Suback_retCount);
    TEST_ASSERT_EQUAL(2, Suback_pRetCodes[0]);
    TEST_ASSERT_EQUAL(0x80, Suback_pRetCodes[1]);
    TEST_ASSERT_EQUAL(0x0604, Suback_msgId);
}

TEST(Decode, SubackWithDequeue)
{
    umqtt_Error_t err;
    PktBuf_t *pkt;
    uint8_t *buf;

    // make a dummy subscribe packet and enqueue it
    mock_malloc_shouldReturn[0] = pktBuf2;
    buf = wrap_newPacket(h, 30);
    buf[0] = 8 << 4;
    wrap_enqueuePacket(h, buf, 0x2345, 456);
    pkt = wrap_getNextPktBuf(h);
    TEST_ASSERT_EQUAL_PTR(pktBuf2, pkt);

    // now do the decode again and verify the subscribe packet
    // was dequeued and freed
    pktBuf[0] = 9 << 4; // suback
    pktBuf[1] = 3; // rem len
    pktBuf[2] = 0x23; // pkt id
    pktBuf[3] = 0x45;
    pktBuf[4] = 1; // ret code
    err = umqtt_DecodePacket(h, pktBuf, 5);
    TEST_ASSERT_EQUAL(UMQTT_ERR_OK, err);
    TEST_ASSERT_EQUAL_PTR(instBuf, Suback_h);
    TEST_ASSERT_EQUAL_PTR(mock_pUser, Suback_pUser);
    TEST_ASSERT_EQUAL_PTR(&pktBuf[4], Suback_pRetCodes);
    TEST_ASSERT_EQUAL(1, Suback_retCount);
    TEST_ASSERT_EQUAL(1, Suback_pRetCodes[0]);
    TEST_ASSERT_EQUAL(0x2345, Suback_msgId);

    // verify packet is no longer enqueued
    pkt = wrap_getNextPktBuf(h);
    TEST_ASSERT_NULL(pkt);

    // verify it was freed
    TEST_ASSERT_TRUE(mock_free_wasCalled);
    TEST_ASSERT_EQUAL_PTR(pktBuf2, mock_free_in_ptr);
}

TEST(Decode, UnsubackBadLen)
{
    umqtt_Error_t err;
    pktBuf[0] = 11 << 4; // unsuback
    pktBuf[1] = 3; // rem len
    pktBuf[2] = 1; // pkt id
    pktBuf[3] = 2;
    pktBuf[4] = 0; // extra byte
    err = umqtt_DecodePacket(h, pktBuf, 5);
    TEST_ASSERT_EQUAL(UMQTT_ERR_PACKET_ERROR, err);
    TEST_ASSERT_NULL(Puback_h);
}

TEST(Decode, Unsuback)
{
    umqtt_Error_t err;
    pktBuf[0] = 11 << 4; // unsuback
    pktBuf[1] = 2; // rem len
    pktBuf[2] = 7; // pkt id
    pktBuf[3] = 4;
    err = umqtt_DecodePacket(h, pktBuf, 4);
    TEST_ASSERT_EQUAL(UMQTT_ERR_OK, err);
    TEST_ASSERT_EQUAL_PTR(instBuf, Unsuback_h);
    TEST_ASSERT_EQUAL_PTR(mock_pUser, Unsuback_pUser);
    TEST_ASSERT_EQUAL(0x0704, Unsuback_msgId);
}

TEST(Decode, UnsubackWithDequeue)
{
    umqtt_Error_t err;
    PktBuf_t *pkt;
    uint8_t *buf;

    // make a dummy unsubscribe packet and enqueue it
    mock_malloc_shouldReturn[0] = pktBuf2;
    buf = wrap_newPacket(h, 30);
    buf[0] = 10 << 4;
    wrap_enqueuePacket(h, buf, 0x3456, 456);
    pkt = wrap_getNextPktBuf(h);
    TEST_ASSERT_EQUAL_PTR(pktBuf2, pkt);

    // now do the decode again and verify the unsubscribe packet
    // was dequeued and freed
    pktBuf[0] = 11 << 4; // unsuback
    pktBuf[1] = 2; // rem len
    pktBuf[2] = 0x34; // pkt id
    pktBuf[3] = 0x56;
    err = umqtt_DecodePacket(h, pktBuf, 4);
    TEST_ASSERT_EQUAL(UMQTT_ERR_OK, err);
    TEST_ASSERT_EQUAL_PTR(instBuf, Unsuback_h);
    TEST_ASSERT_EQUAL_PTR(mock_pUser, Unsuback_pUser);
    TEST_ASSERT_EQUAL(0x3456, Unsuback_msgId);

    // verify packet is no longer enqueued
    pkt = wrap_getNextPktBuf(h);
    TEST_ASSERT_NULL(pkt);

    // verify it was freed
    TEST_ASSERT_TRUE(mock_free_wasCalled);
    TEST_ASSERT_EQUAL_PTR(pktBuf2, mock_free_in_ptr);
}

TEST(Decode, PingrespBadLen)
{
    umqtt_Error_t err;
    pktBuf[0] = 13 << 4; // pingresp
    pktBuf[1] = 1; // rem len
    pktBuf[2] = 0; // extra byte
    err = umqtt_DecodePacket(h, pktBuf, 3);
    TEST_ASSERT_EQUAL(UMQTT_ERR_PACKET_ERROR, err);
    TEST_ASSERT_NULL(Puback_h);
}

TEST(Decode, Pingresp)
{
    umqtt_Error_t err;
    pktBuf[0] = 13 << 4; // pingresp
    pktBuf[1] = 0; // rem len
    err = umqtt_DecodePacket(h, pktBuf, 2);
    TEST_ASSERT_EQUAL(UMQTT_ERR_OK, err);
    TEST_ASSERT_EQUAL_PTR(instBuf, Pingresp_h);
    TEST_ASSERT_EQUAL_PTR(mock_pUser, Pingresp_pUser);
}

TEST_GROUP_RUNNER(Decode)
{
    RUN_TEST_CASE(Decode, NullParms);
    RUN_TEST_CASE(Decode, LengthMismatch);
    RUN_TEST_CASE(Decode, BadType);
    RUN_TEST_CASE(Decode, ConnackBadLen);
    RUN_TEST_CASE(Decode, ConnackBadRet);
    RUN_TEST_CASE(Decode, Connack);
    RUN_TEST_CASE(Decode, ConnackWithDequeue);
    RUN_TEST_CASE(Decode, PublishBadTopicLen);
    RUN_TEST_CASE(Decode, PublishBadLenQos);
    RUN_TEST_CASE(Decode, PublishBadQos);
    RUN_TEST_CASE(Decode, PublishBadMsgLen);
    RUN_TEST_CASE(Decode, PublishTopicBasic);
    RUN_TEST_CASE(Decode, PublishTopicMsg);
    RUN_TEST_CASE(Decode, PublishQos1);
    RUN_TEST_CASE(Decode, PubackBadLen);
    RUN_TEST_CASE(Decode, Puback);
    RUN_TEST_CASE(Decode, PubackWithDequeue);
    RUN_TEST_CASE(Decode, SubackBadLen);
    RUN_TEST_CASE(Decode, Suback);
    RUN_TEST_CASE(Decode, SubackMultiTopic);
    RUN_TEST_CASE(Decode, SubackWithDequeue);
    RUN_TEST_CASE(Decode, UnsubackBadLen);
    RUN_TEST_CASE(Decode, Unsuback);
    RUN_TEST_CASE(Decode, UnsubackWithDequeue);
    RUN_TEST_CASE(Decode, PingrespBadLen);
    RUN_TEST_CASE(Decode, Pingresp);
}

