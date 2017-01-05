/******************************************************************************
 * net_client.h - network abstraction layer for lwip
 *
 * Copyright (c) 2017, Joseph Kroesche (tronics.kroesche.io)
 * All rights reserved.
 *
 * This software is released under the FreeBSD license, found in the
 * accompanying file LICENSE.txt and at the following URL:
 *      http://www.freebsd.org/copyright/freebsd-license.html
 *
 * This software is provided as-is and without warranty.
 */

#ifndef __NET_CLIENT_H__
#define __NET_CLIENT_H__

// number of incoming packets that can be queued
#define NETCLIENT_QUEUE_SIZE 8

/**
 * Event codes returned through the callback function.
 */
typedef enum
{
    NET_EVENT_NONE,
    NET_EVENT_DISCONNECTED, ///< connection was disconnected
    NET_EVENT_CONNECTED,    ///< connection was connected
    NET_EVENT_POLL,         ///< periodic poll event
} NetClient_Event_t;

/**
 * @internal
 * Instance structure - treat as opaque.
 */
typedef struct
{
    uint8_t idx;
    uint8_t odx;
    void *queue[NETCLIENT_QUEUE_SIZE];
    void *hNet;
    void *pUser;
    void (*pfnCb)(NetClient_Event_t, void *);
    bool isConnected;
} NetClient_Instance_t;

/**
 * The number of bytes needed for net_client instance.
 */
#define NETCLIENT_INSTANCE_SIZE sizeof(NetClient_Instance_t)

/**
 * Handle type to use for accessing net_client functions.
 */
typedef void * NetClient_Handle_t;

#ifdef __cplusplus
extern "C" {
#endif

extern NetClient_Handle_t net_Init(void *pInstMem,
                                   void (*pfnEventCb)(NetClient_Event_t, void *),
                                   void *pUser);
extern int net_Connect(NetClient_Handle_t h, uint8_t addr[], uint16_t port);
extern void net_Disconnect(NetClient_Handle_t h);
extern int net_ReadPacket(NetClient_Handle_t h, uint8_t *pBuf, uint16_t len);
extern int net_WritePacket(NetClient_Handle_t h, const uint8_t *pBuf, uint32_t len, bool isMore);
extern bool net_IsConnected(NetClient_Handle_t h);
extern uint16_t net_GetReadLen(NetClient_Handle_t h);

#ifdef __cplusplus
}
#endif

#endif
