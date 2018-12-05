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
void handleForward(struct sr_instance* sr, uint8_t* packet, unsigned int len, char* interface );
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
void arpSendRequest(struct sr_instance* sr,
		struct sr_if* interfacePTR, 
		uint32_t targetIP);
int isBroadcast(struct sr_ethernet_hdr* eHdr);
int packetForUs(struct sr_instance* sr, uint8_t* packet, const char* iface);


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

	// If this packet is a broadcast packet
	if(isBroadcast(ethernetHdr){
		handleArp(sr, packet, len, interface);
	}
	// If the packet is meant for us
	else if(packetForUs(sr, packet, interface)){
		
		//if our packet is an IP
		if (ntohs(ethernetHdr->ether_type) == ETHERTYPE_IP) {
			handleIP(sr, packet,len, interface);
		}
		//if our packet is an ARP
		if (ntohs(ethernetHdr->ether_type) == ETHERTYPE_ARP) {
			handleArp(sr, packet, len, interface);
		}
	}
	// The packet is not for us, so we must pass it along
	else{
		forwardPacket(sr, packet, len, interface);
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
 * Method: void handleForward(struct sr_instance*, uint8_t*,
 *                            unsigned int, char* )
 *
 * This will handle what to do when we have to forward a packet
 *---------------------------------------------------------------------*/
void handleForward(struct sr_instance* sr, uint8_t* packet, unsigned int len, char* interface ){
	
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
 * Method: void arpSendRequest(struct sr_instance* sr, struct sr_if* interfacePTR, uint32_t targetIP)
 *
 * Sends out an ARP request looking for the next hop's Ethernet Address
 *---------------------------------------------------------------------------*/
void arpSendRequest(struct sr_instance* sr, struct sr_if* interfacePTR, uint32_t targetIP){
	
	struct in_addr request;
	uint8_t broadcast[ETHER_ADDR_LEN];      
	
	
    uint8_t* requestPacket = malloc(42 * sizeof(uint8_t));
    if (requestPacket == NULL) {
        fprintf(stderr, "Error: malloc failed to make a new request packet\n");
        return;
    }
	
	memset(requestPacket, 0, 42 * sizeof(uint8_t));
	
	struct sr_ethernet_hdr* ethernetHdr = (struct sr_ethernet_hdr*) requestPacket;
	struct sr_arphdr* arpHeader = (struct sr_arphdr*)(requestPacket+14);
	
	int i=0;
	for(i = 0; i<ETHER_ADDR_LEN; i++){
		broadcast[i] = 0xff;
	}
	
	makeArp(arpHeader, htons(ARPHDR_ETHER), htons(ETHERTYPE_IP), 6, 4, htons(ARP_REQUEST), iface->addr, iface->ip,
            broadcast, tip);
			
	makeEthernet(ethernetHdr, ETHERTYPE_ARP, interfacePTR->addr, broadcast);
	
	// Send the request
	sr_send_packet(sr, requestPacket, sizeof(requestPacket), iface->name);
	
	request.s_addr = targetIP;
	printf("<- ARP Request: who has %s?\n", inet_ntoa(requested));
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


/*-----------------------------------------------------------------------------
 * Method: void isBroadcast(struct sr_ethernet_hdr* eHdr)
 *
 * determines if this ethernet packet is a broadcast packet or not
 *---------------------------------------------------------------------------*/
int isBroadcast(struct sr_ethernet_hdr* eHdr){
	
	int i=0;
	for(i=0; i<ETHER_ADDR_LEN; i++){
		if(eHdr->ether_dhost != 0xff){
			return 0;  // This is not a broadcast packet
		}
	}
	
	return 1; // This is a broadcast packet
}

/*-----------------------------------------------------------------------------
 * Method: void packetForUS(struct sr_instance* sr, uint8_t* packet, const char* iface)
 *
 * determines if this packet was meant for us, or if we have to forward this packet
 *---------------------------------------------------------------------------*/
int packetForUs(struct sr_instance* sr, uint8_t* packet, const char* iface){
	
	struct sr_ethernet_hdr* eHdr = (struct sr_ethernet_hdr*)packet;
    struct sr_if* incoming_if = sr_get_interface(sr, interface);

	// ARP packet
    if (ntohs(eHdr->ether_type) == ETHERTYPE_ARP) {
        struct sr_arphdr* arpHdr = (struct sr_arphdr*)(packet+14);
        if (incoming_if->ip == arpHdr->ar_tip)
            return 1;
    } 
	
	// IP packet
	else if (ntohs(eHdr->ether_type) == ETHERTYPE_IP) {
        struct ip* ipHdr = (struct ip*)(packet+14);
        if (incoming_if->ip == ipHdr->ip_dst.s_addr)
            return 1;
    }

    return 0;
}





