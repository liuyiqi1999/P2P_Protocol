#include <stdio.h>
#include <assert.h>
#include <stdlib.h>
#include <stddef.h>
#include "slist.h"
#include "debug.h"

p_slist_node_t new_slist_pnode(char *data, int data_length)
{
    p_slist_node_t pnode = malloc(sizeof(slist_node_t));
    pnode->data = malloc(data_length);
    memcpy(pnode->data, data, data_length);
    pnode->next = NULL;
    return pnode;
}

void slist_init(slist *s)
{
    assert(s);
    s->head = NULL;
}

void slist_push_back(slist *s, char *data, int data_length)
{
    assert(s);
    p_slist_node_t p_new_node = new_slist_pnode(data, data_length);
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

void slist_pop_index(slist*s, int index)
{
    assert(s);
    if(s->head==NULL||index<0)
    {
        return;
    }
    else
    {
        if(index==0)
        {
            slist_pop_front(s);
        }
        else
        {
            p_slist_node_t t = slist_find(s, index);
            p_slist_node_t last = slist_find(s, index-1);
            if(t->next!=NULL)
            {
                last->next = t->next;
            }
            else
            {
                last->next = NULL;
            }
        }
    }
}

void slist_replace_index(slist *s, int index, char *data, int length)
{
    assert(s);

    p_slist_node_t old_node = slist_find(s, index);
    if(old_node==NULL)
        return;
    else
    {
        free(old_node->data);
        old_node->data = malloc(length);
        memcpy(old_node->data, data, length);
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
        return -1;
    }
    else
    {
        p_slist_node_t tmp = s->head;
        int count = 0;
        while (tmp != NULL)
        {
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