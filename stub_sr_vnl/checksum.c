#include <stdint.h>

uint16_t in_checksum(uint16_t* addr, int count)
{
    register uint32_t sum = 0;

    while (count > 1) {
        sum += *addr++;
        count -= 2;
    }

    if (count > 0)
        sum += *((uint8_t*)addr);

    while (sum >> 16)
        sum = (sum & 0xffff) + (sum >> 16);

    return(~sum);
}