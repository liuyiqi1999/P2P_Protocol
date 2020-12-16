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
        package->total_packet_length = header_len + 1024;
        package->seq_number = chunk_num;
        package->ack_number = 0;
        break;
    case 4: //ACK
        package->total_packet_length = header_len;
        package->seq_number = 0;
        package->ack_number = chunk_num;
        break;
    default:
        package->total_packet_length = header_len;
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
    c = c + package->header_length;
    memcpy(c,package->body,(package->total_packet_length-package->header_length));
    return tempc;
}