
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>

#include "unity_fixture.h"
#include "umqtt/umqtt.h"
#include "umqtt_mocks.h"
#include "umqtt_wrapper.h"

TEST_GROUP(Unsubscribe);

static umqtt_Handle_t h = NULL;
static void *instBuf = NULL;
#define SIZE_INSTBUF 200
static uint8_t *pktBuf = NULL;
#define SIZE_PKTBUF 1024
static char *topicBuf = NULL;
#define SIZE_TOPICBUF 256

TEST_SETUP(Unsubscribe)
{
    // allocate memory for the instance that will be created
    // init the instance so it will be ready for all tests
    mock_malloc_Reset();
    instBuf = malloc(SIZE_INSTBUF);
    mock_hNet = &mock_hNet;
    transportConfig.hNet = mock_hNet;
    mock_malloc_shouldReturn[0] = instBuf;
    h = umqtt_New(&transportConfig, NULL, NULL);
    TEST_ASSERT_NOT_NULL(h);
    // clear all mocks
    mock_malloc_Reset();
    mock_free_Reset();
    mock_NetRead_Reset();
    mock_NetWrite_Reset();
    // ready a buffer that can be used for packet allocation
    pktBuf = malloc(SIZE_PKTBUF);
    TEST_ASSERT_NOT_NULL(pktBuf);
    topicBuf = malloc(SIZE_TOPICBUF);
    TEST_ASSERT_NOT_NULL(topicBuf);
    memset(topicBuf, 'A', SIZE_TOPICBUF);
    // fake an existing connection
    wrap_setConnected(h, true);
}

TEST_TEAR_DOWN(Unsubscribe)
{
    if (instBuf) { free(instBuf); instBuf = NULL; }
    if (pktBuf) { free(pktBuf); pktBuf = NULL; }
    if (topicBuf) { free(topicBuf); topicBuf = NULL; }
    h = NULL;
}

TEST(Unsubscribe, NullParms)
{
    const char *topics[1];
    topics[0] = "topic";
    umqtt_Error_t err = umqtt_Unsubscribe(NULL, 0, NULL, NULL);
    TEST_ASSERT_EQUAL(UMQTT_ERR_PARM, err);
    err = umqtt_Unsubscribe(h, 0, NULL, NULL);
    TEST_ASSERT_EQUAL(UMQTT_ERR_PARM, err);
    err = umqtt_Unsubscribe(h, 0, topics, NULL);
    TEST_ASSERT_EQUAL(UMQTT_ERR_PARM, err);
}

TEST(Unsubscribe, CalcLength)
{
    umqtt_Error_t err;
    uint16_t expectedLen;
    const char *topics[2];

    // base value for expected len.  every packet should have this
    // 1 byte fix header, 4 bytes remaining length (we allow the max of 4)
    // 2 bytes packet id, 2 bytes topic len, internal packet overhead
    expectedLen = 1 + 4 + 2 + 2 + sizeof(PktBuf_t);

    // create a single topic packet, check length calculation
    topics[0] = "topic";
    expectedLen += strlen(topics[0]);
    err = umqtt_Unsubscribe(h, 1, topics, NULL);
    TEST_ASSERT_EQUAL(UMQTT_ERR_BUFSIZE, err);
    TEST_ASSERT_EQUAL(1, mock_malloc_count);
    TEST_ASSERT_EQUAL(expectedLen, mock_malloc_in_size);

    // add a second topic
    mock_malloc_Reset();
    topics[1] = "topic2";
    expectedLen += 2 + strlen(topics[1]);
    err = umqtt_Unsubscribe(h, 2, topics, NULL);
    TEST_ASSERT_EQUAL(UMQTT_ERR_BUFSIZE, err);
    TEST_ASSERT_EQUAL(1, mock_malloc_count);
    TEST_ASSERT_EQUAL(expectedLen, mock_malloc_in_size);
}

TEST(Unsubscribe, EncodedLength)
{
    umqtt_Error_t err;
    uint16_t rem;
    const char *topics[1];

    // create a dummy subscribe topic of known length
    topicBuf[45] = 0;
    topics[0] = topicBuf;
    // remaining size is packetId + topic len + topic + qos
    rem = 2 + 2 + 45;

    // create a unsubscribe packet and verify encoded length
    mock_malloc_shouldReturn[0] = pktBuf;
    err = umqtt_Unsubscribe(h, 1, topics, NULL);
    TEST_ASSERT_EQUAL(UMQTT_ERR_NETWORK, err);
    TEST_ASSERT_EQUAL(1, mock_malloc_count);
    TEST_ASSERT_NOT_EQUAL(0, mock_malloc_in_size);
    TEST_ASSERT_TRUE(mock_NetWrite_wasCalled);
    TEST_ASSERT_EQUAL(rem, mock_NetWrite_in_pBuf[1]);

    // create one that uses two bytes for remaining length
    mock_malloc_Reset();
    mock_NetWrite_Reset();
    topicBuf[45] = 'A';
    topicBuf[201] = 0;
    rem = 2 + 2 + 201;
    mock_malloc_shouldReturn[0] = pktBuf;
    err = umqtt_Unsubscribe(h, 1, topics, NULL);
    TEST_ASSERT_EQUAL(UMQTT_ERR_NETWORK, err);
    TEST_ASSERT_EQUAL(1, mock_malloc_count);
    TEST_ASSERT_NOT_EQUAL(0, mock_malloc_in_size);
    TEST_ASSERT_TRUE(mock_NetWrite_wasCalled);
    TEST_ASSERT_EQUAL(0xCD, mock_NetWrite_in_pBuf[1]);
    TEST_ASSERT_EQUAL(1, mock_NetWrite_in_pBuf[2]);
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
    umqtt_Error_t err;
    const char *topics[1];
    uint16_t msgId = 5555;

    unsigned int pktSize = sizeof(unsubscribePacket0);
    mock_NetWrite_shouldReturn = pktSize;
    mock_malloc_shouldReturn[0] = pktBuf;
    topics[0] = "topic";
    err = umqtt_Unsubscribe(h, 1, topics, &msgId);
    TEST_ASSERT_EQUAL(UMQTT_ERR_OK, err);
    TEST_ASSERT_NOT_EQUAL(0, msgId);
    TEST_ASSERT_NOT_EQUAL(5555, msgId);

    // verify expected allocations/free
    TEST_ASSERT_EQUAL(1, mock_malloc_count);
    TEST_ASSERT_NOT_EQUAL(0, mock_malloc_in_size);
    TEST_ASSERT_FALSE(mock_free_wasCalled);

    // verify net write was called and with expected size
    TEST_ASSERT_TRUE(mock_NetWrite_wasCalled);
    TEST_ASSERT_EQUAL(pktSize, mock_NetWrite_in_len);
    TEST_ASSERT_FALSE(mock_NetWrite_in_isMore);

    // check the packet contents
    TEST_ASSERT_EQUAL_MEMORY(unsubscribePacket0, mock_NetWrite_in_pBuf, pktSize);
}

TEST(Unsubscribe, MultiTopic)
{
    umqtt_Error_t err;
    const char *topics[2];
    uint16_t msgId = 5555;

    unsigned int pktSize = sizeof(unsubscribePacket1);
    mock_NetWrite_shouldReturn = pktSize;
    mock_malloc_shouldReturn[0] = pktBuf;
    topics[0] = "topic";
    topics[1] = "topic2";
    err = umqtt_Unsubscribe(h, 2, topics, &msgId);
    TEST_ASSERT_EQUAL(UMQTT_ERR_OK, err);
    TEST_ASSERT_NOT_EQUAL(0, msgId);
    TEST_ASSERT_NOT_EQUAL(5555, msgId);

    // verify expected allocations/free
    TEST_ASSERT_EQUAL(1, mock_malloc_count);
    TEST_ASSERT_NOT_EQUAL(0, mock_malloc_in_size);
    TEST_ASSERT_FALSE(mock_free_wasCalled);

    // verify net write was called and with expected size
    TEST_ASSERT_TRUE(mock_NetWrite_wasCalled);
    TEST_ASSERT_EQUAL(pktSize, mock_NetWrite_in_len);
    TEST_ASSERT_FALSE(mock_NetWrite_in_isMore);

    // check the packet contents
    TEST_ASSERT_EQUAL_MEMORY(unsubscribePacket1, mock_NetWrite_in_pBuf, pktSize);
}

TEST_GROUP_RUNNER(Unsubscribe)
{
    RUN_TEST_CASE(Unsubscribe, NullParms);
    RUN_TEST_CASE(Unsubscribe, CalcLength);
    RUN_TEST_CASE(Unsubscribe, EncodedLength);
    RUN_TEST_CASE(Unsubscribe, SingleTopic);
    RUN_TEST_CASE(Unsubscribe, MultiTopic);
}
