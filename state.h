#include <stdio.h>
#include <stdlib.h>
#include <netinet/in.h>

typedef struct client_state_s
{
    struct sockaddr_in server;
    int ack_number;
    char *downloading_chunk_hash;
    char *downloading_chunk_index;
    char *temp_data;
    int received_byte_num;
} client_state;

typedef struct server_state_s
{
    struct sockaddr_in client;
    int seq_number;
    char *sending_chunk_hash;

    int last_packet_acked;
    int duplicated_acks_count;
    int last_packet_available;
    int last_packet_sent;

    int sent_byte_num;

    int chunk_index;
} server_state;

typedef struct state_manager_s
{
    int MAX_CONN;
    client_state **c_states;
    server_state **s_states;
    int c_size;
    int s_size;
} state_manager;

void init_client_state(client_state *s, struct sockaddr_in server, state_manager *sm);

void state_receiving_chunk(client_state *s, char *downloading_chunk_hash, int downloading_chunk_index);

void state_ack_update(client_state *s, int ack_number);

void state_write_temp_data(client_state *s, char *temp_data, int length);

void init_server_state(server_state *s, struct sockaddr_in client, state_manager *sm);

void state_sending_chunk(server_state *s, char *sending_chunk_hash, int index);

void state_seq_update(server_state *s, int seq_number);

void init_state_manager(state_manager *sm, int MAX_CONN);

client_state* find_c_state(state_manager *sm, struct sockaddr_in server);

server_state* find_s_state(state_manager *sm, struct sockaddr_in client);

void destroy_s_state(state_manager *sm, server_state* s);

void destroy_c_state(state_manager *sm, client_state* s);