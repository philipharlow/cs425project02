
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "forward.h"
#include "ethernet.h"
#include "arp.h"
#include "sr_if.h"
#include "sr_router.h"
#include "sr_protocol.h"


 struct arp_cache_entry arpCacheEntries[ARP_CACHE_SIZE];
 
/*--------------------------------------------------------------------- 
 * Method: void handleArp(struct sr_instance*, uint8_t*,
 *                          unsigned int, char* )
 *
 * This will handle what to do with the incoming ARP packet
 *---------------------------------------------------------------------*/
void handleArp(
        struct sr_instance* sr,
        uint8_t* packet,
        unsigned int len,
        char* interface)
{
    struct in_addr requested, replied;
    struct sr_if * ifPtr = sr->if_list;
    struct sr_arphdr * arpHeader = (struct sr_arphdr*)(packet+14);
    int i;

    // if we have an ARP request, check our list of interfaces to see if we
    // have the hardware address of the request ipaddress and send a reply if we do 
    if (ntohs(arpHeader->ar_op) == ARP_REQUEST) {
        requested.s_addr = arpHeader->ar_tip;
        fprintf(stdout, "-> ARP Request: checking who has %s?\n", inet_ntoa(requested));
        while (ifPtr) {
            if (ifPtr->ip == arpHeader->ar_tip) {
                arpSendReply(sr, packet, len, interface, ifPtr);
                return;
            } else {
                ifPtr = ifPtr->next;
            }
        }

        if (!ifPtr) {
            printf("-- ARP Request: do not have %s\n", inet_ntoa(requested));
        }
    }
    // if packet is an arp reply, cache it
    if (ntohs(arpHeader->ar_op) == ARP_REPLY) {
        replied.s_addr = arpHeader->ar_sip;

        // print out the information
        printf("-> ARP Reply: %s is at ", inet_ntoa(replied));
        for (i = 0; i < ETHER_ADDR_LEN; i++)
            printf("%2.2x", arpHeader->ar_sha[i]);
        printf("\n");

        // cache the new arp entry
        arpCacheEntry(arpHeader);

        // check cache to find matching packets
        for (i = 0; i < ARP_CACHE_SIZE; i++) {
            if (arpCacheEntries[i].valid == 1)
                checkCachedPackets(sr, i);
        }
    }
}

/*---------------------------------------------------------------------
 * Method: void arpSendReply(struct sr_if*, struct sr_arphdr*,
 *                              struct sr_ethernet_hdr* ethernetHdr)
 *
 * Reply to ARP requests
 * -------------------------------------------------------------------*/
void arpSendReply(
        struct sr_instance* sr,
        uint8_t* packet,
        unsigned int len,
        char* interface,
        struct sr_if * ifPtr)
{
    struct sr_ethernet_hdr * ethernetHdr = (struct sr_ethernet_hdr*)packet;
    struct sr_arphdr * arpHeader = (struct sr_arphdr*)(packet+14);
    struct in_addr replied;
    int i;

    makearp(arpHeader, arpHeader->ar_hrd, arpHeader->ar_pro, arpHeader->ar_hln, arpHeader->ar_pln, htons(ARP_REPLY),
            sr_get_interface(sr, interface)->addr, sr_get_interface(sr, interface)->ip,
            arpHeader->ar_sha, arpHeader->ar_sip);
    makeethernet(ethernetHdr, ETHERTYPE_ARP, ifPtr->addr, ethernetHdr->ether_shost);

    // Send the ARP reply
    sr_send_packet(sr, packet, len, interface);

    // Print out the reply
    replied.s_addr = arpHeader->ar_sip;
    fprintf(stdout, "<- ARP Reply: %s is at ", inet_ntoa(replied));
    for (i = 0; i < ETHER_ADDR_LEN; i++) {
        printf("%2.2x", arpHeader->ar_sha[i]);
    } printf ("\n");
}

/*-----------------------------------------------------------------------------
 * Method: void arpSendRequest(struct sr_instance* sr, struct sr_if* iface, uint32_t tip)
 *
 * broadcasts any arp request looking for the hardware address that matches
 *---------------------------------------------------------------------------*/
void arpSendRequest(struct sr_instance* sr, struct sr_if* iface, uint32_t tip)
{

    struct in_addr requested;               
    uint8_t broadcast[ETHER_ADDR_LEN];      
    int i;

    // allocate memory for new packet
    uint8_t* requestPacket = malloc(42 * sizeof(uint8_t));
    if (requestPacket == NULL) {
        fprintf(stderr, "Error: malloc failed on new packet\n");
        return;
    }
    memset(requestPacket, 0, 42 * sizeof(uint8_t));

    // organize the packet
    struct sr_ethernet_hdr* ethernetHdr = (struct sr_ethernet_hdr*)requestPacket;
    struct sr_arphdr* arpHeader = (struct sr_arphdr*)(requestPacket+14);

    // fill in the broadcast array
    for (i = 0; i < ETHER_ADDR_LEN; i++)
        broadcast[i] = 0xff;

    // make ARP request with new packet
    makearp(arpHeader, htons(ARPHDR_ETHER), htons(ETHERTYPE_IP), 6, 4, htons(ARP_REQUEST),
            iface->addr, iface->ip,
            broadcast, tip);
    makeethernet(ethernetHdr, ETHERTYPE_ARP, iface->addr, broadcast);

    // send packet
    sr_send_packet(sr, requestPacket, 42, iface->name);

    requested.s_addr = tip;
    printf("<- ARP Request: who has %s?\n", inet_ntoa(requested));

    free(requestPacket);
}

/*-----------------------------------------------------------------------------
 * Method: void makearp(
 *      struct sr_arphdr* arpHeader,
 *      uint16_t    arp_hrd,
 *      uint16_t    arp_pro,
 *      uint8_t     arp_hln,
 *      uint8_t     arp_pln,
 *      uint16_t    arp_op,
 *      uint8_t*    arp_sha,
 *      uint32_t    arp_sip,
 *      uint8_t*    arp_tha,
 *      uint32_t    arp_tip )
 *
 * modifies an arp packet with the parameters passed
 * assuming everything is in network byte order
 *---------------------------------------------------------------------------*/
void makearp(
        struct sr_arphdr* arpHeader,
        uint16_t    arp_hrd,
        uint16_t    arp_pro,
        uint8_t     arp_hln,
        uint8_t     arp_pln,
        uint16_t    arp_op,
        uint8_t*    arp_sha,
        uint32_t    arp_sip,
        uint8_t*    arp_tha,
        uint32_t    arp_tip )
{
    uint32_t sbuf, tbuf;
    uint8_t shabuf[ETHER_ADDR_LEN], thabuf[ETHER_ADDR_LEN];
    int i;

    arpHeader->ar_op = arp_op;
    arpHeader->ar_pro = arp_pro;
    arpHeader->ar_hrd = arp_hrd;
    arpHeader->ar_pln = arp_pln;
    arpHeader->ar_hln = arp_hln;

    // read into buffer
    sbuf = arp_sip;
    tbuf = arp_tip;
    arpHeader->ar_sip = sbuf; 
    arpHeader->ar_tip = tbuf;

    // read into buffers
    for (i = 0; i < ETHER_ADDR_LEN; i++) {
        shabuf[i] = arp_sha[i];
        thabuf[i] = arp_tha[i];
    }

    for (i = 0; i < ETHER_ADDR_LEN; i++) {
        arpHeader->ar_sha[i] = shabuf[i];
        arpHeader->ar_tha[i] = thabuf[i];
    }
}

/*-----------------------------------------------------------------------------
 * Method: void arpCacheEntry(struct sr_arphdr* arpHeader)
 *
 * store ARP info in network byte order in the cache
 *---------------------------------------------------------------------------*/
void arpCacheEntry(struct sr_arphdr* arpHeader)
{
    int i,j;

    // find first empty slot in the cache
    for (i = 0; i < ARP_CACHE_SIZE; i++) {
        if (arpCacheEntries[i].valid == 0) 
            break;
    }

    // extract ARP info from arpHeader and cache it
    arpCacheEntries[i].ar_sip = arpHeader->ar_sip;
    for (j = 0; j < ETHER_ADDR_LEN; j++)
        arpCacheEntries[i].ar_sha[j] = arpHeader->ar_sha[j];

    // timestamp and make valid
    arpCacheEntries[i].timeCached = time(NULL);
    arpCacheEntries[i].valid = 1;
}

/*---------------------------------------------------------------------
 * Method: void arpInitCache(struct arp_translation_table arpCacheEntries)
 *
 * zero's all valid fields in our ARP cache
 *--------------------------------------------------------------------*/
void arpInitCache()
{
    int i;
    for (i = 0; i < ARP_CACHE_SIZE; i++) {
        arpCacheEntries[i].valid = 0;
    }
}

/*-----------------------------------------------------------------------------
 * Method: int arpSearchCache(struct ip* ipHdr)
 *
 * searches our ARP cache to see if we have a valid hardware address
 * that matches the target ipaddress
 *---------------------------------------------------------------------------*/
int arpSearchCache(uint32_t ipaddr)
{
    int i;

    for (i = 0; i < ARP_CACHE_SIZE; i++) {
        if (arpCacheEntries[i].valid == 1) {
            if (arpCacheEntries[i].ar_sip == ipaddr) {
                return i;
            }
        }
    }
    return -1;
}

/*-----------------------------------------------------------------------------
 * Method: void arpUpdateCache()
 *
 * finds stale ARP entries in our cache and invalidates them
 *---------------------------------------------------------------------------*/
void arpUpdateCache()
{
    // find entries older than STALE_TIME seconds and set valid bit to 0
    int i;

    for (i = 0; i < ARP_CACHE_SIZE; i++) {
        // if valid and timestamp is older than 15 seconds, mark invalid
        if (arpCacheEntries[i].valid == 1) {
            if (difftime(time(NULL), arpCacheEntries[i].timeCached) > ARP_STALE_TIME) {
                printf("-- ARP: Marking ARP cache entry %d invalid\n", i);
                arpCacheEntries[i].valid = 0;
            }
        }
    }
}

/*-----------------------------------------------------------------------------
 * Method: uint8_t* arpReturnEntryMac(int entry)
 *
 * returns a pointer to the arpCacheEntries[entry] source hardware address
 *---------------------------------------------------------------------------*/
uint8_t* arpReturnEntryMac(int entry)
{
     return (uint8_t*)&arpCacheEntries[entry].ar_sha;
}

