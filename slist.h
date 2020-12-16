#include <stdio.h>
#include <stdlib.h>

typedef struct slist_node
{
    char *data;
    struct slist_node *next;
} slist_node_t, *p_slist_node_t;

p_slist_node_t new_slist_pnode(char *data, int data_length);

typedef struct slist
{
    p_slist_node_t head;
} slist;

void slist_init(slist *s);

void slist_push_back(slist *s, char *data, int data_length);

void slist_pop_index(slist *s, int index);

void slist_replace_index(slist *s, int index, char *data, int length);

p_slist_node_t slist_pop_front(slist *s);

p_slist_node_t slist_find(slist *s, int index);

int slist_search(slist *s, char *data, int length);

int slist_size(slist *s);

void slist_destroy(slist *s);