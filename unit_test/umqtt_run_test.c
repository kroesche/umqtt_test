
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>

#include "unity_fixture.h"
#include "umqtt/umqtt.h"
#include "umqtt_mocks.h"
#include "umqtt_wrapper.h"

/*
run test cases

-basic rx and decode
-basic rx network error

-ping timeout (pings sent normally)
-ping timeout w network error
-ping timeout resume after network error

-nominal connect/connack
-connect timeout

-nominal publish/puback
-publish retry
-publish timeout

 */

TEST_GROUP(Run);

static umqtt_Handle_t h = NULL;
static void *instBuf = NULL;
#define SIZE_INSTBUF 200
static uint8_t *pktBuf = NULL;
#define SIZE_PKTBUF 1024
static char *clientBuf = NULL;
#define SIZE_CLIENTBUF 256

static umqtt_Handle_t Pingresp_h;
static void *Pingresp_pUser;
static void Pingresp_Reset(void) { Pingresp_h = NULL; Pingresp_pUser = NULL; }
static void
PingrespCb(umqtt_Handle_t h, void *pUser)
{   Pingresp_h = h; Pingresp_pUser = pUser; }

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

static umqtt_Callbacks_t callbacks =
{
    ConnackCb, /*PublishCb*/NULL, PubackCb, SubackCb, UnsubackCb, PingrespCb
};

TEST_SETUP(Run)
{
    // allocate memory for the instance that will be created
    // init the instance so it will be ready for all tests
    mock_malloc_Reset();
    instBuf = malloc(SIZE_INSTBUF);
    mock_hNet = &mock_hNet;
    transportConfig.hNet = mock_hNet;
    mock_malloc_shouldReturn[0] = instBuf;
    h = umqtt_New(&transportConfig, &callbacks, NULL);
    TEST_ASSERT_NOT_NULL(h);
    // clear all mocks
    mock_malloc_Reset();
    mock_free_Reset();
    mock_NetRead_Reset();
    mock_NetWrite_Reset();
    // ready a buffer that can be used for packet allocation
    pktBuf = malloc(SIZE_PKTBUF);
    TEST_ASSERT_NOT_NULL(pktBuf);
    Pingresp_Reset();
    Connack_Reset();
    Puback_Reset();
    Suback_Reset();
    Unsuback_Reset();
    clientBuf = malloc(SIZE_CLIENTBUF);
    TEST_ASSERT_NOT_NULL(clientBuf);
    memset(clientBuf, 'A', SIZE_CLIENTBUF); // used for dummy strings
    // fake an existing connection
//    wrap_setConnected(h, true);
}

TEST_TEAR_DOWN(Run)
{
    if (instBuf) { free(instBuf); instBuf = NULL; }
    if (pktBuf) { free(pktBuf); pktBuf = NULL; }
    if (clientBuf) { free(clientBuf); clientBuf = NULL; }
    h = NULL;
}

void
initiateConnect(void)
{
    umqtt_Error_t err;
    // lazy way to get required packet length
    mock_malloc_shouldReturn[0] = pktBuf;
    mock_malloc_shouldReturn[1] = &pktBuf[900];
    err = umqtt_Connect(h, false, false, 0, 30, "testClient", NULL, NULL, 0, NULL, NULL);
    TEST_ASSERT_EQUAL(UMQTT_ERR_NETWORK, err);
    size_t len = mock_NetWrite_in_len;

    // set up malloc and net write for successful connect in progress
    // then initiate a connect
    mock_malloc_Reset();
    mock_NetWrite_shouldReturn = len;
    mock_malloc_shouldReturn[0] = pktBuf;
    mock_malloc_shouldReturn[1] = &pktBuf[900];
    err = umqtt_Connect(h, false, false, 0, 30, "testClient", NULL, NULL, 0, NULL, NULL);
    TEST_ASSERT_EQUAL(UMQTT_ERR_OK, err);
    // we dont need to check anything else here.  this process is
    // thoroughly checked by another unit test
    // getting here means that a connect was initiated and should now
    // be in progress
    err = umqtt_GetConnectedStatus(h);
    TEST_ASSERT_EQUAL(UMQTT_ERR_CONNECT_PENDING, err);
    // clean up
    mock_malloc_Reset();
    mock_free_Reset();
    mock_NetWrite_Reset();
}

void
completeConnect(void)
{
    umqtt_Error_t err;
    pktBuf[0] = 2 << 4; // connack
    pktBuf[1] = 2; // remaining length
    pktBuf[2] = 0; // flags
    pktBuf[3] = 0; // ret code
    err = umqtt_DecodePacket(h, pktBuf, 4);
    TEST_ASSERT_EQUAL(UMQTT_ERR_OK, err);
    TEST_ASSERT_EQUAL_PTR(instBuf, Connack_h);
    PktBuf_t *pkt = wrap_getNextPktBuf(h);
    TEST_ASSERT_NULL(pkt);
    err = umqtt_GetConnectedStatus(h);
    TEST_ASSERT_EQUAL(UMQTT_ERR_CONNECTED, err);
    // clean up
    mock_malloc_Reset();
    mock_free_Reset();
    mock_NetWrite_Reset();
}

TEST(Run, NothingHappens)
{
    initiateConnect();
    completeConnect();
    umqtt_Error_t err;
    // call run
    err = umqtt_Run(h, 1000);
    TEST_ASSERT_EQUAL(UMQTT_ERR_OK, err);
    // verify all mocks, no calls (read, write, malloc, free)
    TEST_ASSERT_EQUAL(0, mock_malloc_count);
    TEST_ASSERT_FALSE(mock_free_wasCalled);
    TEST_ASSERT_FALSE(mock_NetWrite_wasCalled);
    TEST_ASSERT_TRUE(mock_NetRead_wasCalled);

    // call run with updated ticks
    err = umqtt_Run(h, 10000);
    TEST_ASSERT_EQUAL(UMQTT_ERR_OK, err);
    // verify all mocks, no calls (read, write, malloc, free)
    TEST_ASSERT_EQUAL(0, mock_malloc_count);
    TEST_ASSERT_FALSE(mock_free_wasCalled);
    TEST_ASSERT_FALSE(mock_NetWrite_wasCalled);
    TEST_ASSERT_TRUE(mock_NetRead_wasCalled);
}

TEST(Run, BasicRx)
{
    initiateConnect();
    completeConnect();
    umqtt_Error_t err;
    // stage a pingresp packet
    pktBuf[0] = 13 << 4; // pingresp
    pktBuf[1] = 0; // rem len
    mock_NetRead_shouldReturn_pBuf = pktBuf;
    mock_NetRead_shouldReturn = 2;
    err = umqtt_Run(h, 1000);
    TEST_ASSERT_EQUAL(UMQTT_ERR_OK, err);
    // verify pingresp callback was called
    TEST_ASSERT_TRUE(mock_NetRead_wasCalled);
    TEST_ASSERT_EQUAL_PTR(instBuf, Pingresp_h);
}

TEST(Run, RxNetError)
{
    initiateConnect();
    completeConnect();
    umqtt_Error_t err;
    mock_NetRead_shouldReturn = -1;
    err = umqtt_Run(h, 1000);
    TEST_ASSERT_EQUAL(UMQTT_ERR_NETWORK, err);
    // verify pingresp callback not called
    TEST_ASSERT_TRUE(mock_NetRead_wasCalled);
    TEST_ASSERT_NULL(Pingresp_h);
}

static const uint8_t pingPacket[] =
{ 12 << 4, 0 };

// ping sent after normal timeout
TEST(Run, Ping)
{
    initiateConnect();
    completeConnect();
    umqtt_Error_t err;

    // initial call to run - nothing happens
    err = umqtt_Run(h, 1000);
    TEST_ASSERT_EQUAL(UMQTT_ERR_OK, err);
    TEST_ASSERT_EQUAL(0, mock_malloc_count);
    TEST_ASSERT_FALSE(mock_NetWrite_wasCalled);

    // set up to expect tx of 2 bytes packet
    mock_NetWrite_pCopy = pktBuf;
    mock_NetWrite_copyLen = 2;
    mock_NetWrite_shouldReturn = 2;
    err = umqtt_Run(h, 20000); // 20 seconds
    TEST_ASSERT_EQUAL(UMQTT_ERR_OK, err);

    // verify 2 byte ping packet was generated
    TEST_ASSERT_EQUAL(0, mock_malloc_count);
    TEST_ASSERT_TRUE(mock_NetWrite_wasCalled);
    TEST_ASSERT_EQUAL(2, mock_NetWrite_in_len);
    TEST_ASSERT_EQUAL_MEMORY(pingPacket, pktBuf, 2);
}

// cause normal ping, then verify no ping sent with
// ticks 1 less than needed for next ping
TEST(Run, PingExclusive)
{
    initiateConnect();
    completeConnect();
    umqtt_Error_t err;

    // do run with ticks at 20 seconds
    // should cause pingreq
    mock_NetWrite_pCopy = pktBuf;
    mock_NetWrite_copyLen = 2;
    mock_NetWrite_shouldReturn = 2;
    err = umqtt_Run(h, 20000); // 20 seconds
    TEST_ASSERT_EQUAL(UMQTT_ERR_OK, err);

    // verify 2 byte ping packet was generated
    TEST_ASSERT_EQUAL(0, mock_malloc_count);
    TEST_ASSERT_TRUE(mock_NetWrite_wasCalled);
    TEST_ASSERT_EQUAL(2, mock_NetWrite_in_len);
    TEST_ASSERT_EQUAL_MEMORY(pingPacket, pktBuf, 2);
    mock_NetWrite_Reset();

    // keepalive is 30 second, so next pingreq should be at 35000
    mock_NetWrite_pCopy = pktBuf;
    mock_NetWrite_copyLen = 2;
    mock_NetWrite_shouldReturn = 2;
    err = umqtt_Run(h, 34999); // just under 15 seconds
    TEST_ASSERT_EQUAL(UMQTT_ERR_OK, err);

    // nothing happens
    TEST_ASSERT_EQUAL(0, mock_malloc_count);
    TEST_ASSERT_FALSE(mock_NetWrite_wasCalled);
}

// cause normal ping, then verify next ping sent with
// ticks exactly needed for next ping
TEST(Run, PingInclusive)
{
    initiateConnect();
    completeConnect();
    umqtt_Error_t err;

    // do run with ticks at 20 seconds
    // should cause pingreq
    mock_NetWrite_pCopy = pktBuf;
    mock_NetWrite_copyLen = 2;
    mock_NetWrite_shouldReturn = 2;
    err = umqtt_Run(h, 20000); // 20 seconds
    TEST_ASSERT_EQUAL(UMQTT_ERR_OK, err);

    // verify 2 byte ping packet was generated
    TEST_ASSERT_EQUAL(0, mock_malloc_count);
    TEST_ASSERT_TRUE(mock_NetWrite_wasCalled);
    TEST_ASSERT_EQUAL(2, mock_NetWrite_in_len);
    TEST_ASSERT_EQUAL_MEMORY(pingPacket, pktBuf, 2);
    mock_NetWrite_Reset();

    // keepalive is 30 second, so next pingreq should be at 35000
    mock_NetWrite_pCopy = pktBuf;
    mock_NetWrite_copyLen = 2;
    mock_NetWrite_shouldReturn = 2;
    err = umqtt_Run(h, 35001); // just over 15 seconds
    TEST_ASSERT_EQUAL(UMQTT_ERR_OK, err);

    // verify 2 byte ping packet was generated
    TEST_ASSERT_EQUAL(0, mock_malloc_count);
    TEST_ASSERT_TRUE(mock_NetWrite_wasCalled);
    TEST_ASSERT_EQUAL(2, mock_NetWrite_in_len);
    TEST_ASSERT_EQUAL_MEMORY(pingPacket, pktBuf, 2);
}

// normal ping,
// then network error, verify no ping
// then network resumed, verify new ping
TEST(Run, PingNetError)
{
    initiateConnect();
    completeConnect();
    umqtt_Error_t err;

    // do run with ticks at 20 seconds
    // should cause pingreq
    mock_NetWrite_pCopy = pktBuf;
    mock_NetWrite_copyLen = 2;
    mock_NetWrite_shouldReturn = 2;
    err = umqtt_Run(h, 20000); // 20 seconds
    TEST_ASSERT_EQUAL(UMQTT_ERR_OK, err);

    // verify 2 byte ping packet was generated
    TEST_ASSERT_EQUAL(0, mock_malloc_count);
    TEST_ASSERT_TRUE(mock_NetWrite_wasCalled);
    TEST_ASSERT_EQUAL(2, mock_NetWrite_in_len);
    TEST_ASSERT_EQUAL_MEMORY(pingPacket, pktBuf, 2);
    mock_NetWrite_Reset();

    // run again which should cause another ping, except net write
    // does not cooperate.  should not send a packet
    err = umqtt_Run(h, 40000);
    TEST_ASSERT_EQUAL(UMQTT_ERR_NETWORK, err);

    // nothing happens, but net write was called
    TEST_ASSERT_EQUAL(0, mock_malloc_count);
    TEST_ASSERT_TRUE(mock_NetWrite_wasCalled);
    mock_NetWrite_Reset();

    // one more time with network enabled again
    mock_NetWrite_pCopy = pktBuf;
    mock_NetWrite_copyLen = 2;
    mock_NetWrite_shouldReturn = 2;
    err = umqtt_Run(h, 60000);
    TEST_ASSERT_EQUAL(UMQTT_ERR_OK, err);

    // verify 2 byte ping packet was generated
    TEST_ASSERT_EQUAL(0, mock_malloc_count);
    TEST_ASSERT_TRUE(mock_NetWrite_wasCalled);
    TEST_ASSERT_EQUAL(2, mock_NetWrite_in_len);
    TEST_ASSERT_EQUAL_MEMORY(pingPacket, pktBuf, 2);
}

// initiate connect but no connack within timeout
TEST(Run, ConnectTimeout)
{
    initiateConnect();
    umqtt_Error_t err;
    PktBuf_t *savedBuf = wrap_getNextPktBuf(h);
    TEST_ASSERT_NOT_NULL(savedBuf);

    // run a few times with ticks less than timeout, verify ok
    err = umqtt_Run(h, 1000);
    TEST_ASSERT_EQUAL(UMQTT_ERR_OK, err);
    err = umqtt_Run(h, 2000);
    TEST_ASSERT_EQUAL(UMQTT_ERR_OK, err);
    err = umqtt_Run(h, 4000);
    TEST_ASSERT_EQUAL(UMQTT_ERR_OK, err);
    err = umqtt_GetConnectedStatus(h);
    TEST_ASSERT_EQUAL(UMQTT_ERR_CONNECT_PENDING, err);
    savedBuf = wrap_getNextPktBuf(h);
    TEST_ASSERT_NOT_NULL(savedBuf);

    // now go past timeout and verify timeout is returned
    err = umqtt_Run(h, 5001);
    TEST_ASSERT_EQUAL(UMQTT_ERR_TIMEOUT, err);
    // stored packet should have been freed
    savedBuf = wrap_getNextPktBuf(h);
    TEST_ASSERT_NULL(savedBuf);
    err = umqtt_GetConnectedStatus(h);
    TEST_ASSERT_EQUAL(UMQTT_ERR_DISCONNECTED, err);

    // check expected state of other functions
    TEST_ASSERT_EQUAL(0, mock_malloc_count);
    TEST_ASSERT_TRUE(mock_free_wasCalled);
    TEST_ASSERT_FALSE(mock_NetWrite_wasCalled);
}

// initiate connect and connack within timeout
TEST(Run, Connect)
{
    initiateConnect();
    umqtt_Error_t err;
    PktBuf_t *savedBuf = wrap_getNextPktBuf(h);
    TEST_ASSERT_NOT_NULL(savedBuf);

    // run a few times with ticks less than timeout, verify ok
    err = umqtt_Run(h, 1000);
    TEST_ASSERT_EQUAL(UMQTT_ERR_OK, err);
    err = umqtt_Run(h, 2000);
    TEST_ASSERT_EQUAL(UMQTT_ERR_OK, err);
    err = umqtt_Run(h, 4000);
    TEST_ASSERT_EQUAL(UMQTT_ERR_OK, err);
    // verify connect is still pending
    savedBuf = wrap_getNextPktBuf(h);
    TEST_ASSERT_NOT_NULL(savedBuf);
    err = umqtt_GetConnectedStatus(h);
    TEST_ASSERT_EQUAL(UMQTT_ERR_CONNECT_PENDING, err);

    // complete the connect and verify connect no longer pending
    // verify pending connect is cleared
    completeConnect();
    // stored packet should have been freed
    savedBuf = wrap_getNextPktBuf(h);
    TEST_ASSERT_NULL(savedBuf);
    err = umqtt_GetConnectedStatus(h);
    TEST_ASSERT_EQUAL(UMQTT_ERR_CONNECTED, err);
    err = umqtt_Run(h, 6000);
    TEST_ASSERT_EQUAL(UMQTT_ERR_OK, err);
}

// only test qos 1.  qos 0 was essentially tested with publish unit test
// so there is no point to repeat that here.  qos 2 is not supported at this
// time

// verify packet times out and retries
TEST(Run, PublishTimeout)
{
    umqtt_Error_t err;

    initiateConnect();
    completeConnect();
    PktBuf_t *savedBuf = wrap_getNextPktBuf(h);
    TEST_ASSERT_NULL(savedBuf);

    // for this test, set a really long keep alive which prevents
    // ping packets from being generated, which interferes with the
    // test below
    wrap_setKeepAlive(h, 1000);

    // lazy way to get required packet length
    mock_malloc_shouldReturn[0] = pktBuf;
    err = umqtt_Publish(h, "topic", (uint8_t *)"message", sizeof("message"), 1, false, NULL);
    TEST_ASSERT_EQUAL(UMQTT_ERR_NETWORK, err);
    size_t len = mock_NetWrite_in_len;

    // now send the publish packet
    mock_malloc_Reset();
    mock_NetWrite_shouldReturn = len;
    mock_malloc_shouldReturn[0] = pktBuf;
    err = umqtt_Publish(h, "topic", (uint8_t *)"message", sizeof("message"), 1, false, NULL);
    TEST_ASSERT_EQUAL(UMQTT_ERR_OK, err);

    // verify publish packet is hanging around
    // and reset mocks to be ready for next step
    savedBuf = wrap_getNextPktBuf(h);
    TEST_ASSERT_NOT_NULL(savedBuf);
    mock_malloc_Reset();
    mock_free_Reset();
    mock_NetWrite_Reset();

    // run through half the timeout interval, verify nothing happens
    err = umqtt_Run(h, 2500);
    TEST_ASSERT_EQUAL(UMQTT_ERR_OK, err);
    savedBuf = wrap_getNextPktBuf(h);
    TEST_ASSERT_NOT_NULL(savedBuf);
    err = umqtt_GetConnectedStatus(h);
    TEST_ASSERT_EQUAL(UMQTT_ERR_CONNECTED, err);
    TEST_ASSERT_EQUAL(0, mock_malloc_count);
    TEST_ASSERT_FALSE(mock_free_wasCalled);
    TEST_ASSERT_FALSE(mock_NetWrite_wasCalled);

    // now exceed the timeout and verify the packet was resent
    mock_NetWrite_pCopy = (uint8_t*)clientBuf;
    mock_NetWrite_copyLen = 1;
    mock_NetWrite_shouldReturn = len;
    err = umqtt_Run(h, 5001);
    TEST_ASSERT_EQUAL(UMQTT_ERR_OK, err);
    savedBuf = wrap_getNextPktBuf(h);
    TEST_ASSERT_NOT_NULL(savedBuf);
    err = umqtt_GetConnectedStatus(h);
    TEST_ASSERT_EQUAL(UMQTT_ERR_CONNECTED, err);
    TEST_ASSERT_EQUAL(0, mock_malloc_count);
    TEST_ASSERT_FALSE(mock_free_wasCalled);
    TEST_ASSERT_TRUE(mock_NetWrite_wasCalled);
    TEST_ASSERT_EQUAL(len, mock_NetWrite_in_len);
    TEST_ASSERT_EQUAL(0x32, clientBuf[0]); // check first byte of resent packet
}

// verify pending publish clears after ack
TEST(Run, Publish)
{
    umqtt_Error_t err;
    uint16_t msgId;

    initiateConnect();
    completeConnect();
    PktBuf_t *savedBuf = wrap_getNextPktBuf(h);
    TEST_ASSERT_NULL(savedBuf);

    // for this test, set a really long keep alive which prevents
    // ping packets from being generated, which interferes with the
    // test below
    wrap_setKeepAlive(h, 1000);

    // lazy way to get required packet length
    mock_malloc_shouldReturn[0] = pktBuf;
    err = umqtt_Publish(h, "topic", (uint8_t *)"message", sizeof("message"), 1, false, NULL);
    TEST_ASSERT_EQUAL(UMQTT_ERR_NETWORK, err);
    size_t len = mock_NetWrite_in_len;

    // now send the publish packet
    mock_malloc_Reset();
    mock_NetWrite_shouldReturn = len;
    mock_malloc_shouldReturn[0] = pktBuf;
    err = umqtt_Publish(h, "topic", (uint8_t *)"message", sizeof("message"), 1, false, &msgId);
    TEST_ASSERT_EQUAL(UMQTT_ERR_OK, err);

    // verify publish packet is hanging around
    // and reset mocks to be ready for next step
    savedBuf = wrap_getNextPktBuf(h);
    TEST_ASSERT_NOT_NULL(savedBuf);
    mock_malloc_Reset();
    mock_free_Reset();
    mock_NetWrite_Reset();

    // execute run loop once and verify nothing happens
    err = umqtt_Run(h, 2500);
    TEST_ASSERT_EQUAL(UMQTT_ERR_OK, err);
    savedBuf = wrap_getNextPktBuf(h);
    TEST_ASSERT_NOT_NULL(savedBuf);
    err = umqtt_GetConnectedStatus(h);
    TEST_ASSERT_EQUAL(UMQTT_ERR_CONNECTED, err);
    TEST_ASSERT_EQUAL(0, mock_malloc_count);
    TEST_ASSERT_FALSE(mock_free_wasCalled);
    TEST_ASSERT_FALSE(mock_NetWrite_wasCalled);

    // set up to receive puback
    pktBuf[900] = 4 << 4; // puback
    pktBuf[901] = 2; // remaining length
    pktBuf[902] = msgId << 8; // packet ID that was saved earlier
    pktBuf[903] = msgId & 0xFF;
    mock_NetRead_shouldReturn = 4;
    mock_NetRead_shouldReturn_pBuf = &pktBuf[900];

    // execute run loop which should cause receipt of puback and
    // free the pending publish
    err = umqtt_Run(h, 3500);
    TEST_ASSERT_EQUAL(UMQTT_ERR_OK, err);
    savedBuf = wrap_getNextPktBuf(h);
    TEST_ASSERT_NULL(savedBuf); // pending connect packet was freed
    TEST_ASSERT_EQUAL_PTR(instBuf, Puback_h);
    TEST_ASSERT_TRUE(mock_free_wasCalled);

    // run once more with ticks past the retry timeout
    // verify nothing happens
    mock_malloc_Reset();
    mock_free_Reset();
    mock_NetWrite_Reset();
    mock_NetRead_Reset();
    err = umqtt_Run(h, 5500);
    TEST_ASSERT_EQUAL(UMQTT_ERR_OK, err);
    savedBuf = wrap_getNextPktBuf(h);
    TEST_ASSERT_NULL(savedBuf);
    err = umqtt_GetConnectedStatus(h);
    TEST_ASSERT_EQUAL(UMQTT_ERR_CONNECTED, err);
    TEST_ASSERT_EQUAL(0, mock_malloc_count);
    TEST_ASSERT_FALSE(mock_free_wasCalled);
    TEST_ASSERT_FALSE(mock_NetWrite_wasCalled);
}

// verify packet times out and retries
TEST(Run, SubscribeTimeout)
{
    umqtt_Error_t err;
    const char *topics[1];
    uint8_t qoss[1];
    topics[0] = "topic";
    qoss[0] = 0;

    initiateConnect();
    completeConnect();
    PktBuf_t *savedBuf = wrap_getNextPktBuf(h);
    TEST_ASSERT_NULL(savedBuf);

    // for this test, set a really long keep alive which prevents
    // ping packets from being generated, which interferes with the
    // test below
    wrap_setKeepAlive(h, 1000);

    // lazy way to get required packet length
    mock_malloc_shouldReturn[0] = pktBuf;
    err = umqtt_Subscribe(h, 1, topics, qoss, NULL);
    TEST_ASSERT_EQUAL(UMQTT_ERR_NETWORK, err);
    size_t len = mock_NetWrite_in_len;

    // now send the subscribe packet
    mock_malloc_Reset();
    mock_NetWrite_shouldReturn = len;
    mock_malloc_shouldReturn[0] = pktBuf;
    err = umqtt_Subscribe(h, 1, topics, qoss, NULL);
    TEST_ASSERT_EQUAL(UMQTT_ERR_OK, err);

    // verify subscribe packet is hanging around
    // and reset mocks to be ready for next step
    savedBuf = wrap_getNextPktBuf(h);
    TEST_ASSERT_NOT_NULL(savedBuf);
    mock_malloc_Reset();
    mock_free_Reset();
    mock_NetWrite_Reset();

    // run through half the timeout interval, verify nothing happens
    err = umqtt_Run(h, 2500);
    TEST_ASSERT_EQUAL(UMQTT_ERR_OK, err);
    savedBuf = wrap_getNextPktBuf(h);
    TEST_ASSERT_NOT_NULL(savedBuf);
    err = umqtt_GetConnectedStatus(h);
    TEST_ASSERT_EQUAL(UMQTT_ERR_CONNECTED, err);
    TEST_ASSERT_EQUAL(0, mock_malloc_count);
    TEST_ASSERT_FALSE(mock_free_wasCalled);
    TEST_ASSERT_FALSE(mock_NetWrite_wasCalled);

    // now exceed the timeout and verify the packet was resent
    mock_NetWrite_pCopy = (uint8_t*)clientBuf;
    mock_NetWrite_copyLen = 1;
    mock_NetWrite_shouldReturn = len;
    err = umqtt_Run(h, 5001);
    TEST_ASSERT_EQUAL(UMQTT_ERR_OK, err);
    savedBuf = wrap_getNextPktBuf(h);
    TEST_ASSERT_NOT_NULL(savedBuf);
    err = umqtt_GetConnectedStatus(h);
    TEST_ASSERT_EQUAL(UMQTT_ERR_CONNECTED, err);
    TEST_ASSERT_EQUAL(0, mock_malloc_count);
    TEST_ASSERT_FALSE(mock_free_wasCalled);
    TEST_ASSERT_TRUE(mock_NetWrite_wasCalled);
    TEST_ASSERT_EQUAL(len, mock_NetWrite_in_len);
    TEST_ASSERT_EQUAL_INT8(0x82, clientBuf[0]); // check first byte of resent packet
}

// verify pending clears after ack
TEST(Run, Subscribe)
{
    umqtt_Error_t err;
    uint16_t msgId;
    const char *topics[1];
    uint8_t qoss[1];
    topics[0] = "topic";
    qoss[0] = 0;

    initiateConnect();
    completeConnect();
    PktBuf_t *savedBuf = wrap_getNextPktBuf(h);
    TEST_ASSERT_NULL(savedBuf);

    // for this test, set a really long keep alive which prevents
    // ping packets from being generated, which interferes with the
    // test below
    wrap_setKeepAlive(h, 1000);

    // lazy way to get required packet length
    mock_malloc_shouldReturn[0] = pktBuf;
    err = umqtt_Subscribe(h, 1, topics, qoss, NULL);
    TEST_ASSERT_EQUAL(UMQTT_ERR_NETWORK, err);
    size_t len = mock_NetWrite_in_len;

    // now send the publish packet
    mock_malloc_Reset();
    mock_NetWrite_shouldReturn = len;
    mock_malloc_shouldReturn[0] = pktBuf;
    err = umqtt_Subscribe(h, 1, topics, qoss, &msgId);
    TEST_ASSERT_EQUAL(UMQTT_ERR_OK, err);

    // verify publish packet is hanging around
    // and reset mocks to be ready for next step
    savedBuf = wrap_getNextPktBuf(h);
    TEST_ASSERT_NOT_NULL(savedBuf);
    mock_malloc_Reset();
    mock_free_Reset();
    mock_NetWrite_Reset();

    // execute run loop once and verify nothing happens
    err = umqtt_Run(h, 2500);
    TEST_ASSERT_EQUAL(UMQTT_ERR_OK, err);
    savedBuf = wrap_getNextPktBuf(h);
    TEST_ASSERT_NOT_NULL(savedBuf);
    err = umqtt_GetConnectedStatus(h);
    TEST_ASSERT_EQUAL(UMQTT_ERR_CONNECTED, err);
    TEST_ASSERT_EQUAL(0, mock_malloc_count);
    TEST_ASSERT_FALSE(mock_free_wasCalled);
    TEST_ASSERT_FALSE(mock_NetWrite_wasCalled);

    // set up to receive suback
    pktBuf[900] = 9 << 4; // suback
    pktBuf[901] = 3; // remaining length
    pktBuf[902] = msgId << 8; // packet ID that was saved earlier
    pktBuf[903] = msgId & 0xFF;
    pktBuf[904] = 0;
    mock_NetRead_shouldReturn = 5;
    mock_NetRead_shouldReturn_pBuf = &pktBuf[900];

    // execute run loop which should cause receipt of suback and
    // free the pending subscribe
    err = umqtt_Run(h, 3500);
    TEST_ASSERT_EQUAL(UMQTT_ERR_OK, err);
    savedBuf = wrap_getNextPktBuf(h);
    TEST_ASSERT_NULL(savedBuf); // pending packet was freed
    TEST_ASSERT_EQUAL_PTR(instBuf, Suback_h);
    TEST_ASSERT_TRUE(mock_free_wasCalled);

    // run once more with ticks past the retry timeout
    // verify nothing happens
    mock_malloc_Reset();
    mock_free_Reset();
    mock_NetWrite_Reset();
    mock_NetRead_Reset();
    err = umqtt_Run(h, 5500);
    TEST_ASSERT_EQUAL(UMQTT_ERR_OK, err);
    savedBuf = wrap_getNextPktBuf(h);
    TEST_ASSERT_NULL(savedBuf);
    err = umqtt_GetConnectedStatus(h);
    TEST_ASSERT_EQUAL(UMQTT_ERR_CONNECTED, err);
    TEST_ASSERT_EQUAL(0, mock_malloc_count);
    TEST_ASSERT_FALSE(mock_free_wasCalled);
    TEST_ASSERT_FALSE(mock_NetWrite_wasCalled);
}

TEST(Run, UnsubscribeTimeout)
{
    umqtt_Error_t err;
    const char *topics[1];
    topics[0] = "topic";

    initiateConnect();
    completeConnect();
    PktBuf_t *savedBuf = wrap_getNextPktBuf(h);
    TEST_ASSERT_NULL(savedBuf);

    // for this test, set a really long keep alive which prevents
    // ping packets from being generated, which interferes with the
    // test below
    wrap_setKeepAlive(h, 1000);

    // lazy way to get required packet length
    mock_malloc_shouldReturn[0] = pktBuf;
    err = umqtt_Unsubscribe(h, 1, topics, NULL);
    TEST_ASSERT_EQUAL(UMQTT_ERR_NETWORK, err);
    size_t len = mock_NetWrite_in_len;

    // now send the unsubscribe packet
    mock_malloc_Reset();
    mock_NetWrite_shouldReturn = len;
    mock_malloc_shouldReturn[0] = pktBuf;
    err = umqtt_Unsubscribe(h, 1, topics, NULL);
    TEST_ASSERT_EQUAL(UMQTT_ERR_OK, err);

    // verify unsubscribe packet is hanging around
    // and reset mocks to be ready for next step
    savedBuf = wrap_getNextPktBuf(h);
    TEST_ASSERT_NOT_NULL(savedBuf);
    mock_malloc_Reset();
    mock_free_Reset();
    mock_NetWrite_Reset();

    // run through half the timeout interval, verify nothing happens
    err = umqtt_Run(h, 2500);
    TEST_ASSERT_EQUAL(UMQTT_ERR_OK, err);
    savedBuf = wrap_getNextPktBuf(h);
    TEST_ASSERT_NOT_NULL(savedBuf);
    err = umqtt_GetConnectedStatus(h);
    TEST_ASSERT_EQUAL(UMQTT_ERR_CONNECTED, err);
    TEST_ASSERT_EQUAL(0, mock_malloc_count);
    TEST_ASSERT_FALSE(mock_free_wasCalled);
    TEST_ASSERT_FALSE(mock_NetWrite_wasCalled);

    // now exceed the timeout and verify the packet was resent
    mock_NetWrite_pCopy = (uint8_t*)clientBuf;
    mock_NetWrite_copyLen = 1;
    mock_NetWrite_shouldReturn = len;
    err = umqtt_Run(h, 5001);
    TEST_ASSERT_EQUAL(UMQTT_ERR_OK, err);
    savedBuf = wrap_getNextPktBuf(h);
    TEST_ASSERT_NOT_NULL(savedBuf);
    err = umqtt_GetConnectedStatus(h);
    TEST_ASSERT_EQUAL(UMQTT_ERR_CONNECTED, err);
    TEST_ASSERT_EQUAL(0, mock_malloc_count);
    TEST_ASSERT_FALSE(mock_free_wasCalled);
    TEST_ASSERT_TRUE(mock_NetWrite_wasCalled);
    TEST_ASSERT_EQUAL(len, mock_NetWrite_in_len);
    TEST_ASSERT_EQUAL_INT8(0xA2, clientBuf[0]); // check first byte of resent packet
}

TEST(Run, Unsubscribe)
{
    umqtt_Error_t err;
    uint16_t msgId;
    const char *topics[1];
    topics[0] = "topic";

    initiateConnect();
    completeConnect();
    PktBuf_t *savedBuf = wrap_getNextPktBuf(h);
    TEST_ASSERT_NULL(savedBuf);

    // for this test, set a really long keep alive which prevents
    // ping packets from being generated, which interferes with the
    // test below
    wrap_setKeepAlive(h, 1000);

    // lazy way to get required packet length
    mock_malloc_shouldReturn[0] = pktBuf;
    err = umqtt_Unsubscribe(h, 1, topics, NULL);
    TEST_ASSERT_EQUAL(UMQTT_ERR_NETWORK, err);
    size_t len = mock_NetWrite_in_len;

    // now send the publish packet
    mock_malloc_Reset();
    mock_NetWrite_shouldReturn = len;
    mock_malloc_shouldReturn[0] = pktBuf;
    err = umqtt_Unsubscribe(h, 1, topics, &msgId);
    TEST_ASSERT_EQUAL(UMQTT_ERR_OK, err);

    // verify publish packet is hanging around
    // and reset mocks to be ready for next step
    savedBuf = wrap_getNextPktBuf(h);
    TEST_ASSERT_NOT_NULL(savedBuf);
    mock_malloc_Reset();
    mock_free_Reset();
    mock_NetWrite_Reset();

    // execute run loop once and verify nothing happens
    err = umqtt_Run(h, 2500);
    TEST_ASSERT_EQUAL(UMQTT_ERR_OK, err);
    savedBuf = wrap_getNextPktBuf(h);
    TEST_ASSERT_NOT_NULL(savedBuf);
    err = umqtt_GetConnectedStatus(h);
    TEST_ASSERT_EQUAL(UMQTT_ERR_CONNECTED, err);
    TEST_ASSERT_EQUAL(0, mock_malloc_count);
    TEST_ASSERT_FALSE(mock_free_wasCalled);
    TEST_ASSERT_FALSE(mock_NetWrite_wasCalled);

    // set up to receive unsuback
    pktBuf[900] = 11 << 4; // unsuback
    pktBuf[901] = 2; // remaining length
    pktBuf[902] = msgId << 8; // packet ID that was saved earlier
    pktBuf[903] = msgId & 0xFF;
    mock_NetRead_shouldReturn = 4;
    mock_NetRead_shouldReturn_pBuf = &pktBuf[900];

    // execute run loop which should cause receipt of suback and
    // free the pending subscribe
    err = umqtt_Run(h, 3500);
    TEST_ASSERT_EQUAL(UMQTT_ERR_OK, err);
    savedBuf = wrap_getNextPktBuf(h);
    TEST_ASSERT_NULL(savedBuf); // pending packet was freed
    TEST_ASSERT_EQUAL_PTR(instBuf, Unsuback_h);
    TEST_ASSERT_TRUE(mock_free_wasCalled);

    // run once more with ticks past the retry timeout
    // verify nothing happens
    mock_malloc_Reset();
    mock_free_Reset();
    mock_NetWrite_Reset();
    mock_NetRead_Reset();
    err = umqtt_Run(h, 5500);
    TEST_ASSERT_EQUAL(UMQTT_ERR_OK, err);
    savedBuf = wrap_getNextPktBuf(h);
    TEST_ASSERT_NULL(savedBuf);
    err = umqtt_GetConnectedStatus(h);
    TEST_ASSERT_EQUAL(UMQTT_ERR_CONNECTED, err);
    TEST_ASSERT_EQUAL(0, mock_malloc_count);
    TEST_ASSERT_FALSE(mock_free_wasCalled);
    TEST_ASSERT_FALSE(mock_NetWrite_wasCalled);
}

// confirm pending packet expires after number of retries
TEST(Run, PublishExpire)
{
    umqtt_Error_t err;

    initiateConnect();
    completeConnect();
    PktBuf_t *savedBuf = wrap_getNextPktBuf(h);
    TEST_ASSERT_NULL(savedBuf);

    // for this test, set a really long keep alive which prevents
    // ping packets from being generated, which interferes with the
    // test below
    wrap_setKeepAlive(h, 1000);

    // lazy way to get required packet length
    mock_malloc_shouldReturn[0] = pktBuf;
    err = umqtt_Publish(h, "topic", (uint8_t *)"message", sizeof("message"), 1, false, NULL);
    TEST_ASSERT_EQUAL(UMQTT_ERR_NETWORK, err);
    size_t len = mock_NetWrite_in_len;

    // now send the publish packet
    mock_malloc_Reset();
    mock_NetWrite_shouldReturn = len;
    mock_malloc_shouldReturn[0] = pktBuf;
    err = umqtt_Publish(h, "topic", (uint8_t *)"message", sizeof("message"), 1, false, NULL);
    TEST_ASSERT_EQUAL(UMQTT_ERR_OK, err);

    // verify publish packet is hanging around
    // and reset mocks to be ready for next step
    savedBuf = wrap_getNextPktBuf(h);
    TEST_ASSERT_NOT_NULL(savedBuf);
    mock_malloc_Reset();
    mock_free_Reset();
    mock_NetWrite_Reset();

    // this loop should run through all the retries before it gives up
    for (uint32_t ticks = 2500; ticks < 50000; ticks += 5000)
    {
        // run through half the timeout interval, verify nothing happens
        err = umqtt_Run(h, ticks);
        TEST_ASSERT_EQUAL(UMQTT_ERR_OK, err);
        savedBuf = wrap_getNextPktBuf(h);
        TEST_ASSERT_NOT_NULL(savedBuf);
        err = umqtt_GetConnectedStatus(h);
        TEST_ASSERT_EQUAL(UMQTT_ERR_CONNECTED, err);
        TEST_ASSERT_EQUAL(0, mock_malloc_count);
        TEST_ASSERT_FALSE(mock_free_wasCalled);
        TEST_ASSERT_FALSE(mock_NetWrite_wasCalled);

        // now exceed the timeout and verify the packet was resent
        mock_NetWrite_pCopy = (uint8_t*)clientBuf;
        mock_NetWrite_copyLen = 1;
        mock_NetWrite_shouldReturn = len;
        err = umqtt_Run(h, ticks + 2500);
        TEST_ASSERT_EQUAL(UMQTT_ERR_OK, err);
        savedBuf = wrap_getNextPktBuf(h);
        TEST_ASSERT_NOT_NULL(savedBuf);
        err = umqtt_GetConnectedStatus(h);
        TEST_ASSERT_EQUAL(UMQTT_ERR_CONNECTED, err);
        TEST_ASSERT_EQUAL(0, mock_malloc_count);
        TEST_ASSERT_FALSE(mock_free_wasCalled);
        TEST_ASSERT_TRUE(mock_NetWrite_wasCalled);
        TEST_ASSERT_EQUAL(len, mock_NetWrite_in_len);
        TEST_ASSERT_EQUAL(0x32, clientBuf[0]); // check first byte of resent packet
        mock_NetWrite_Reset();
    }

    // next time to exceed the retry timeout should cause it to give up
    err = umqtt_Run(h, 55000);
    TEST_ASSERT_EQUAL(UMQTT_ERR_TIMEOUT, err);
    savedBuf = wrap_getNextPktBuf(h);
    TEST_ASSERT_NULL(savedBuf);
    err = umqtt_GetConnectedStatus(h);
    TEST_ASSERT_EQUAL(UMQTT_ERR_CONNECTED, err);
    TEST_ASSERT_EQUAL(0, mock_malloc_count);
    TEST_ASSERT_TRUE(mock_free_wasCalled);
    TEST_ASSERT_FALSE(mock_NetWrite_wasCalled);
}


TEST_GROUP_RUNNER(Run)
{
    RUN_TEST_CASE(Run, NothingHappens);
    RUN_TEST_CASE(Run, BasicRx);
    RUN_TEST_CASE(Run, RxNetError);
    RUN_TEST_CASE(Run, Ping);
    RUN_TEST_CASE(Run, PingExclusive);
    RUN_TEST_CASE(Run, PingInclusive);
    RUN_TEST_CASE(Run, PingNetError);
    RUN_TEST_CASE(Run, ConnectTimeout);
    RUN_TEST_CASE(Run, Connect);
    RUN_TEST_CASE(Run, PublishTimeout);
    RUN_TEST_CASE(Run, Publish);
    RUN_TEST_CASE(Run, SubscribeTimeout);
    RUN_TEST_CASE(Run, Subscribe);
    RUN_TEST_CASE(Run, UnsubscribeTimeout);
    RUN_TEST_CASE(Run, Unsubscribe);
    RUN_TEST_CASE(Run, PublishExpire);
}
