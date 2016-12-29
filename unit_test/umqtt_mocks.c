
#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>

#include "umqtt/umqtt.h"
#include "umqtt_mocks.h"

void *mock_hNet;

umqtt_TransportConfig_t transportConfig =
{
    NULL,
    mock_malloc,
    mock_free,
    mock_NetRead,
    mock_NetWrite
};

void *mock_malloc_shouldReturn[3];
unsigned int mock_malloc_count;
size_t mock_malloc_in_size;
void *
mock_malloc(size_t size)
{
    mock_malloc_in_size = size;
    void *ret = mock_malloc_shouldReturn[mock_malloc_count];
    ++mock_malloc_count;
    return ret;
}
void
mock_malloc_Reset(void)
{
    mock_malloc_count = 0;
    mock_malloc_shouldReturn[0] = NULL;
    mock_malloc_shouldReturn[1] = NULL;
    mock_malloc_shouldReturn[2] = NULL;
    mock_malloc_in_size = 0;
}

bool mock_free_wasCalled;
void *mock_free_in_ptr;
void
mock_free(void *ptr)
{
    mock_free_wasCalled = true;
    mock_free_in_ptr = ptr;
}
void
mock_free_Reset(void)
{
    mock_free_wasCalled = false;
    mock_free_in_ptr = NULL;
}

bool mock_NetRead_wasCalled;
void *mock_NetRead_in_hNet;
uint8_t *mock_NetRead_shouldReturn_pBuf;
int mock_NetRead_shouldReturn;
int
mock_NetRead(void *hNet, uint8_t **ppBuf)
{
    mock_NetRead_wasCalled = true;
    mock_NetRead_in_hNet = hNet;
    *ppBuf = mock_NetRead_shouldReturn_pBuf;
    return mock_NetRead_shouldReturn;
}
void
mock_NetRead_Reset(void)
{
    mock_NetRead_wasCalled = false;
    mock_NetRead_in_hNet = NULL;
    mock_NetRead_shouldReturn_pBuf = NULL;
    mock_NetRead_shouldReturn = 0;
}

bool mock_NetWrite_wasCalled;
void *mock_NetWrite_in_hNet;
const uint8_t *mock_NetWrite_in_pBuf;
uint16_t mock_NetWrite_in_len;
bool mock_NetWrite_in_isMore;
int mock_NetWrite_shouldReturn;
uint8_t *mock_NetWrite_pCopy;
uint16_t mock_NetWrite_copyLen;
int
mock_NetWrite(void *hNet, const uint8_t *pBuf, uint32_t len, bool isMore)
{
    mock_NetWrite_wasCalled = true;
    mock_NetWrite_in_hNet = hNet;
    mock_NetWrite_in_pBuf = pBuf;
    mock_NetWrite_in_len = len;
    mock_NetWrite_in_isMore = isMore;
    if (mock_NetWrite_pCopy)
    {
        memcpy(mock_NetWrite_pCopy, pBuf, mock_NetWrite_copyLen);
    }
    return mock_NetWrite_shouldReturn;
}
void
mock_NetWrite_Reset(void)
{
    mock_NetWrite_wasCalled = false;
    mock_NetWrite_in_hNet = NULL;
    mock_NetWrite_in_pBuf = NULL;
    mock_NetWrite_in_len = 0;
    mock_NetWrite_in_isMore = false;
    mock_NetWrite_shouldReturn = 0;
    mock_NetWrite_pCopy = NULL;
    mock_NetWrite_copyLen = 0;
}

