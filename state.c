#include <stdio.h>
#include <assert.h>
#include <string.h>
#include <stdlib.h>
#include <netinet/in.h>
#include "state.h"
#include "slist.h"
#include "debug.h"

void init_client_state(client_state *s, struct sockaddr_in server, state_manager *sm)
{
    DPRINTF(4, "init client state\n");

    assert(s);
    s->server = server;
    s->ack_number = 0;
    s->downloading_chunk_hash = NULL;
    s->temp_data = NULL;
    s->received_byte_num = 0;

    sm->c_states[sm->c_size] = s;
    sm->c_size++;
    DPRINTF(4, "\n\n\n\n\n\n\n%d\n\n\n\n\n\n", sm->c_size);
}

void state_receiving_chunk(client_state *s, char *downloading_chunk_hash, int downloading_chunk_index)
{
    assert(s);
    s->downloading_chunk_hash = malloc(20);
    memcpy(s->downloading_chunk_hash, downloading_chunk_hash, 20);

    s->downloading_chunk_index = downloading_chunk_index;
    
    s->temp_data = malloc(512 * 1024);
}

void state_ack_update(client_state *s, int ack_number)
{
    assert(s);
    s->ack_number = ack_number;
}

void state_write_temp_data(client_state *s, char *temp_data, int length)
{
    assert(s);
    memcpy(s->temp_data + s->received_byte_num, temp_data, length);
}

void init_server_state(server_state *s, struct sockaddr_in client, state_manager *sm)
{
    DPRINTF(4, "init server state\n");
    assert(s);
    s->client = client;
    s->seq_number = 0;
    s->sending_chunk_hash = NULL;
    s->last_packet_acked = 0;
    s->duplicated_acks_count = 0;
    s->last_packet_available = 0;
    s->last_packet_sent = 0;
    s->sent_byte_num = 0;
    s->chunk_index = 0;
    sm->s_states[sm->s_size] = s;
    sm->s_size++;
}

void state_sending_chunk(server_state *s, char *sending_chunk_hash, int index)
{
    assert(s);
    s->sending_chunk_hash = malloc(20);
    memcpy(s->sending_chunk_hash, sending_chunk_hash, 20);
    s->chunk_index = index;
}

void state_seq_update(server_state *s, int seq_number)
{
    assert(s);
    s->seq_number = seq_number;
}

void init_state_manager(state_manager *sm, int MAX_CONN)
{
    assert(sm);
    sm->MAX_CONN = MAX_CONN;
    sm->c_states = malloc(sizeof(client_state *) * MAX_CONN);
    sm->s_states = malloc(sizeof(server_state *) * MAX_CONN);
    sm->c_size = 0;
    sm->s_size = 0;
}

client_state* find_c_state(state_manager *sm, struct sockaddr_in server)
{
    for (int i = 0; i < sm->MAX_CONN; i++)
    {   
        if(sm->c_states[i]!=NULL)
        {
            if(sm->c_states[i]->server.sin_addr.s_addr==server.sin_addr.s_addr && 
            sm->c_states[i]->server.sin_port==server.sin_port)
            {
                return sm->c_states[i];
            }
        }
    }
    return NULL;
}

int find_c_state_index(state_manager *sm, struct sockaddr_in server)
{
    for (int i = 0; i < sm->MAX_CONN; i++)
    {   
        if(sm->c_states[i]->server.sin_addr.s_addr==server.sin_addr.s_addr && 
            sm->c_states[i]->server.sin_port==server.sin_port)
        {
            return i;
        }
    }
    return -1;
}

server_state* find_s_state(state_manager *sm, struct sockaddr_in client)
{
    for (int i = 0; i < sm->MAX_CONN; i++)
    {   
        if(sm->s_states[i]!=NULL)
        {
            if(sm->s_states[i]->client.sin_addr.s_addr==client.sin_addr.s_addr && 
            sm->s_states[i]->client.sin_port==client.sin_port)
            {
                return sm->s_states[i];
            }
        }
    }
    return NULL;
}

int find_s_state_index(state_manager *sm, struct sockaddr_in client)
{
    for (int i = 0; i < sm->MAX_CONN; i++)
    {   
        if(sm->s_states[i]->client.sin_addr.s_addr==client.sin_addr.s_addr && 
            sm->s_states[i]->client.sin_port==client.sin_port)
        {
            return i;
        }
    }
    return -1;
}

void destroy_s_state(state_manager* sm, server_state* s)
{
    free(s);
    server_state* empty_state = malloc(sizeof(server_state *));
    sm->s_states[find_s_state_index(sm, s->client)] = empty_state;
}

void destroy_c_state(state_manager* sm, client_state* s)
{
    free(s);
    client_state* empty_state = malloc(sizeof(client_state *));
    sm->c_states[find_c_state_index(sm, s->server)] = empty_state;
}