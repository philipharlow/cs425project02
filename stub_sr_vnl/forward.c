
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <time.h>

#include "sr_if.h"
#include "sr_rt.h"
#include "sr_router.h"
#include "sr_protocol.h"
#include "ethernet.h"
#include "arp.h"
#include "icmp.h"
#include "ip.h"
#include "forward.h"

// length of zero signifies empty spot in cache
struct packet_cache_entry packetCacheEntries[PACKET_CACHE_SIZE];

/*-----------------------------------------------------------------------------
 * Method: void handleForward
 *
 * decide what interface to send packet out
 *---------------------------------------------------------------------------*/
void handleForward(
        struct sr_instance* sr,
        uint8_t* packet,
        unsigned int len,
        char* interface )
{
    struct ip* ipHeader = (struct ip*)(packet+14);
    struct sr_rt* rtptr = sr->routing_table;
    int cachedEntry;

    // loop through rtable for the next hop with the packet's destination ip
    while (rtptr) {
        if (rtptr->dest.s_addr == ipHeader->ip_dst.s_addr)
            break;
        else
            rtptr = rtptr->next;
    }

    // if not in our rtable, send it out eth0
    if (!rtptr) {
        rtptr = sr->routing_table;          // eth0
        if ((cachedEntry = arpSearchCache(rtptr->gw.s_addr)) > -1) {
            forwardPacket(sr, packet, len, rtptr->interface, arpReturnEntryMac(cachedEntry));
        } else {
            cachePacket(sr, packet, len, rtptr);
        }
    }
    
    /* Check ARP cache for matching MAC, forward the packet. 
     * otherwise, cache the packet and wait for an ARP reply
     * to tell us the correct MAC address to use */
    if ((cachedEntry = arpSearchCache(rtptr->gw.s_addr)) > -1) {
        forwardPacket(sr, packet, len, rtptr->interface, arpReturnEntryMac(cachedEntry));       
    } else {
        cachePacket(sr, packet, len, rtptr);
    }
}

/*-----------------------------------------------------------------------------
 * Method: void forwardPacket
 *
 * builds the headers to forward the TCP/UDP
 *---------------------------------------------------------------------------*/
void forwardPacket(
        struct sr_instance* sr,
        uint8_t* packet,
        unsigned int len,
        char* interface,
        uint8_t* desthwaddr )
{
    struct sr_ethernet_hdr* ethernetHeader = (struct sr_ethernet_hdr*)packet;
    struct ip* ipHeader = (struct ip*)(packet+14);
    struct in_addr forwarded;
    int i;
    
    makeethernet(ethernetHeader, ntohs(ethernetHeader->ether_type),
            sr_get_interface(sr, interface)->addr, desthwaddr);

    sr_send_packet(sr, packet, len, interface);

    // log on send
    forwarded.s_addr = ipHeader->ip_dst.s_addr;
    printf("<- Forwarded packet with ip_dst %s to ", inet_ntoa(forwarded));
    for (i = 0; i < ETHER_ADDR_LEN; i++)
        printf("%2.2x", ethernetHeader->ether_dhost[i]);
    printf("\n");
}

/*-----------------------------------------------------------------------------
 * Method: void cachePacket(struct sr_instance* sr, uint8_t* packet,
 *                              unsigned int len, struct sr_rt* rtptr)
 *
 * sends a request for the hardware address of the ipaddress we have, stores our packet
 * until we have an arp entry that matches the ip, then sends the packet
 *---------------------------------------------------------------------------*/
void cachePacket(
        struct sr_instance* sr,
        uint8_t* packet,
        unsigned int len,
        struct sr_rt* rtptr)
{
    struct ip* ipHeader = (struct ip*)(packet+14);
    int i;

    // request ARP for the unknown packet
    arpSendRequest(sr, sr_get_interface(sr, rtptr->interface), rtptr->gw.s_addr);

    // look through packet cache for the first empty entry
    for (i = 0; i < PACKET_CACHE_SIZE; i++) {
        if (packetCacheEntries[i].len == 0) 
            break;
    }

    // copy packet data to cache
    memcpy(&packetCacheEntries[i].packet, packet, len);
    packetCacheEntries[i].nexthop = rtptr;
    packetCacheEntries[i].tip = ipHeader->ip_dst.s_addr;
    packetCacheEntries[i].len = len;
    packetCacheEntries[i].arps = 1;
    packetCacheEntries[i].timeCached = time(NULL);

}

/*-----------------------------------------------------------------------------
 * Method: void checkCachedPackets(struct sr_instance* sr, int cachedArp)
 *
 * searches our packet cache for a matching ip in arpCacheEntries[cachedArp].
 * if a match found, we forward the packet. if we do not find a match we need
 * an ARP cache entry for it.
 *---------------------------------------------------------------------------*/
void checkCachedPackets(struct sr_instance* sr, int cachedArp)
{
    int i, arpMatch;
    for (i = 0; i < PACKET_CACHE_SIZE; i++) {
        if (packetCacheEntries[i].len > 0) {
            // if we have a packet waiting
            if (packetCacheEntries[i].arps <= 5) {
                // not sent 5 ARP's for this packet
                if ((arpMatch = arpSearchCache(packetCacheEntries[i].tip)) > -1) {
                    // ARP match for our packet's next hop
                    forwardPacket(sr, (uint8_t*)&packetCacheEntries[i].packet, packetCacheEntries[i].len,
                            // send it along
                            packetCacheEntries[i].nexthop->interface, arpReturnEntryMac(arpMatch));
                    packetCacheEntries[i].len = 0;
                } else {
                    // wait three seconds between each ARP request
                    if ((int)(difftime(time(NULL), packetCacheEntries[i].timeCached))%3 < 1) {
                        arpSendRequest(sr, sr_get_interface(sr, packetCacheEntries[i].nexthop->interface),
                                packetCacheEntries[i].nexthop->gw.s_addr);
                        packetCacheEntries[i].arps++;
                    }
                }
            } else {
                icmpSendUnreachable(sr, (uint8_t*)&packetCacheEntries[i].packet, packetCacheEntries[i].len,
                        packetCacheEntries[i].nexthop->interface, ICMP_HOST_UNREACHABLE);
                packetCacheEntries[i].len = 0;
            }
        }
    }
}

/*-----------------------------------------------------------------------------
 * Method void initPacketCache()
 *
 * zero all fields
 *---------------------------------------------------------------------------*/
void initPacketCache()
{
    int i;
    for (i = 0; i < PACKET_CACHE_SIZE; i++)
        packetCacheEntries[i].len = 0;
}