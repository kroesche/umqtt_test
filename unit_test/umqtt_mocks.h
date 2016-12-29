
#ifndef _UMQTT_MOCKS_H_
#define _UMQTT_MOCKS_H_

// this private structure is used in the library (copied here)
// need to be able to compute its size for test
typedef struct PktBuf
{
    struct PktBuf *next;
    uint16_t packetId;
    uint32_t ticks;
    unsigned int ttl;
} PktBuf_t;

extern void *mock_hNet;
extern umqtt_TransportConfig_t transportConfig;

extern unsigned int mock_malloc_count;
extern void *mock_malloc_shouldReturn[];
extern size_t mock_malloc_in_size;
extern void *mock_malloc(size_t size);
extern void mock_malloc_Reset(void);

extern bool mock_free_wasCalled;
extern void *mock_free_in_ptr;
extern void mock_free(void *ptr);
extern void mock_free_Reset(void);

extern bool mock_NetRead_wasCalled;
extern void *mock_NetRead_in_hNet;
extern uint8_t *mock_NetRead_shouldReturn_pBuf;
extern int mock_NetRead_shouldReturn;
extern int mock_NetRead(void *hNet, uint8_t **ppBuf);
extern void mock_NetRead_Reset(void);

extern bool mock_NetWrite_wasCalled;
extern void *mock_NetWrite_in_hNet;
extern const uint8_t *mock_NetWrite_in_pBuf;
extern uint16_t mock_NetWrite_in_len;
extern bool mock_NetWrite_in_isMore;
extern int mock_NetWrite_shouldReturn;
extern uint8_t *mock_NetWrite_pCopy;
extern uint16_t mock_NetWrite_copyLen;
extern int mock_NetWrite(void *hNet, const uint8_t *pBuf, uint32_t len, bool isMore);
extern void mock_NetWrite_Reset(void);

#endif
