#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include "package.h"
#include "debug.h"

int init_package(package_t *package, uint8_t type, uint16_t header_len, char *body, int chunk_num)
{
    package->package_type = type;
    package->magic = 15441;
    package->version = 1;
    package->header_length = header_len;
    switch (type)
    {
    case 0: //WHOHAS
    case 1: //IHAVE
        package->total_packet_length = header_len + 4 + chunk_num * 20;
        package->seq_number = 0;
        package->ack_number = 0;
        break;
    case 2: //GET
        package->total_packet_length = header_len + 20;
        package->seq_number = 0;
        package->ack_number = 0;
        break;
    case 3: //DATA
        package->seq_number = 24;
        package->ack_number = 0;
        break;
    default:
        package->seq_number = 0;
        package->ack_number = 0;
        break;
    }
    package->body = body;
}

char *get_msg(package_t *package, uint8_t type)
{
    char *c = (char *)malloc(package->total_packet_length);
    if (c == NULL)
    {
        perror("Malloc message error! ");
        exit(1);
    }
    char *tempc = c;
    *(uint16_t *)c = htons(package->magic);
    *(uint8_t *)(c + 2) = package->version;
    *(uint8_t *)(c + 3) = package->package_type;
    *(uint16_t *)(c + 4) = htons(package->header_length);
    *(uint16_t *)(c + 6) = htons(package->total_packet_length);
    *(uint32_t *)(c + 8) = htonl(package->seq_number);
    *(uint32_t *)(c + 12) = htonl(package->ack_number);
    DPRINTF(4, "get msg, after header\n");
    c = c + package->header_length;
    DPRINTF(4, "before body\n");
    memcpy(c,package->body,(package->total_packet_length-package->header_length));
    DPRINTF(4,"sending header length %d\n", *(uint16_t*)(tempc+4));
    return tempc;
}
