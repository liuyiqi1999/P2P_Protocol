#include <stdio.h>
#include <assert.h>
#include <stdlib.h>
#include <stddef.h>
#include "slist.h"
#include "debug.h"

p_slist_node_t new_slist_pnode(char *data)
{
    p_slist_node_t pnode = malloc(sizeof(slist_node_t));
    pnode->data = malloc(strlen(data));
    strcpy(pnode->data, data);
    return pnode;
}

void slist_init(slist *s)
{
    assert(s);
    s->head = NULL;
}

void slist_push_back(slist *s, char *data)
{
    assert(s);
    p_slist_node_t p_new_node = new_slist_pnode(data);
    if (s->head == NULL)
    {
        s->head = p_new_node;
    }
    else
    {
        p_slist_node_t p_cur = s->head;
        while (p_cur->next)
        {
            p_cur = p_cur->next;
        }
        p_cur->next = p_new_node;
    }
}

p_slist_node_t slist_pop_front(slist *s)
{
    assert(s);
    if (s->head == NULL)
    {
        return NULL;
    }
    else if (s->head->next == NULL)
    {
        p_slist_node_t tmp = s->head;
        return tmp;
    }
    else
    {
        p_slist_node_t p_cur = s->head;
        s->head = p_cur->next;
        p_cur->next = NULL;
        return p_cur;
    }
}

p_slist_node_t slist_find(slist *s, int index)
{
    assert(s);
    if (s->head == NULL)
    {
        return NULL;
    }
    else
    {
        p_slist_node_t tmp = s->head;
        for(int i=0;i<index;i++)
        {
            tmp = tmp->next;
        }
        return tmp;
    }
}

int slist_size(slist *s)
{
    assert(s);
    int count = 0;
    p_slist_node_t p_cur = s->head;
    while (p_cur)
    {
        count++;
        p_cur = p_cur->next;
    }
    return count;
}

void slist_destroy(slist *s)
{
    assert(s);
    if (s->head == NULL)
    {
        free(s->head);
        return;
    }
    while (s->head)
    {
        p_slist_node_t tmp = s->head->next;
        free(s->head);
        s->head = tmp;
    }
}