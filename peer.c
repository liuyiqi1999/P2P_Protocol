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
#include "package.h"
#define MAX_LINE 1024

int sock;
bt_config_t config;

void peer_run(bt_config_t *config);

int main(int argc, char **argv)
{
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
    slist_push_back(chunk_hashes, buf);
  }
  int chunk_num = slist_size(chunk_hashes);
  char* data = (char *)malloc(4+chunk_num*4);
  char* temp = data;
  *(uint8_t *)data = chunk_num;
  *(uint8_t *)(data+1) = 0;
  *(uint16_t *)(data+2) = 0;
  for(int i=0;i<chunk_num;i++)
  {
    *(uint32_t *)(data+4*(i+1)) = slist_pop_front(chunk_hashes)->data;
  }

  package_t* whohas_package = malloc(sizeof(package_t));
  init_package(whohas_package, 0, 16, temp);
  char* msg = get_msg(whohas_package, 0);
  bt_peer_t *peers = config.peers;
  while(peers->next!=NULL)
  {
    spiffy_sendto(sock, msg, strlen(msg), 0, (struct sockaddr *)&peers->addr, sizeof(peers->addr));
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
