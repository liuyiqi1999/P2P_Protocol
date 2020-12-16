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
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <time.h>
#include "debug.h"
#include "spiffy.h"
#include "bt_parse.h"
#include "input_buffer.h"
#include "slist.h"
#include "state.h"
#include "chunk.h"
#include "package.h"
#include "sha.h"
#define MAX_LINE 1024

#define CLOCKID CLOCK_REALTIME
#define SIG_WHOHAS SIGRTMIN
#define SIG_SENDER SIGRTMIN + 1
#define SIG_RECEIVER SIGRTMIN + 2

#define errExit(msg)    \
  do                    \
  {                     \
    perror(msg);        \
    exit(EXIT_FAILURE); \
  } while (0)

int sock;
bt_config_t config;
slist *has_chunk_list;
slist *has_chunk_index_list;
char *output_file;
slist *chunk_hashes;
slist *downloading_chunk_hashes;
slist *data_list;
state_manager *sm;
FILE *output_fp;
timer_t whohas_timerid;
timer_t sender_timerid;
timer_t receiver_timerid;

void peer_run(bt_config_t *config);

static void
print_siginfo(siginfo_t *si)
{
  timer_t *tidp;
  int or ;

  tidp = si->si_value.sival_ptr;

  printf("    sival_ptr = %p; ", si->si_value.sival_ptr);
  printf("    *sival_ptr = %#jx\n", (uintmax_t)*tidp);

  or = timer_getoverrun(*tidp);
  if (or == -1)
    errExit("timer_getoverrun");
  else
    printf("    overrun count = %d\n", or);
}

void set_start_timer(timer_t timerid, int freq_secs)
{
  DPRINTF(4, "set timer %d\n", freq_secs);
  // set and start the timer
  struct itimerspec its;
  its.it_value.tv_sec = freq_secs;
  its.it_value.tv_nsec = 0;
  its.it_interval.tv_sec = freq_secs;
  its.it_interval.tv_nsec = 0;
  if (timer_settime(timerid, 0, &its, NULL) == -1)
  {
    errExit("timer_settime");
  }
}

timer_t create_timer(int SIGNAL, timer_t timerid, void *s)
{
  sigset_t mask;

  // block timer signal temporarily
  sigemptyset(&mask);
  sigaddset(&mask, SIGNAL);
  if (sigprocmask(SIG_SETMASK, &mask, NULL) == -1)
    errExit("sigprocmask");

  // create the timer
  struct sigevent sev;
  sev.sigev_notify = SIGEV_SIGNAL;
  sev.sigev_signo = SIGNAL;
  sev.sigev_value.sival_ptr = s;
  if (timer_create(CLOCKID, &sev, &timerid) == -1)
    errExit("timer_create");

  printf("timer ID is 0x%lx\n", (long)timerid);

  if (sigprocmask(SIG_UNBLOCK, &mask, NULL) == -1)
    errExit("sigprocmask");
  return timerid;
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
    char *chunk_as = malloc(50);
    char *index = malloc(5);
    sscanf(buf, "%s %s", index, chunk_as);
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
  for (int i = 0; i < chunk_num; i++)
  {
    char *chunk = slist_find(ihave_chunk_list, i)->data;
    memcpy(data + 4 + 20 * i, chunk, 20);
  }
  package_t *ihave_package = malloc(sizeof(package_t));
  init_package(ihave_package, 1, 16, temp, chunk_num);
  char *msg = get_msg(ihave_package, 1);
  spiffy_sendto(sock, msg, ihave_package->total_packet_length, 0, (struct sockaddr *)&to, sizeof(to));
  free(ihave_package);
  free(temp);
}

void send_get(char *chunk_hash, struct sockaddr_in to)
{
  package_t *get_package = malloc(sizeof(package_t));
  init_package(get_package, 2, 16, chunk_hash, 1);
  char *msg = get_msg(get_package, 2);
  spiffy_sendto(sock, msg, get_package->total_packet_length, 0, (struct sockaddr *)&to, sizeof(to));
  free(get_package);
}

void send_data(char *data, int seq_number, struct sockaddr_in to, server_state *s_state, int resend)
{
  if (s_state->sent_byte_num > 511 * 1024)
  {
    return;
  }
  DPRINTF(4, "sending %d\n", seq_number);
  package_t *data_package = malloc(sizeof(package_t));
  init_package(data_package, 3, 16, data, seq_number);
  char *msg = get_msg(data_package, 3);
  spiffy_sendto(sock, msg, data_package->total_packet_length, 0, (struct sockaddr *)&to, sizeof(to));

  if (resend == 0)
  {
    s_state->last_packet_sent = seq_number;
    s_state->sent_byte_num += data_package->total_packet_length - data_package->header_length;
  }
}

void send_ack(int ack_number, struct sockaddr_in to)
{
  package_t *ack_package = malloc(sizeof(package_t));
  init_package(ack_package, 4, 16, NULL, ack_number);
  char *msg = get_msg(ack_package, 4);
  spiffy_sendto(sock, msg, ack_package->total_packet_length, 0, (struct sockaddr *)&to, sizeof(to));
  free(ack_package);
}

void clear_chunk_hash(char *chunk_hash)
{
  if (chunk_hashes == NULL)
  {
    return;
  }

  int index = slist_search(chunk_hashes, chunk_hash, 20);
  if (index == -1)
  {
    return;
  }
  else
  {
    slist_pop_index(chunk_hashes, index);
  }
}

void init_downloading_chunk_hashes()
{
  if (downloading_chunk_hashes != NULL)
  {
    return;
  }

  downloading_chunk_hashes = malloc(sizeof(slist));
  slist_init(downloading_chunk_hashes);
}

void buffer_sending_data(int pos, server_state *s_state)
{
  if (data_list == NULL)
  {
    data_list = malloc(sizeof(slist));

    slist_init(data_list);

    for (int i = 0; i < 8; i++)
    {
      char *thousand_data = malloc(1024);
      slist_push_back(data_list, thousand_data, 1024);
      free(thousand_data);
    }
  }

  char *chunk_file = config.chunk_file;
  //DPRINTF(4, "chunk file: %s\n", config.chunk_file);
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
    buf[strlen(buf) - 1] = '\0';
    memcpy(master_file, buf + 6, 15);
  }
  //DPRINTF(4, "master_file: %s\n", master_file);
  fclose(master_chunk_fp);

  FILE *master_file_fp;
  master_file_fp = fopen(master_file, "r");
  if (master_file_fp == NULL)
  {
    perror("fail to read master_file. ");
    exit(1);
  }
  free(master_file);
  //int thousand_count = 0;
  char *thousand_data = malloc(1024);
  //char *temp=thousand_data;
  //DPRINTF(4, "chunk_index: %d\n", s_state->chunk_index);
  fseek(master_file_fp, (512 * 1024 * s_state->chunk_index) + (pos - 1) * 1024, SEEK_SET);
  //DPRINTF(4, "buffering from %d\n", ftell(master_file_fp));
  fread(thousand_data, 1024, 1, master_file_fp);
  //DPRINTF(4, "thousand_data: %s\n", thousand_data);
  fclose(master_file_fp);
  //DPRINTF(4, "i: %d\n", (pos-1)%8);
  slist_replace_index(data_list, (pos - 1) % 8, thousand_data, 1024);
  s_state->last_packet_available = pos;
  free(thousand_data);
}

void process_inbound_udp(int sock)
{
#define BUFLEN 1500
  struct sockaddr_in from;
  socklen_t fromlen;
  char buf[BUFLEN];

  if (sm == NULL)
  {
    sm = malloc(sizeof(state_manager));
    init_state_manager(sm, config.max_conn);
  }

  fromlen = sizeof(from);
  spiffy_recvfrom(sock, buf, BUFLEN, 0, (struct sockaddr *)&from, &fromlen);
  //        inet_ntoa(from.sin_addr),
  //        ntohs(from.sin_port),
  //        buf);

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
  DPRINTF(4, "\n\n\n");

  switch (package_type)
  {
  case 0: //server: receiving WHOHAS
  {
    init_has_chunk_list();
    uint8_t chunk_num = *(uint8_t *)(buf + 16); //ignoring the rest in this row
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
    if (slist_size(ihave_chunk_list) > 0)
      send_ihave(ihave_chunk_list, from);
    break;
  }
  case 1: //client: receiving IHAVE
  {
    if (chunk_hashes == NULL)
    {
      DPRINTF(4, "All data required is downloaded. \n");
      return;
    }

    char *chunk_hash = malloc(20);
    memcpy(chunk_hash, buf + 20, 20); //getting only the first chunk
    int index = slist_search(chunk_hashes, chunk_hash, 20);
    if (index == -1)
    {
      DPRINTF(4, "receiving IHAVE didn't asked. \n");
      break;
    }

    set_start_timer(whohas_timerid, 0);

    if (downloading_chunk_hashes != NULL)
    {
      int result = slist_search(downloading_chunk_hashes, chunk_hash, 20);
      if (result != -1)
      {
        DPRINTF(4, "already downloading this chunk\n");
        return;
      }
    }

    send_get(chunk_hash, from);

    init_downloading_chunk_hashes();
    slist_push_back(downloading_chunk_hashes, chunk_hash, 20);

    client_state *c_state = malloc(sizeof(client_state));
    init_client_state(c_state, from, sm); //set the client-end connection
    receiver_timerid = create_timer(SIG_RECEIVER, receiver_timerid, c_state);
    state_receiving_chunk(c_state, chunk_hash, index);
    free(chunk_hash);
    break;
  }
  case 2: //server: receiving GET
  {
    server_state *s_state = malloc(sizeof(server_state));
    init_server_state(s_state, from, sm); //set the server-end connection
    sender_timerid = create_timer(SIG_SENDER, sender_timerid, s_state);
    DPRINTF(4, "create sender timer\n");
    init_has_chunk_list(s_state);
    char *chunk_hash = malloc(20);
    memcpy(chunk_hash, buf + 16, 20);
    int result = slist_search(has_chunk_list, chunk_hash, 20);
    if (result == -1)
      DPRINTF(4, "GET packages I don't have. \n");
    else
    {
      int index = atoi(slist_find(has_chunk_index_list, result)->data);
      state_sending_chunk(s_state, chunk_hash, index);
      free(chunk_hash);
      //initiation: buffering the data of first 8KB
      for (int i = 0; i < 8; i++)
      {
        buffer_sending_data(i + 1, s_state);
      }
      for (int i = 0; i < 8; i++)
      {
        char *data = slist_find(data_list, i)->data;
        send_data(data, i + 1, s_state->client, s_state, 0);
        state_seq_update(s_state, i + 1);
      }
      set_start_timer(sender_timerid, 5);
    }
    break;
  }
  case 3: //client: receiving DATA
  {
    client_state *c_state = find_c_state(sm, from);
    if (c_state == NULL)
    {
      DPRINTF(4, "NULL\n");
      break;
    }
    int index_wish = slist_search(chunk_hashes, c_state->downloading_chunk_hash, 20);
    if (index_wish == -1)
    {
      DPRINTF(4, "receiving DATA not wishing. \n");
      break;
    }
    int index_downloading = slist_search(downloading_chunk_hashes, c_state->downloading_chunk_hash, 20);
    if (index_wish == -1)
    {
      DPRINTF(4, "receiving DATA not downloading. \n");
      break;
    }
    DPRINTF(4, "seq: %d, ack: %d\n", seq_number, c_state->ack_number);
    set_start_timer(receiver_timerid, 0);
    if (seq_number == c_state->ack_number + 1) //expected data packet
    {
      send_ack(seq_number, c_state->server);
      state_ack_update(c_state, seq_number);
      set_start_timer(receiver_timerid, 3);
    }
    else //duplicated ACK
    {
      send_ack(c_state->ack_number, c_state->server);
      set_start_timer(receiver_timerid, 3);
      break;
    }

    char *r_data = malloc(total_packet_length - header_length);
    memcpy(r_data, buf + header_length, total_packet_length - header_length);

    state_write_temp_data(c_state, r_data, total_packet_length - header_length);
    free(r_data);
    c_state->received_byte_num += total_packet_length - header_length;

    if (c_state->received_byte_num >= 512 * 1024) //downloading packet finish
    {
      DPRINTF(4, "\n\nDOWNLOADING FINISHED\n\n");
      timer_delete(receiver_timerid);
      receiver_timerid = NULL;

      slist_pop_index(downloading_chunk_hashes, index_downloading);
      DPRINTF(4, "Downloading chunk %d finished. \n", c_state->downloading_chunk_index);

      uint8_t *hash = malloc(SHA1_HASH_SIZE);
      shahash(c_state->temp_data, c_state->received_byte_num, hash);
      if (strncmp(hash, c_state->downloading_chunk_hash, 20) != 0)
      {
        DPRINTF(4, "Downloaded chunk %d SHA-1 error, resending whohas...\n", c_state->downloading_chunk_index);

        slist *reask_chunk_hash = malloc(sizeof(slist));
        slist_init(reask_chunk_hash);
        slist_push_back(reask_chunk_hash, c_state->downloading_chunk_hash, 20);
        send_whohas(reask_chunk_hash);
        //destroy_c_state(sm, c_state);
        free(hash);
        break;
      }
      else
      {
        free(hash);
        DPRINTF(4, "Downloaded chunk %d succeed, writing to file...\n", c_state->downloading_chunk_index);
        fseek(output_fp, index_downloading * 512 * 1024, SEEK_SET);
        if (fwrite(c_state->temp_data, 1, 512 * 1024, output_fp) >= 0)
        {
          DPRINTF(4, "Writing to file finished! \n");
        }
        destroy_c_state(sm, c_state);
        DPRINTF(4, "size: %d\n", slist_size(chunk_hashes));
        if (chunk_hashes != NULL && slist_size(chunk_hashes) > 1)
        {
          slist_pop_index(chunk_hashes, index_wish);
          send_whohas(chunk_hashes);
        }
        else if (chunk_hashes != NULL && slist_size(chunk_hashes) == 1)
        {
          //send_whohas(chunk_hashes);
          //DPRINTF(4, "All data is downloaded! \n");
          slist_destroy(chunk_hashes);
          fclose(output_fp);
          return;
        }
      }
    }
    break;
  }
  case 4: //server: receiving ACK
  {
    server_state *s_state = find_s_state(sm, from);
    if (s_state == NULL)
    {
      DPRINTF(4, "NULL!\n");
      break;
    }
    set_start_timer(sender_timerid, 0);
    if (ack_number == s_state->last_packet_acked) //duplicated ACKs
    {
      DPRINTF(4, "duplicated ACK\n");
      s_state->duplicated_acks_count++;
      set_start_timer(sender_timerid, 3);
      if (s_state->duplicated_acks_count >= 2)
      {
        char *resend_data = slist_find(data_list, (ack_number + 1 - 1) % 8)->data;
        send_data(resend_data, ack_number + 1, s_state->client, s_state, 1);
        DPRINTF(4, "resend packet %d\n", ack_number + 1);
        break;
      }
    }
    else if (ack_number < s_state->last_packet_acked) //older ACK: throwing away
    {
      s_state->duplicated_acks_count = 0;
      set_start_timer(sender_timerid, 3);
      DPRINTF(4, "throwed ack %d\n", ack_number);
      break;
    }
    else //normal ACK: cumulative acknowledgment
    {
      s_state->duplicated_acks_count = 0;
      //set_start_timer(sender_timerid, 0);
      DPRINTF(4, "normal ACK%d, last acked %d\n", ack_number, s_state->last_packet_acked);
      if (ack_number >= 512)
      {
        set_start_timer(sender_timerid, 0);
        DPRINTF(4, "all data in this chunk is sent! \n");
        slist_destroy(data_list);
        data_list = NULL;
        destroy_s_state(sm, s_state);

        timer_delete(sender_timerid);
        break;
      }

      int d = ack_number - s_state->last_packet_acked;
      int lpa = s_state->last_packet_available;
      for (int i = 0; i < d; i++)
      {
        buffer_sending_data(lpa + 1 + i, s_state); //buffer the skipped amount of packets
      }
      s_state->last_packet_acked = ack_number;

      int lps = s_state->last_packet_sent;
      for (int i = 1; i <= d; i++)
      {
        char *data = slist_find(data_list, (lps + i - 1) % 8)->data; //send the skipped amount of packets: making sure there're as many as 8 packets on sending
        //DPRINTF(4, "sent %d\n", s_state->last_packet_sent + i);
        send_data(data, lps + i, s_state->client, s_state, 0);
      }
      set_start_timer(sender_timerid, 3);
    }
  }
  }
}

void init_chunk_hashes(char *chunkfile)
{
  char buf[MAX_LINE];
  FILE *fp;

  if (chunk_hashes != NULL)
    return;

  chunk_hashes = malloc(sizeof(slist));
  slist_init(chunk_hashes);

  if ((fp = fopen(chunkfile, "r")) == NULL)
  {
    perror("Fail to read chunkfile. ");
    exit(1);
  }

  while (fgets(buf, MAX_LINE, fp) != NULL)
  {
    char *chunk_as = malloc(50);
    char *index = malloc(5);
    sscanf(buf, "%s %s", index, chunk_as);
    char *chunk = malloc(20);
    ascii2hex(chunk_as, 40, chunk);
    slist_push_back(chunk_hashes, chunk, 20);
    free(chunk_as);
    free(index);
    free(chunk);
  }
}

void send_whohas(slist *chunk_hashes)
{
  int chunk_num = slist_size(chunk_hashes);
  char *data = (char *)malloc(4 + chunk_num * 20);

  char *temp = data;
  *(uint8_t *)data = chunk_num;
  *(uint8_t *)(data + 1) = 0;
  *(uint16_t *)(data + 2) = 0;

  for (int i = 0; i < chunk_num; i++)
  {
    char *chunk = slist_find(chunk_hashes, i)->data;
    memcpy(data + 4 + 20 * i, chunk, 20);
  }
  package_t *whohas_package = malloc(sizeof(package_t));
  init_package(whohas_package, 0, 16, temp, chunk_num);
  char *msg = get_msg(whohas_package, 0);
  bt_peer_t *peers = config.peers;

  set_start_timer(whohas_timerid, 3);

  while (peers != NULL)
  {
    spiffy_sendto(sock, msg, whohas_package->total_packet_length, 0, (struct sockaddr *)&peers->addr, sizeof(peers->addr));
    peers = peers->next;
  }
  free(msg);
  free(temp);
}

void whohas_timer_handler(int sig, siginfo_t *si, void *uc)
{
  DPRINTF(4, "whohas handler\n");
  timer_delete(whohas_timerid);
  whohas_timerid = NULL;
  DPRINTF(4, "handling whohas timer expire\n");
  send_whohas(chunk_hashes);
  //print_siginfo(si);
  //signal(sig, SIG_IGN);
}

void sender_timer_handler(int sig, siginfo_t *si, void *uc)
{
  DPRINTF(4, "handling sender timer expire\n");
  server_state *s = si->si_value.sival_ptr;
  char *resend_data = slist_find(data_list, (s->last_packet_sent - 1) % 8)->data;
  DPRINTF(4, "resending %d\n", s->last_packet_sent);
  send_data(resend_data, s->last_packet_sent, s->client, s, 1);
  //print_siginfo(si);
  //signal(sig, SIG_IGN);
}

void receiver_timer_handler(int sig, siginfo_t *si, void *uc)
{
  DPRINTF(4, "handling receiver timer expire\n");
  client_state *s = si->si_value.sival_ptr;
  send_ack(s->ack_number, s->server);
  //print_siginfo(si);
  //signal(sig, SIG_IGN);
}

void process_get(char *chunkfile, char *outputfile)
{
  init_chunk_hashes(chunkfile); //chunks going to download from any connection

  if (sm == NULL)
  {
    sm = malloc(sizeof(state_manager));
    init_state_manager(sm, config.max_conn);
  }
  whohas_timerid = create_timer(SIG_WHOHAS, whohas_timerid, &whohas_timerid);
  send_whohas(chunk_hashes);
  output_file = malloc(128);
  memcpy(output_file, outputfile, 128);
  if ((output_fp = fopen(output_file, "w")) == NULL)
  {
    perror("fail to open output_file. ");
    exit(1);
  }
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
      //slist_destroy(data_list);
      //data_list=NULL;
      //fclose(output_fp);
      //slist_destroy(downloading_chunk_hashes);
      //slist_destroy(chunk_hashes);
      //free(output_file);
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

  // establish handler
  struct sigaction whohas_sa;
  whohas_sa.sa_flags = SA_SIGINFO;
  whohas_sa.sa_sigaction = whohas_timer_handler;
  sigemptyset(&whohas_sa.sa_mask);
  if (sigaction(SIG_WHOHAS, &whohas_sa, NULL) == -1)
    errExit("sigaction whohas");

  struct sigaction sender_sa;
  sender_sa.sa_flags = SA_SIGINFO;
  sender_sa.sa_sigaction = sender_timer_handler;
  sigemptyset(&sender_sa.sa_mask);
  if (sigaction(SIG_SENDER, &sender_sa, NULL) == -1)
    errExit("sigaction sender");

  struct sigaction receiver_sa;
  receiver_sa.sa_flags = SA_SIGINFO;
  receiver_sa.sa_sigaction = receiver_timer_handler;
  sigemptyset(&receiver_sa.sa_mask);
  if (sigaction(SIG_RECEIVER, &receiver_sa, NULL) == -1)
    errExit("sigaction receiver");

  DPRINTF(4, "init handlers\n");

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

int main(int argc, char **argv)
{
  has_chunk_list = NULL;
  has_chunk_index_list = NULL;
  chunk_hashes = NULL;
  output_file = NULL;
  downloading_chunk_hashes = NULL;
  data_list = NULL;
  sm = NULL;
  output_fp = NULL;
  whohas_timerid = NULL;
  sender_timerid = NULL;
  receiver_timerid = NULL;

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