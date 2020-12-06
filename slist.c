#include <stdio.h>
#include <assert.h>
#include <stdlib.h>
#include <stddef.h>
#include "slist.h"
#include "debug.h"

p_slist_node_t new_slist_pnode(char *data, int data_length)
{
    DPRINTF(4, "enter new node\n");
    p_slist_node_t pnode = malloc(sizeof(slist_node_t));
    DPRINTF(4, "alloc new pnode, length: %d\n", data_length);
    pnode->data = malloc(data_length);
    DPRINTF(4, "alloc data\n");
    memcpy(pnode->data, data, data_length);
    DPRINTF(4, "after memcpy\n");
    pnode->next = NULL;
    DPRINTF(4, "after next\n");
    return pnode;
}

void slist_init(slist *s)
{
    assert(s);
    s->head = NULL;
}

void slist_push_back(slist *s, char *data, int data_length)
{
    DPRINTF(4, "begin push back\n");
    assert(s);
    p_slist_node_t p_new_node = new_slist_pnode(data, data_length);
    DPRINTF(4, "new node data: %s\n");
    if (s->head == NULL)
    {
        s->head = p_new_node;
    }
    else
    {
        p_slist_node_t p_cur = s->head;
        DPRINTF(4, "get head\n");
        if(p_cur->next->data!=NULL)
        {
            DPRINTF(4, "NOTNULL!\n");
        }
        while (p_cur->next!=NULL)
        {
            DPRINTF(4, "%s\n", p_cur->next->data);
            p_cur = p_cur->next;
        }
        DPRINTF(4, "get p_cur\n");
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
        for (int i = 0; i < index; i++)
        {
            tmp = tmp->next;
        }
        return tmp;
    }
}

int slist_search(slist *s, char *data, int length)
{
    assert(s);
    if (s->head == NULL)
    {
        return NULL;
    }
    else
    {
        p_slist_node_t tmp = s->head;
        int count = 0;
        while (tmp != NULL)
        {
            DPRINTF(4,"tmp->data: %20.20s, data: %20.20s\n", tmp->data, data);
            if (strncmp(tmp->data, data, 20) == 0)
            {
                return count;
            }
            if(tmp->next==NULL) return -1;
            else
            {
                tmp = tmp->next;
                count++;
            }
            
        }
        return -1;
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