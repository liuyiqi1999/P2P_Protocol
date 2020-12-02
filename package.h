#include <stdio.h>
#include <stdlib.h>

typedef struct package
{
    uint16_t magic;
    uint8_t version;
    uint8_t package_type;
    uint16_t header_length;
    uint16_t total_packet_length;
    uint32_t seq_number;
    uint32_t ack_number;
    char *body;
} package_t;

int init_package(package_t *package, uint8_t type, uint16_t header_len, char *body);

char *get_msg(package_t *package, uint8_t type);
