

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

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

/*---------------------------------------------------------------------------------
* Method: void handleIcmp(struct sr_instance*, uint8_t*, unsigned int, char*);
*
* Determines what kind of ICMP packet received
*------------------------------------------------------------------------------------*/
void handleIcmp(struct sr_instance* sr,
          uint8_t* packet,
          unsigned int len,
          char* interface)
{
    struct icmp_hdr * icmpHeader = (struct icmp_hdr*)(packet+34);

    if (icmpHeader->icmp_type == ICMP_ECHO_REQUEST) {
        fprintf(stdout, "--> ICMP Type: %2.2x -> ECHO \n", icmpHeader->icmp_type);
        icmpSendEchoReply(sr, packet, len, interface);
    }
}

/*-----------------------------------------------------------------------------
 * Method: void icmpSendEchoReply(struct sr_instance* sr, uint8_t* packet,
 *                                  unsigned int len, char* interface )
 *
 * reply to incoming ICMP request
 *---------------------------------------------------------------------------*/
void icmpSendEchoReply(
        struct sr_instance* sr,
        uint8_t* packet, 
        unsigned int len,
        char* interface)
{
    // organize the packet
    struct sr_ethernet_hdr* ethernetHeader = (struct sr_ethernet_hdr*)packet;
    struct ip* ipHeader = (struct ip*)(packet+14);
    struct icmp_hdr* icmpHeader = (struct icmp_hdr*)(packet+34);

    // modify packet to be sent back
    makeicmp(icmpHeader, ICMP_ECHO_REPLY, 0, 64);
    makeip(ipHeader, len-14, IP_DF, 64, IPPROTO_ICMP, 
                sr_get_interface(sr, interface)->ip, ipHeader->ip_src.s_addr);
    makeethernet(ethernetHeader, ETHERTYPE_IP, 
                sr_get_interface(sr, interface)->addr, ethernetHeader->ether_shost);

    
    sr_send_packet(sr, packet, len, interface);
    
    
    printf("<-- ICMP ECHO reply sent to %s\n", inet_ntoa(ipHeader->ip_dst));
}

/*-----------------------------------------------------------------------------
 * Method: void icmpSendEchoReply(struct sr_instance* sr, uint8_t* packet,
 *                                  unsigned int len, char* interface )
 *
 * reply to incoming ICMP request
 *---------------------------------------------------------------------------*/
void icmpSendUnreachable(
        struct sr_instance* sr,
        uint8_t* packet, 
        unsigned int len,
        char* interface,
        uint8_t type)
{
    // allocate memory for our new packet
    uint8_t* icmpPacket = malloc(70 * sizeof(uint8_t));
    if (icmpPacket == NULL) {
        fprintf(stderr, "Error: malloc failed on ICMP packet\n");
        return;
    }
    memset(icmpPacket, 0, 70 * sizeof(uint8_t));

    // organize the src packet
    struct sr_ethernet_hdr* srcethernetHdr = (struct sr_ethernet_hdr*)packet;
    struct ip* srcipHdr = (struct ip*)(packet+14);

    // organize pointers for the new packet
    struct sr_ethernet_hdr* newEthernetHeader = (struct sr_ethernet_hdr*)icmpPacket;
    struct ip* newipHeader = (struct ip*)(icmpPacket+14);
    struct icmp_hdr* newicmpHeader = (struct icmp_hdr*)(icmpPacket+34);
    uint8_t* newicmpData = (uint8_t*)(icmpPacket+42);

    // copy src ip header + tcp/udp ports to icmp data
    memcpy(newicmpData, srcipHdr, 28);

    // create icmp, ip and ethernet headers on our new packet
    makeicmp(newicmpHeader, ICMP_DST_UNREACHABLE, type, 36);
    makeip(newipHeader, 70-14, IP_DF, 64, IPPROTO_ICMP,
            sr_get_interface(sr, interface)->ip, srcipHdr->ip_src.s_addr);
    makeethernet(newEthernetHeader, ETHERTYPE_IP,
            sr_get_interface(sr, interface)->addr, srcethernetHdr->ether_shost);
        
    sr_send_packet(sr, icmpPacket, 70, interface);

    if (type == ICMP_PORT_UNREACHABLE)
        printf("<-- ICMP Destination Port Unreachable sent to %s\n", inet_ntoa(newipHeader->ip_dst));
    if (type == ICMP_HOST_UNREACHABLE)
        printf("<-- ICMP Destination Host Unreachable sent to %s\n", inet_ntoa(newipHeader->ip_dst));

    free(icmpPacket);
}

/*-----------------------------------------------------------------------------
 * Method void makeicmp(struct icmp_hdr* icmpHeader, uint8_t type, uint8_t code)
 *
 * assumes everything is in host byte order
 *---------------------------------------------------------------------------*/
void makeicmp(
        struct icmp_hdr * icmpHeader,
        uint8_t type,
        uint8_t code,
        int len)
{
    icmpHeader->icmp_type = type;
    icmpHeader->icmp_code = code;

    icmpHeader->icmp_checksum = 0x0000;
    icmpHeader->icmp_checksum = in_checksum((uint16_t*)icmpHeader, len);
}
