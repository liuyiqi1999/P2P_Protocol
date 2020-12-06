/*
 * peer.c
 * 
 * Author: Yi Lu <19212010040@fudan.edu.cn>,
 *
 * Modified from CMU 15-441,
 * Original Authors: Ed Bardsley <ebardsle+441@andrew.cmu.edu>,
 *                   Dave Andersen
 * 
 * Class: Networks (Spring 2015)
 *
 */

#include <sys/types.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "debug.h"
#include "spiffy.h"
#include "bt_parse.h"
#include "input_buffer.h"
#include "slist.h"
#include "chunk.h"
#include "package.h"
#define MAX_LINE 1024

int sock;
bt_config_t config;
slist *has_chunk_list;
slist *has_chunk_index_list;

void peer_run(bt_config_t *config);

int main(int argc, char **argv)
{
  has_chunk_list = NULL;

  bt_init(&config, argc, argv);

  DPRINTF(DEBUG_INIT, "peer.c main beginning\n");

#ifdef TESTING
  config.identity = 1; // your group number here
  strcpy(config.chunk_file, "chunkfile");
  strcpy(config.has_chunk_file, "haschunks");
#endif

  bt_parse_command_line(&config);

#ifdef DEBUG
  if (debug & DEBUG_INIT)
  {
    bt_dump_config(&config);
  }
#endif

  peer_run(&config);
  return 0;
}

void init_has_chunk_list()
{
  if (has_chunk_list != NULL || has_chunk_index_list != NULL)
  {
    return;
  }
  has_chunk_list = malloc(sizeof(slist));
  slist_init(has_chunk_list);
  has_chunk_index_list = malloc(sizeof(slist));
  slist_init(has_chunk_index_list);

  char buf[MAX_LINE];
  FILE *fp;

  char *has_chunk_file = config.has_chunk_file;
  if ((fp = fopen(has_chunk_file, "r")) == NULL)
  {
    perror("Fail to read has_chunk_file. ");
    exit(1);
  }

  while (fgets(buf, MAX_LINE, fp) != NULL)
  {
    char *chunk_as = malloc(40);
    char *index = malloc(4);
    sscanf(buf, "%d %s", index, chunk_as);
    char *chunk = malloc(20);
    ascii2hex(chunk_as, 40, chunk);
    slist_push_back(has_chunk_list, chunk, 20);
    slist_push_back(has_chunk_index_list, index, 4);
    free(chunk_as);
    free(index);
    free(chunk);
  }
  fclose(fp);
}

void send_ihave(slist *ihave_chunk_list, struct sockaddr_in to)
{
  int chunk_num = slist_size(ihave_chunk_list);
  char *data = (char *)malloc(4 + chunk_num * 20);
  char *temp = data;
  *(uint8_t *)data = chunk_num;
  *(uint8_t *)(data + 1) = 0;
  *(uint16_t *)(data + 2) = 0;

  DPRINTF(4, "init ihave\n");

  for (int i = 0; i < chunk_num; i++)
  {
    char *chunk = slist_find(ihave_chunk_list, i)->data;
    memcpy(data + 4 + 20 * i, chunk, strlen(chunk) - 1);
  }
  package_t *ihave_package = malloc(sizeof(package_t));
  init_package(ihave_package, 1, 16, temp, chunk_num);
  char *msg = get_msg(ihave_package, 1);
  spiffy_sendto(sock, msg, ihave_package->total_packet_length, 0, (struct sockaddr *)&to, sizeof(to));
  free(ihave_package);
}

void send_get(char *chunk_hash, struct sockaddr_in to)
{
  DPRINTF(4, "send_get: %s\n", chunk_hash);
  package_t *get_package = malloc(sizeof(package_t));
  init_package(get_package, 2, 16, chunk_hash, 1);
  char *msg = get_msg(get_package, 2);
  spiffy_sendto(sock, msg, get_package->total_packet_length, 0, (struct sockaddr *)&to, sizeof(to));
  free(get_package);
}

FILE *open_master_file()
{
  char *chunk_file = config.chunk_file;
  char buf[MAX_LINE];
  FILE *master_chunk_fp;
  if ((master_chunk_fp = fopen(chunk_file, "r")) == NULL)
  {
    perror("fail to read chunk_file. ");
    exit(1);
  }
  char *master_file = malloc(MAX_LINE);
  if (fgets(buf, MAX_LINE, master_chunk_fp) != NULL)
  {
    memcpy(master_file, buf + 6, MAX_LINE);
  }
  fclose(master_chunk_fp);

  FILE *master_file_fp;
  DPRINTF(4, "master_file: %s\n", master_file);
  if ((master_file_fp = fopen(master_file, "r")) == NULL)
  {
    perror("fail to read master_file. ");
    exit(1);
  }

  free(master_file);
  return master_file_fp;
}

void process_inbound_udp(int sock)
{
#define BUFLEN 1500
  struct sockaddr_in from;
  socklen_t fromlen;
  char buf[BUFLEN];

  fromlen = sizeof(from);
  spiffy_recvfrom(sock, buf, BUFLEN, 0, (struct sockaddr *)&from, &fromlen);

  printf("PROCESS_INBOUND_UDP SKELETON -- replace!\n"
         "Incoming message from %s:%d\n%s\n\n",
         inet_ntoa(from.sin_addr),
         ntohs(from.sin_port),
         buf);

  uint16_t magic = ntohs(*(uint16_t *)buf);
  uint8_t version = *(uint8_t *)(buf + 2);
  uint8_t package_type = *(uint8_t *)(buf + 3);
  uint16_t header_length = ntohs(*(uint16_t *)(buf + 4));
  uint16_t total_packet_length = ntohs(*(uint16_t *)(buf + 6));
  uint32_t seq_number = ntohl(*(uint32_t *)(buf + 8));
  uint32_t ack_number = ntohl(*(uint32_t *)(buf + 12));

  if (magic != 15441)
  {
    DPRINTF(4, "Magic number error! \n");
    return;
  }
  if (version != 1)
  {
    DPRINTF(4, "Version error! \n");
    return;
  }

  DPRINTF(4, "package_type: %d\n", package_type);
  DPRINTF(4, "header_length: %d\n", header_length);
  DPRINTF(4, "total_packet_length: %d\n", total_packet_length);
  DPRINTF(4, "seq_number: %d\n", seq_number);
  DPRINTF(4, "ack_number: %d\n", ack_number);
  switch (package_type)
  {
  case 0: //receiving WHOHAS
    init_has_chunk_list();

    uint8_t chunk_num = *(uint8_t *)(buf + 16);
    //ignoring the rest in this row

    slist *ihave_chunk_list = malloc(sizeof(slist));
    slist_init(ihave_chunk_list);
    for (int i = 20; i < total_packet_length; i += 20)
    {
      char *chunk_hash = malloc(20);
      memcpy(chunk_hash, buf + i, 20);
      int result = slist_search(has_chunk_list, chunk_hash, 20);
      if (result == -1)
        continue;
      else
      {
        slist_push_back(ihave_chunk_list, chunk_hash, 20);
      }
      free(chunk_hash);
    }
    DPRINTF(4, "after for\n");
    send_ihave(ihave_chunk_list, from);
    break;
  case 1: //receiving IHAVE
    for (int i = 20; i < total_packet_length; i += 20)
    {
      char *chunk_hash = malloc(20);
      memcpy(chunk_hash, buf + i, 20);
      send_get(chunk_hash, from);
      free(chunk_hash);
    }
    break;
  case 2: //receiving GET
    init_has_chunk_list();
    char *chunk_hash = malloc(20);
    memcpy(chunk_hash, buf + 16, 20);
    int result = slist_search(has_chunk_list, chunk_hash, 20);
    free(chunk_hash);
    if (result == -1)
      DPRINTF(4, "GET packages I don't have. \n");
    else
    {
      char *index = slist_find(has_chunk_index_list, result)->data;
      FILE *master_file_fp = open_master_file();

      //initiation: buffering the data of first 8KB
      slist* data_list = malloc(sizeof(slist));
      slist_init(data_list);
      for (int i = 0; i < 8; i++)
      {
        int thousand_count = 0;
        char *thousand_data = malloc(1000);
        fseek(master_file_fp, 512 * 1000 * (int)index, SEEK_SET);
        char c;
        while ((c = fgetc(master_file_fp)) != EOF)
        {
          if (thousand_count >= 1000)
            break;
          *thousand_data = c;
          thousand_count++;
          thousand_data++;
        }
        slist_push_back(data_list, thousand_data, 1000);
      }
      //TODO: free data_list
    }
  }
}

void send_whohas(char *chunkfile)
{
  char buf[MAX_LINE];
  FILE *fp;

  slist *chunk_hashes = malloc(sizeof(slist));

  slist_init(chunk_hashes);

  if ((fp = fopen(chunkfile, "r")) == NULL)
  {
    perror("Fail to read chunkfile. ");
    exit(1);
  }

  while (fgets(buf, MAX_LINE, fp) != NULL)
  {
    char *chunk_as = malloc(40);
    char *index = malloc(4);
    sscanf(buf, "%d %s",index, chunk_as);
    char *chunk = malloc(20);
    ascii2hex(chunk_as, 40, chunk);
    slist_push_back(chunk_hashes, chunk, 20);
  }
  int chunk_num = slist_size(chunk_hashes);
  char *data = (char *)malloc(4 + chunk_num * 20);

  char *temp = data;
  *(uint8_t *)data = chunk_num;
  *(uint8_t *)(data + 1) = 0;
  *(uint16_t *)(data + 2) = 0;

  for (int i = 0; i < chunk_num; i++)
  {
    char *chunk = slist_find(chunk_hashes, i)->data;
    memcpy(data + 4 + 20 * i, chunk, strlen(chunk) - 1);
  }
  package_t *whohas_package = malloc(sizeof(package_t));
  init_package(whohas_package, 0, 16, temp, chunk_num);
  char *msg = get_msg(whohas_package, 0);
  bt_peer_t *peers = config.peers;
  while (peers->next != NULL)
  {
    spiffy_sendto(sock, msg, whohas_package->total_packet_length, 0, (struct sockaddr *)&peers->addr, sizeof(peers->addr));
    peers = peers->next;
  }
}

void process_get(char *chunkfile, char *outputfile)
{
  send_whohas(chunkfile);
}

void handle_user_input(char *line, void *cbdata)
{
  char chunkf[128], outf[128];

  bzero(chunkf, sizeof(chunkf));
  bzero(outf, sizeof(outf));

  if (sscanf(line, "GET %120s %120s", chunkf, outf))
  {
    if (strlen(outf) > 0)
    {
      process_get(chunkf, outf);
    }
  }
}

void peer_run(bt_config_t *config)
{
  struct sockaddr_in myaddr;
  fd_set readfds;
  struct user_iobuf *userbuf;

  if ((userbuf = create_userbuf()) == NULL)
  {
    perror("peer_run could not allocate userbuf");
    exit(-1);
  }

  if ((sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_IP)) == -1)
  {
    perror("peer_run could not create socket");
    exit(-1);
  }

  bzero(&myaddr, sizeof(myaddr));
  myaddr.sin_family = AF_INET;
  myaddr.sin_addr.s_addr = htonl(INADDR_ANY);
  myaddr.sin_port = htons(config->myport);

  if (bind(sock, (struct sockaddr *)&myaddr, sizeof(myaddr)) == -1)
  {
    perror("peer_run could not bind socket");
    exit(-1);
  }

  spiffy_init(config->identity, (struct sockaddr *)&myaddr, sizeof(myaddr));

  while (1)
  {
    int nfds;
    FD_SET(STDIN_FILENO, &readfds);
    FD_SET(sock, &readfds);

    nfds = select(sock + 1, &readfds, NULL, NULL, NULL);

    if (nfds > 0)
    {
      if (FD_ISSET(sock, &readfds))
      {
        process_inbound_udp(sock);
      }

      if (FD_ISSET(STDIN_FILENO, &readfds))
      {
        process_user_input(STDIN_FILENO, userbuf, handle_user_input,
                           "Currently unused");
      }
    }
  }
}
