#ifndef SERTANABENCHMARK_H
#define SERTANABENCHMARK_H

#include "rbtree.h"
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>

struct Socknode {
    struct rb_node node;
    int          sockfd;
    char         bitmap;
};

static int *connections ;
static int *failed      ;
static long long *bytes ;
static int *thread;

#define wait_for_debug() do{}while(0)

#define atmoic_fetch_add(ptr,value) do{__sync_fetch_and_add((ptr),(value));}while(0)
#define atmoic_fetch_sub(ptr,value) do{__sync_fetch_and_sub((ptr),(value));}while(0)
#define atmoic_read(ptr) __sync_fetch_and_add((ptr),(0))
#define atmoic_add_fetch(ptr,value)  __sync_add_and_fetch((ptr),(value))
#define atmoic_set(ptr,value) __sync_lock_test_and_set((ptr),(value))


struct Socknode * my_search(struct rb_root *root, int sockfd)
{
    struct rb_node *node = root->rb_node;

    while (node) {
        struct Socknode *data = container_of(node, struct Socknode, node);
        int result;

        result =  sockfd - data->sockfd;

        if (result < 0)
            node = node->rb_left;
        else if (result > 0)
            node = node->rb_right;
        else
            return data;
    }
    return NULL;
}

int my_insert(struct rb_root *root, struct Socknode *data)
{
    struct rb_node **new = &(root->rb_node), *parent = NULL;

    /* Figure out where to put new node */
    while (*new) {
        struct Socknode *this = container_of(*new, struct Socknode, node);
        int result =  data->sockfd-this->sockfd ;

        parent = *new;
        if (result < 0)
            new = &((*new)->rb_left);
        else if (result > 0)
            new = &((*new)->rb_right);
        else
            return 0;
    }

    /* Add new node and rebalance tree. */
    rb_link_node(&data->node, parent, new);
    rb_insert_color(&data->node, root);

    return 1;
}

#endif // SERTANABENCHMARK_H
