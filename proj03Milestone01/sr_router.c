/**********************************************************************
 * file:  sr_router.c 
 * date:  Mon Feb 18 12:50:42 PST 2002  
 * Contact: casado@stanford.edu 
 *
 * Description:
 * 
 * This file contains all the functions that interact directly
 * with the routing table, as well as the main entry method
 * for routing. 11
 * 90904102
 **********************************************************************/

#include <stdio.h>
#include <assert.h>
#include <stdint.h>
#include <unistd.h>
#include <time.h>


#include "sr_if.h"
#include "sr_rt.h"
#include "sr_router.h"
#include "sr_protocol.h"

void handleArp(struct sr_instance*, uint8_t*, unsigned int, char* );
void sendReply(struct sr_instance*, uint8_t*, unsigned int, char*, struct sr_if* );
void makeArp(
        struct sr_arphdr* arpHdr,
        uint16_t    arp_hrd,
        uint16_t    arp_pro,
        uint8_t     arp_hln,
        uint8_t     arp_pln,
        uint16_t    arp_op,
        uint8_t*    arp_sha,
        uint32_t    arp_sip,
        uint8_t*    arp_tha,
        uint32_t    arp_tip );
void makeip(
        struct ip* ipHdr,
        unsigned int len,
        uint16_t off,
        unsigned char ttl,
        unsigned char proto,
        uint32_t src,
        uint32_t dst );
void arpEntry(struct sr_arphdr* );
void makeEthernet(
        struct sr_ethernet_hdr* ethernetHdr,
        uint16_t type,
        uint8_t* src,
        uint8_t* dst );

#define ARP_CACHE_SIZE 100
#define ARP_STALE_TIME 15   // in seconds



struct arp_cache_entry {
    uint32_t            ar_sip;                     // sender ip addr
    uint8_t             ar_sha[ETHER_ADDR_LEN];     // sender hardware addr
    time_t              timeCached;                 // timestamp
    int                 valid;                      // timeout
};

struct arp_cache_entry arpCacheEntries[ARP_CACHE_SIZE];



/*--------------------------------------------------------------------- 
 * Method: sr_init(void)
 * Scope:  Global
 *
 * Initialize the routing subsystem
 * 
 *---------------------------------------------------------------------*/

void sr_init(struct sr_instance* sr) 
{
    /* REQUIRES */
    assert(sr);

    /* Add initialization code here! */

} /* -- sr_init -- */



/*---------------------------------------------------------------------
 * Method: sr_handlepacket(uint8_t* p,char* interface)
 * Scope:  Global
 *
 * This method is called each time the router receives a packet on the
 * interface.  The packet buffer, the packet length and the receiving
 * interface are passed in as parameters. The packet is complete with
 * ethernet headers.
 *
 * Note: Both the packet buffer and the character's memory are handled
 * by sr_vns_comm.c that means do NOT delete either.  Make a copy of the
 * packet instead if you intend to keep it around beyond the scope of
 * the method call.
 *
 *---------------------------------------------------------------------*/

void sr_handlepacket(struct sr_instance* sr, 
        uint8_t * packet/* lent */,
        unsigned int len,
        char* interface/* lent */)
{
    /* REQUIRES */
    assert(sr);
    assert(packet);
    assert(interface);

    printf("*** -> Received packet of length %d \n",len);

    struct sr_ethernet_hdr * ethernetHdr = (struct sr_ethernet_hdr *)packet;

    if (ntohs(ethernetHdr->ether_type) == ETHERTYPE_IP) {
        //to do for future milestone
    }
    //if our packet is an ARP
    if (ntohs(ethernetHdr->ether_type) == ETHERTYPE_ARP) {
        handleArp(sr, packet, len, interface);
    }

}/* end sr_ForwardPacket */


/*--------------------------------------------------------------------- 
 * Method:
 *
 *---------------------------------------------------------------------*/


/*--------------------------------------------------------------------- 
 * Method: void handleArp(struct sr_instance*, uint8_t*,
 *                          unsigned int, char* )
 *
 * This will handle what to do with the incoming ARP packet
 *---------------------------------------------------------------------*/
void handleArp( struct sr_instance* sr, uint8_t* packet, unsigned int len, char* interface) {

    struct in_addr requested, replied;
    struct sr_if * interfacePtr = sr->if_list;
    struct sr_arphdr * arpHeader = (struct sr_arphdr*)(packet+14);
    int i;

    // if we have an ARP request, check our list of interfaces to see if we
    // have the hardware address of the request ipaddress and send a reply if we do 
    if (ntohs(arpHeader->ar_op) == ARP_REQUEST) {
        requested.s_addr = arpHeader->ar_tip;
        fprintf(stdout, "-> ARP Request: checking who has %s?\n", inet_ntoa(requested));
        while (interfacePtr) {
            if (interfacePtr->ip == arpHeader->ar_tip) {
                sendReply(sr, packet, len, interface, interfacePtr);
                return;
            } else {
                interfacePtr = interfacePtr->next;
            }
        }

        if (!interfacePtr) {
            printf("-- ARP Request: does not have %s\n", inet_ntoa(requested));
        }
    }
    // if packet is an arp reply, cache it
    if (ntohs(arpHeader->ar_op) == ARP_REPLY) {
        replied.s_addr = arpHeader->ar_sip;

        // print out the information
        printf("-> ARP Reply: %s is at ", inet_ntoa(replied));
        for (i = 0; i < ETHER_ADDR_LEN; i++) {
            printf("%x", arpHeader->ar_sha[i]);
        }
        printf("hardware address\n");

        // cache the new arp entry
        arpEntry(arpHeader);
    }
}

/*---------------------------------------------------------------------
 * Method: void SendReply(struct sr_if*, struct sr_arphdr*,
 *                              struct sr_ethernet_hdr* ethernetHdr)
 *
 * Reply to requests
 * -------------------------------------------------------------------*/
void sendReply(
        struct sr_instance* sr,
        uint8_t* packet,
        unsigned int len,
        char* interface,
        struct sr_if * interfacePtr)
{
    struct sr_ethernet_hdr * ethernetHdr = (struct sr_ethernet_hdr*)packet;
    struct sr_arphdr * arpHeader = (struct sr_arphdr*)(packet+14);
    struct in_addr replied;
    int i;

    makeArp(arpHeader, arpHeader->ar_hrd, arpHeader->ar_pro, arpHeader->ar_hln, arpHeader->ar_pln, htons(ARP_REPLY),
            sr_get_interface(sr, interface)->addr, sr_get_interface(sr, interface)->ip,
            arpHeader->ar_sha, arpHeader->ar_sip);
    makeEthernet(ethernetHdr, ETHERTYPE_ARP, interfacePtr->addr, ethernetHdr->ether_shost);

    // Send the reply
    sr_send_packet(sr, packet, len, interface);

    // Print out the reply
    replied.s_addr = arpHeader->ar_sip;
    fprintf(stdout, "<- ARP Reply: %s is at ", inet_ntoa(replied));
    for (i = 0; i < ETHER_ADDR_LEN; i++) {
        printf("%x", arpHeader->ar_sha[i]);
    }
    printf (" --> hardware address in sendReply\n");
}

/*-----------------------------------------------------------------------------
 * Method: void arpEntry(struct sr_arphdr* arpHeader)
 *
 * store ARP info in network byte order in the cache
 *---------------------------------------------------------------------------*/
void arpEntry(struct sr_arphdr* arpHeader)
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



/*-----------------------------------------------------------------------------
 * Method: void makeArp( struct sr_arphdr* arpHeader, uint16_t arp_hrd, uint16_t arp_pro, uint8_t arp_hln, uint8_t arp_pln,
 *                      uint16_t arp_op, uint8_t* arp_sha, uint32_t arp_sip, uint8_t* arp_tha, uint32_t arp_tip )
 *
 * modifies an ARP packet with the parameters passed
 * assumes everything is in network byte order
 *---------------------------------------------------------------------------*/
void makeArp(
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
 * Method: void makeEthernet( struct sr_ethernet_hdr* ethernetHdr, uint16_t type, uint8_t* src, uint8_t* dst)
 *
 * modifies an Ethernet packet with the parameters passed
 *---------------------------------------------------------------------------*/
void makeEthernet(
        struct sr_ethernet_hdr* ethernetHdr,
        uint16_t type,
        uint8_t* src,
        uint8_t* dst )
{
    int i;
    uint8_t sbuf[ETHER_ADDR_LEN], dbuf[ETHER_ADDR_LEN];

    // read to buffer in case we are overwriting
    for (i = 0; i < ETHER_ADDR_LEN; i++) {
        sbuf[i] = src[i];
        dbuf[i] = dst[i];
    }

    ethernetHdr->ether_type = htons(type);
    for (i = 0; i < ETHER_ADDR_LEN; i++) {
        ethernetHdr->ether_shost[i] = sbuf[i];
        ethernetHdr->ether_dhost[i] = dbuf[i];
    }
}