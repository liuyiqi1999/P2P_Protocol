#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include "package.h"

int init_package(package_t *package, uint8_t type, uint16_t header_len, char *body)
{
    package->package_type = type;
    package->magic = 15441;
    package->version = 1;
    package->header_length = header_len;
    switch (type)
    {
    case 0: //WHOHAS
        package->total_packet_length = 60;
        package->seq_number = 0;
        package->ack_number = 0;
        break;
    case 3: //DATA
        package->total_packet_length = 1016;
        package->seq_number = 24;
        package->ack_number = 0;
        break;
    default:
        package->total_packet_length = 0;
        package->seq_number = 0;
        package->ack_number = 0;
        break;
    }
    package->body = body;
}

char *get_msg(package_t *package, uint8_t type)
{
    char *c = (char *)malloc(20 + strlen(package->body));
    if (c == NULL)
    {
        perror("Malloc message error! ");
        exit(1);
    }
    char *tempc = c;
    *(uint16_t *)c = package->magic;
    *(uint8_t *)(c + 2) = package->version;
    *(uint8_t *)(c + 3) = package->package_type;
    *(uint16_t *)(c + 4) = package->header_length;
    *(uint16_t *)(c + 6) = package->total_packet_length;
    *(u_int32_t *)(c + 8) = package->seq_number;
    *(u_int32_t *)(c + 10) = package->ack_number;
    while(*package->body != '\0')
    {
        *c++ = *package->body++;
    }
    return tempc;
}
