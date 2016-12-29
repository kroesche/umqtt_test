
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>

#include "unity_fixture.h"
#include "umqtt/umqtt.h"
#include "umqtt_mocks.h"
#include "umqtt_wrapper.h"

TEST_GROUP(Subscribe);

static umqtt_Handle_t h = NULL;
static void *instBuf = NULL;
#define SIZE_INSTBUF 200
static uint8_t *pktBuf = NULL;
#define SIZE_PKTBUF 1024
static char *topicBuf = NULL;
#define SIZE_TOPICBUF 256

TEST_SETUP(Subscribe)
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

TEST_TEAR_DOWN(Subscribe)
{
    if (instBuf) { free(instBuf); instBuf = NULL; }
    if (pktBuf) { free(pktBuf); pktBuf = NULL; }
    if (topicBuf) { free(topicBuf); topicBuf = NULL; }
    h = NULL;
}

TEST(Subscribe, NullParms)
{
    umqtt_Error_t err = umqtt_Subscribe(NULL, 0, NULL, NULL, NULL);
    TEST_ASSERT_EQUAL(UMQTT_ERR_PARM, err);
    err = umqtt_Subscribe(h, 0, NULL, NULL, NULL);
    TEST_ASSERT_EQUAL(UMQTT_ERR_PARM, err);
    TEST_ASSERT_EQUAL(0, mock_malloc_count);
}

TEST(Subscribe, CalcLength)
{
    umqtt_Error_t err;
    uint16_t expectedLen;
    const char *topics[2];
    uint8_t qoss[2];

    // base value for expected len.  every packet should have this
    // 1 byte fix header, 4 bytes remaining length (we allow the max of 4)
    // 2 bytes packet id, 2 bytes topic len, 1 byte qos, internal packet overhead
    expectedLen = 1 + 4 + 2 + 2 + 1 + sizeof(PktBuf_t);

    // create a single topic/qos packet, check length calculation
    // since malloc fails, it will not try to send a packet but we
    // can verify the requested size
    topics[0] = "topic";
    expectedLen += strlen(topics[0]);
    qoss[0] = 0;
    err = umqtt_Subscribe(h, 1, topics, qoss, NULL);
    TEST_ASSERT_EQUAL(UMQTT_ERR_BUFSIZE, err);
    TEST_ASSERT_EQUAL(1, mock_malloc_count);
    TEST_ASSERT_EQUAL(expectedLen, mock_malloc_in_size);

    // add a second topic and qos
    mock_malloc_Reset();
    topics[1] = "topic2";
    qoss[1] = 1;
    // add another 2 bytes topic len plus 1 byte qos plus strlen
    expectedLen += 2 + 1 + strlen(topics[1]);
    err = umqtt_Subscribe(h, 2, topics, qoss, NULL);
    TEST_ASSERT_EQUAL(UMQTT_ERR_BUFSIZE, err);
    TEST_ASSERT_EQUAL(1, mock_malloc_count);
    TEST_ASSERT_EQUAL(expectedLen, mock_malloc_in_size);
}

TEST(Subscribe, EncodedLength)
{
    umqtt_Error_t err;
    uint16_t rem;
    const char *topics[1];
    uint8_t qoss[1];

    // create a dummy subscribe topic of known length
    topicBuf[45] = 0;
    topics[0] = topicBuf;
    qoss[0] = 0;
    // remaining size is packetId + topic len + topic + qos
    rem = 2 + 2 + 45 + 1;

    // create a subscribe packet and verify encoded length
    mock_malloc_shouldReturn[0] = pktBuf;
    err = umqtt_Subscribe(h, 1, topics, qoss, NULL);
    TEST_ASSERT_EQUAL(UMQTT_ERR_NETWORK, err);
    TEST_ASSERT_EQUAL(1, mock_malloc_count);
    TEST_ASSERT_NOT_EQUAL(0, mock_malloc_in_size);
    TEST_ASSERT_TRUE(mock_NetWrite_wasCalled);
    TEST_ASSERT_EQUAL(rem, mock_NetWrite_in_pBuf[1]);

    // create one that uses two bytes for remaining length
    mock_malloc_Reset();
    mock_NetWrite_Reset();
    topicBuf[45] = 'A';
    topicBuf[200] = 0;
    rem = 2 + 2 + 200 + 1;
    mock_malloc_shouldReturn[0] = pktBuf;
    err = umqtt_Subscribe(h, 1, topics, qoss, NULL);
    TEST_ASSERT_EQUAL(UMQTT_ERR_NETWORK, err);
    TEST_ASSERT_EQUAL(1, mock_malloc_count);
    TEST_ASSERT_NOT_EQUAL(0, mock_malloc_in_size);
    TEST_ASSERT_TRUE(mock_NetWrite_wasCalled);
    TEST_ASSERT_EQUAL(0xCD, mock_NetWrite_in_pBuf[1]);
    TEST_ASSERT_EQUAL(1, mock_NetWrite_in_pBuf[2]);
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
    umqtt_Error_t err;
    const char *topics[1];
    uint8_t qoss[1];
    uint16_t msgId = 5555;

    unsigned int pktSize = sizeof(subscribePacket0);
    mock_NetWrite_shouldReturn = pktSize;
    mock_malloc_shouldReturn[0] = pktBuf;
    topics[0] = "topic";
    qoss[0] = 0;
    err = umqtt_Subscribe(h, 1, topics, qoss, &msgId);
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
    TEST_ASSERT_EQUAL_MEMORY(subscribePacket0, mock_NetWrite_in_pBuf, pktSize);
}

TEST(Subscribe, MultiTopic)
{
    umqtt_Error_t err;
    const char *topics[2];
    uint8_t qoss[2];
    uint16_t msgId = 5555;

    unsigned int pktSize = sizeof(subscribePacket1);
    mock_NetWrite_shouldReturn = pktSize;
    mock_malloc_shouldReturn[0] = pktBuf;
    topics[0] = "topic";
    qoss[0] = 0;
    topics[1] = "topic2";
    qoss[1] = 1;
    err = umqtt_Subscribe(h, 2, topics, qoss, &msgId);
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
    TEST_ASSERT_EQUAL_MEMORY(subscribePacket1, mock_NetWrite_in_pBuf, pktSize);
}

TEST_GROUP_RUNNER(Subscribe)
{
    RUN_TEST_CASE(Subscribe, NullParms);
    RUN_TEST_CASE(Subscribe, CalcLength);
    RUN_TEST_CASE(Subscribe, EncodedLength);
    RUN_TEST_CASE(Subscribe, SingleTopic);
    RUN_TEST_CASE(Subscribe, MultiTopic);
}
