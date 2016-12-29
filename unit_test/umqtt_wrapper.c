
#include "umqtt/umqtt.c"

uint8_t *
wrap_newPacket(umqtt_Handle_t h, size_t len)
{
    umqtt_Instance_t *this = (umqtt_Instance_t *)h;
    return newPacket(this, len);
}

void
wrap_deletePacket(umqtt_Handle_t h, uint8_t *pbuf)
{
    umqtt_Instance_t *this = (umqtt_Instance_t *)h;
    deletePacket(this, pbuf);
}

void
wrap_enqueuePacket(umqtt_Handle_t h, uint8_t *pbuf, uint16_t packetId, uint32_t ticks)
{
    umqtt_Instance_t *this = (umqtt_Instance_t *)h;
    enqueuePacket(this, pbuf, packetId, ticks);
}

uint8_t *
wrap_dequeuePacketById(umqtt_Handle_t h, uint16_t packetId)
{
    umqtt_Instance_t *this = (umqtt_Instance_t *)h;
    return dequeuePacketById(this, packetId);
}

uint8_t *
wrap_dequeuePacketByType(umqtt_Handle_t h, uint8_t type)
{
    umqtt_Instance_t *this = (umqtt_Instance_t *)h;
    return dequeuePacketByType(this, type);
}

PktBuf_t *
wrap_getNextPktBuf(umqtt_Handle_t h)
{
    umqtt_Instance_t *this = (umqtt_Instance_t *)h;
    return this->pktList.next;
}

void
wrap_setConnected(umqtt_Handle_t h, bool connected)
{
    umqtt_Instance_t *this = (umqtt_Instance_t *)h;
    this->isConnected = connected;
}

void
wrap_setKeepAlive(umqtt_Handle_t h, uint16_t keepAlive)
{
    umqtt_Instance_t *this = (umqtt_Instance_t *)h;
    this->keepAlive = keepAlive;
}
