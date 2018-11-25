
#include <stdio.h>

#include "sr_protocol.h"

void makeethernet(
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

