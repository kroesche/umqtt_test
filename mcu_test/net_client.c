/******************************************************************************
 * net_client.c - network abstraction layer for lwip
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

#include <stdint.h>
#include <stdbool.h>
#include <string.h>

#include "lwip/opt.h"
#include "lwip/tcp.h"

#include "net_client.h"

/**
 * This file provides a network abstraction that can be used by the
 * application for accessing the network without knowing any implementation
 * details of the network stack, in this case lwip.  It manages all aspects
 * of the network connection so the app code can just use a simple read/write
 * interface for passing data.
 *
 * Because lwip passes received packets in its own context (possibly the
 * ethernet interrupt), this module implements a simple circular queue to
 * hold incoming packets.  They are placed in the queue in the lwip context
 * and can then be removed in the app context.
 *
 * The applications needs to implement a callback function to be notified
 * of network events.
 */

#ifdef NET_CLIENT_DBGPRINTF
#include "utils/uartstdio.h"
#define DbgPrintf(...) UARTprintf(__VA_ARGS__)
#else
#define DbgPrintf(...) (void)0
#endif

// error handling convenience
#define RETURN_IF_ERR(c,e) do{if(c){return (e);}}while(0)

/**
 * @internal
 * Enqueue a lwip pbuf into a list for later processing.
 *
 * @param this network instance
 * @param pbuf an incoming lwip pbuf
 *
 * @return true if the pbuf was enqueued, false otherwise
 *
 * When a new network packet is received by lwip, the incoming pbuf is
 * stored in a simple circular queue for later retrieval.  This will be
 * called by the ReceiveCb function in the lwip context when a pbuf is
 * received, so it must be protected by a lwip critical section.
 */
static bool  __attribute__ ((noinline))
net_EnqueuePbuf(NetClient_Instance_t *this, struct pbuf *pbuf)
{
    bool ret = false;
    SYS_ARCH_DECL_PROTECT(crit);
    SYS_ARCH_PROTECT(crit); // lwip critical section

    // queue logic:
    // - idx points to the last location written
    // - odx points to the last location that was read
    // - if they are equal the queue is empty

    // ndx points to the next location to store in the queue
    uint32_t ndx = (this->idx + 1) % NETCLIENT_QUEUE_SIZE;

    // if there is room, put the pbuf in the queue
    if (ndx != this->odx)
    {
        this->queue[ndx] = pbuf;
        this->idx = ndx;
        ret = true;
        DbgPrintf("enq[%u]: %u\n", ndx, pbuf->len);
    }
    SYS_ARCH_UNPROTECT(crit);
    return ret;
}

/**
 * @internal
 * Dequeue a lwip pbuf from the where it is stored in the list
 *
 * @param this network instance
 *
 * @return a dequeued pbuf or NULL
 *
 * This function can be called in the application context to retrieve the
 * next lwip pbuf from the list.
 */
static struct pbuf *  __attribute__ ((noinline))
net_DequeuePbuf(NetClient_Instance_t *this)
{
    struct pbuf *pb = NULL;
    // protect the queue structure from lwip interruptions
    // using an lwip critical section
    SYS_ARCH_DECL_PROTECT(crit);
    SYS_ARCH_PROTECT(crit);

    // anything in the queue?
    if (this->idx != this->odx)
    {
        uint32_t ndx = (this->odx + 1) % NETCLIENT_QUEUE_SIZE;
        pb = this->queue[ndx];
        this->odx = ndx;
        DbgPrintf("deq[%u]: %u\n", ndx, pb->len);
    }
    SYS_ARCH_UNPROTECT(crit);
    return pb;
}

/**
 * Get the length of the next packet in the list
 *
 * @param h network instance handle
 *
 * @return length of the next packet that would be returned by net_ReadPacket()
 *
 * This function provides a way for a client to find out the size of the next
 * packet before attempting to read it.
 */
uint16_t
net_GetReadLen(NetClient_Handle_t h)
{
    struct pbuf *pb = NULL;
    uint16_t len = 0;
    // use lwip critical section to protect the list structure from
    // lwip interrupt
    SYS_ARCH_DECL_PROTECT(crit);
    SYS_ARCH_PROTECT(crit);
    NetClient_Instance_t *this = h;
    if (this->idx != this->odx)
    {
        uint32_t ndx = (this->odx + 1) % NETCLIENT_QUEUE_SIZE;
        pb = this->queue[ndx];
        len = pb->len;
    }
    SYS_ARCH_UNPROTECT(crit);
    return len;
}

/**
 * @internal
 * Error handler callback for lwip
 *
 * @param arg instance handle we provided to lwip during init
 * @param err lwip error code
 *
 * This function is required to support lwip.  It will be called when there
 * is a network error.  It will reset internal state (unconnected) and
 * notify the client with event callback.
 */
static void
net_ErrorCb(void *arg, err_t err)
{
    DbgPrintf("net_ErrorCb() %d\n", err);
    NetClient_Instance_t *this = arg;
    tcp_arg(this->hNet, NULL);
    tcp_sent(this->hNet, NULL);
    tcp_recv(this->hNet, NULL);
    tcp_err(this->hNet, NULL);
    tcp_poll(this->hNet, NULL, 0);
    tcp_abort(this->hNet);
    this->hNet = NULL;
    this->isConnected = false;
    this->pfnCb(NET_EVENT_DISCONNECTED, this->pUser);
}

/**
 * @internal
 * Poll handler callback for lwip
 *
 * @param arg instance handle we provided to lwip during init
 * @param pcb lwip protocol control block of the active connection
 *
 * @return lwip error code (always returns ERR_OK)
 *
 * This function is a support callback for lwip.  It is called periodically
 * when there is an active connection.  This function will in turn notify the
 * application with event callback NET_EVENT_POLL.  This can be used to
 * check for errors or do time based processing.  This callback is in the
 * lwip context (which may be interrupt).
 */
static err_t
net_PollCb(void *arg, struct tcp_pcb *pcb)
{
    NetClient_Instance_t *this = arg;
    this->pfnCb(NET_EVENT_POLL, this->pUser);
    return ERR_OK;
}

/**
 * @internal
 * Receive handler callback for lwip
 *
 * @param arg instance handle we provided to lwip
 * @param pcb protocol control block of the active connection
 * @param pb lwip pbuf containing the incoming packet
 * @param err lwip error code
 *
 * @return lwip error code, ERR_MEM if there is a memory error or ERR_OK
 *
 * This function must be implemented to support lwip.  It will be
 * called whenever a packet has been received.  This function queues the
 * packet into the incoming packet list, where it will be retrieved later.
 */
static err_t
net_ReceiveCb(void *arg, struct tcp_pcb *pcb, struct pbuf *pb, err_t err)
{
    // validate input args
    // lwip always set err=ERR_OK so there is no need to check it
    if (arg && pcb /* && (err == ERR_OK) */)
    {
        NetClient_Instance_t *this = arg;

        // check for data available
        if (pb)
        {
            // attempt to queue the incoming pbuf
            if (!net_EnqueuePbuf(this, pb))
            {
                // if enqueue fails, return an error
                // lwip will keep the pbuf and attempt to redeliver
                // and eventually give up.
                // when it give up the pbuf is freed by lwip.
                return ERR_MEM;
            }
        }

        // else the pbuf is NULL which means connection was closed
        else
        {
            // closes it and resets everything, notifies client
            net_ErrorCb(this, ERR_OK);
            return ERR_OK;
        }
    }

    return ERR_OK;
}

// called by lwip when ack has been received for prior data
/**
 * @internal
 * Packet sent handler callback for lwip
 *
 * @param arg instance handle we provided to lwip
 * @param pcb protocol control block for the active connection
 * @param len count of outgoing bytes that were acknowledged
 *
 * @return lwip error code, in this case always ERR_OK
 *
 * This function is called by lwip when outgoing ethernet packet data
 * is acknowledge by the remote socket.  It is a number of bytes that were
 * acknowledged.  This could be used to free data bytes from a circular
 * queue or some similar mechanism.  We are not managing data that way
 * in this module, so this function does nothing.
 */
static err_t
net_SentCb(void *arg, struct tcp_pcb *pcb, uint16_t len)
{
    // s2e uses this to reset timeouts
    return ERR_OK;
}

/**
 * @internal
 * TCP callback when connection is established
 *
 * @param arg mqtt instance data for this connection
 * @param pcb tcp connection instance from stack
 * @param err error indicator
 *
 * This is called by lwip TCP stack when a connection is established.
 *
 * @return ERR_OK if no error, returns to TCP stack
 */
static err_t
net_ConnectedCb(void *arg, struct tcp_pcb *pcb, err_t cberror)
{
    // note: err is always ERR_OK so we dont need to check
    // check args?

    DbgPrintf("net_ConnectedCb()\n");
    NetClient_Instance_t *this = arg;

    // normal connection - no errors
    // now we have a TCP connection - set up all the lwip callbacks
    tcp_err(this->hNet, net_ErrorCb);
    tcp_poll(this->hNet, net_PollCb, 20);
    tcp_recv(this->hNet, net_ReceiveCb);
    tcp_sent(this->hNet, net_SentCb);

    this->isConnected = true;
    this->pfnCb(NET_EVENT_CONNECTED, this->pUser);

    // lwip doesnt handle anything but an "ok" return here anyway
    return ERR_OK;
}

/**
 * Initialize the network module
 *
 * @param pInstMem memory to hold the network instance data
 * @param pfnEventCb callback function for notifications
 * @param pUser optional client data pointer that will be passed back
 * during a notification callback
 *
 * @return a network instance handle
 *
 * This function is used to initialize an instance for this network module.
 * The caller must provide memory for the instance data and pass a pointer
 * to it via the _pInstMem_ parameter.  The size of the memory provided
 * must be NETCLIENT_INSTANCE_SIZE which is defined in net_client.h.  The
 * caller must also implement a callback function which is used for
 * notifications.  The callback function will be passed an event ID and
 * the user data pointer (from the parameter _pUser_).  The callback
 * events can be one of the following:
 *
 * Event | Description
 * ------|------------
 * NET_EVENT_DISCONNECTED | connection was disconnected
 * NET_EVENT_CONNECTED    | connection was established
 * NET_EVENT_POLL         | periodic poll event
 */
NetClient_Handle_t
net_Init(void *pInstMem, void (*pfnEventCb)(NetClient_Event_t, void *),
         void *pUser)
{
    NetClient_Instance_t *this = pInstMem;
    if (this)
    {
        this->idx = 0;
        this->odx = 0;
        this->hNet = NULL;
        this->isConnected = false;
        this->pUser = pUser;
        this->pfnCb = pfnEventCb;
    }
    return this;
}

/**
 * Establish a TCP connection to the specified address and port
 *
 * @param h network instance handle that was returned by net_Init()
 * @param addr IP address in a byte array
 * @param port the TCP port to connect
 *
 * @return returns 0 if successful, negative number if there is an error
 *
 * This function initiates a TCP connection to a remote host.  Note that
 * when this function returns, the connection is not yet established but
 * is in progress.  The client can use the callback event NET_EVENT_CONNECTED
 * to know when the connection is established.
 */
int
net_Connect(NetClient_Handle_t h, uint8_t addr[], uint16_t port)
{
    RETURN_IF_ERR(h == NULL, -1);
    NetClient_Instance_t *this = h;

    // build the IP address
    struct ip_addr ipaddr;
    IP4_ADDR(&ipaddr, addr[0], addr[1], addr[2], addr[3]);
    DbgPrintf("net_Connect() to %u.%u.%u.%u:%u\n", addr[0], addr[1], addr[2], addr[3], port);

    // allocate a PCB for the connection
    void *pcb = tcp_new();
    RETURN_IF_ERR(pcb == NULL, -2);

    // set the error callback
    tcp_err(pcb, net_ErrorCb);

    // set the client arg
    this->hNet = pcb;
    tcp_arg(pcb, this);

    DbgPrintf("net_Connect() initiating\n");
    // initiate a connection.  the callback will be called when complete
    err_t err = tcp_connect(pcb, &ipaddr, port, net_ConnectedCb);
    if (err != ERR_OK)
    {
        DbgPrintf("net_Connect() failed\n");
        this->isConnected = false;
        this->hNet = NULL;
        tcp_close(pcb);
        return -4;
    }

    return 0;
}

/**
 * Disconnect the active connection
 *
 * @param h network instance handle
 *
 * This function will terminate the active connection.
 */
void
net_Disconnect(NetClient_Handle_t h)
{
    if (h)
    {
        NetClient_Instance_t *this = h;

        tcp_arg(this->hNet, NULL);
        tcp_sent(this->hNet, NULL);
        tcp_recv(this->hNet, NULL);
        tcp_err(this->hNet, NULL);
        tcp_poll(this->hNet, NULL, 0);
        tcp_close(this->hNet);
        this->hNet = NULL;
        this->isConnected = false;
        this->pfnCb(NET_EVENT_DISCONNECTED, this->pUser);
    }
}

/**
 * Read a packet from the network connection
 *
 * @param h network instance handle (from net_Init())
 * @param pBuf pointer to location to store incoming packet data
 * @param len size of buffer pointed at by pBuf
 *
 * @return count of bytes that were copied to pBuf or 0 if no packet
 * data is available
 *
 * This function reads a packet of data received from the network, and stores
 * the packet data at the location pointed at by _pBuf_.  It will return
 * the number of bytes that were copied.
 */
int
net_ReadPacket(NetClient_Handle_t h, uint8_t *pBuf, uint16_t len)
{
    RETURN_IF_ERR((h == NULL) || (pBuf == NULL), -1);
    NetClient_Instance_t *this = h;

    struct pbuf *pb = net_DequeuePbuf(this);
    if (pb)
    {
        uint8_t *payload = (uint8_t *)pb->payload;
        uint16_t pktlen = pb->len;
        len = pktlen > len ? len : pktlen;
        memcpy(pBuf, payload, len);
        tcp_recved(this->hNet, pktlen);
        pbuf_free(pb);
    }
    else
    {
        len = 0;
    }

    return len;
}

/**
 * Write a packet of data to the network connection
 *
 * @param h network instance handle, from net_Init()
 * @param pBuf pointer to buffer holding the data to send
 * @param len count of bytes to send
 * @param isMore true if there is more data to be written
 *
 * @return the number of bytes that were sent
 *
 * This function will attempt to write the bytes from _pBuf_ to the active
 * network connection.  The flag _isMore_ can be used to indicate that
 * this data buffer is part of a bigger data set that can be aggregated before
 * transmitting.  This can be used as a hint to the network stack and may
 * allow it to be more efficient and sending data, but is not required.
 */
int
net_WritePacket(NetClient_Handle_t h, const uint8_t *pBuf, uint32_t len, bool isMore)
{
    RETURN_IF_ERR((h == NULL) || (pBuf == NULL), -1);
    NetClient_Instance_t *this = h;

    err_t err = tcp_write(this->hNet, pBuf, len, TCP_WRITE_FLAG_COPY);
    RETURN_IF_ERR(err != ERR_OK, 0);
    if (!isMore)
    {
        // tcp_output could return error as well, assume its okay
        tcp_output(this->hNet);
    }
    return len;
}

/**
 * Determine the current connection state.
 *
 * @param h the network instance handle
 *
 * @return true if there is an active connection
 */
bool
net_IsConnected(NetClient_Handle_t h)
{
    bool ret = false;
    if (h)
    {
        NetClient_Instance_t *this = h;
        ret = this->isConnected;
    }
    return ret;
}

