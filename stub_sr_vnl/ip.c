

#include <stdio.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include "sr_if.h"
#include "sr_router.h"
#include "sr_protocol.h"
#include "ethernet.h"
#include "ip.h"
#include "icmp.h"
#include "checksum.h"

/*--------------------------------------------------------------------- 
 * Method: void handleIp(struct sr_instance*, uint8_t*,
 *                          unsigned int, char* )
 *
 * decides what to do with incoming IP packet
 *---------------------------------------------------------------------*/
 void handleIp(
         struct sr_instance* sr, 
         uint8_t* packet,
         unsigned int len,
         char* interface)
{
    struct ip* ipHdr = (struct ip*)(packet+14);

    if (ipHdr->ip_p == IPPROTO_ICMP) {
        fprintf(stdout, "-> IP Protocol: %2.2x -> ICMP\n", ipHdr->ip_p);
        handleIcmp(sr, packet, len, interface);
    }

    if (ipHdr->ip_p == IPPROTO_TCP) {
        fprintf(stdout, "-> IP Protocol: %2.2x -> TCP\n", ipHdr->ip_p);
        icmpSendUnreachable(sr, packet, len, interface, ICMP_PORT_UNREACHABLE);
    }

    if (ipHdr->ip_p == IPPROTO_UDP) {
        fprintf(stdout, "-> IP Protocol: %2.2x -> UDP\n", ipHdr->ip_p);
        icmpSendUnreachable(sr, packet, len, interface, ICMP_PORT_UNREACHABLE);
    }
}

/*-----------------------------------------------------------------------------
 * Method: void makeip(struct ip* ipHdr, unsigned int len, unsigned char ttl,
 *                      unsigned char proto, in_addr* src, in_addr* dst)
 *
 * Create IP header
 *---------------------------------------------------------------------------*/
void makeip(
        struct ip* ipHdr,
        unsigned int len,
        uint16_t off,
        unsigned char ttl,
        unsigned char proto,
        uint32_t src,
        uint32_t dst )
{
    uint32_t sbuf, dbuf;
    uint8_t* p = (uint8_t*) ipHdr;
    *p = 0x45;                      
    ipHdr->ip_tos = 0;
    ipHdr->ip_len = htons(len);
    ipHdr->ip_id = 0;               
    ipHdr->ip_off = htons(off);
    ipHdr->ip_ttl = ttl;
    ipHdr->ip_p = proto;

    // read into buffers in case we are overwriting
    sbuf = src;
    dbuf = dst;
    ipHdr->ip_src.s_addr = sbuf;
    ipHdr->ip_dst.s_addr = dbuf;

    ipHdr->ip_sum = 0x0000;
    ipHdr->ip_sum = in_checksum((uint16_t*)ipHdr, 20);
}

