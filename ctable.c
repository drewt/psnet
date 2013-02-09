#include <stdlib.h>
#include <string.h>

#include "ctable.h"

struct ct_node {
    const data_t *data;
    unsigned int delta;      // delta for delta list
    struct ct_node *ht_next; // hash table next pointer
    struct ct_node *dl_next; // delta list next pointer
    struct ct_node *dl_prev; // delta list prev pointer
};

static struct {
    unsigned int delta;
    struct ct_node *delta_head;
    struct ct_node *delta_tail;
    unsigned int (* const hash)(const data_t*);
    bool (* const equals)(const data_t*,const data_t*);
    void (* const act)(const data_t*);
    struct ct_node *table[HT_SIZE];
} hash_table = {
    .delta = 0,
    .delta_head = NULL,
    .delta_tail = NULL,
    .hash = ctable_hash,
    .equals = ctable_equals,
    .act = ctable_act
};

/*-----------------------------------------------------------------------------
 * Increases "time" by one tick */
//-----------------------------------------------------------------------------
int ctable_tick (void) {
    struct ct_node *tmp;

    if (!hash_table.delta_head)
        return 0;

    hash_table.delta--;
    hash_table.delta_head->delta--;

    // remove any expired elements
    while (hash_table.delta_head && !hash_table.delta_head->delta) {
        tmp = hash_table.delta_head;
        hash_table.delta_head = hash_table.delta_head->dl_next;
        hash_table.act (tmp->data);
        ctable_remove (tmp->data);
    }
    return 0;
}

/*-----------------------------------------------------------------------------
 * Inserts a node into a bucket in the hash table */
//-----------------------------------------------------------------------------
static void hash_insert (struct ct_node *node) {
    unsigned int index = hash_table.hash (node->data) % HT_SIZE;

    node->ht_next = hash_table.table[index];
    hash_table.table[index] = node;
}

/*-----------------------------------------------------------------------------
 * Inserts a node into the delta list */
//-----------------------------------------------------------------------------
static void delta_insert (struct ct_node *node) {

    node->delta = EXP_INTERVAL;

    if (!hash_table.delta_head) {
        hash_table.delta_head = hash_table.delta_tail = node;
    } else {
        node->delta -= hash_table.delta;

        hash_table.delta_tail->dl_next = node;
        node->dl_prev = hash_table.delta_tail;
        hash_table.delta_tail = node;
    }
    node->dl_next = NULL;

    hash_table.delta = EXP_INTERVAL;
}

/*-----------------------------------------------------------------------------
 * Inserts an element into the table */
//-----------------------------------------------------------------------------
int *ctable_insert (const data_t *data) {
    struct ct_node *node;

    node = malloc (sizeof (struct ct_node));
    node->data = data;

    hash_insert (node);
    delta_insert (node);

    return 0;
}

/*-----------------------------------------------------------------------------
 * Finds the struct dh_node associated with a given element, if that element
 * exists in the table.  If the element does not exist, NULL is returned. If
 * `prev' is not NULL, then prev will be set to the previous element in the
 * hash table bucket when this function returns */
//-----------------------------------------------------------------------------
static struct ct_node *get_node (const data_t *data, struct ct_node **prev) {

    unsigned int index;
    struct ct_node *it, *last;

    index = hash_table.hash (data) % HT_SIZE;

    last = NULL;
    for (it = hash_table.table[index]; it; it = it->ht_next) {
        if (hash_table.equals (it->data, data))
            break;
        last = it;
    }

    if (prev)
        *prev = last; // NULL if data was bucket head

    return it;
}

/*-----------------------------------------------------------------------------
 * Removes an element from the table */
//-----------------------------------------------------------------------------
int *ctable_remove (const data_t *data) {
    unsigned int index;
    struct ct_node *node, *prev;

    node = get_node (data, &prev);

    // remove from hash table
    if (prev) {
        prev->ht_next = node->ht_next;
    } else {
        index = hash_table.hash (data) % HT_SIZE;
        hash_table.table[index] = node->ht_next;
    }

    // adjust deltas
    if (node->dl_next) {
        node->dl_next->delta += node->delta;
        node->dl_next->dl_prev = node->dl_prev;
    } else {
        hash_table.delta -= node->delta;
    }

    // adjust links in delta list
    if (node->dl_prev)
        node->dl_prev->dl_next = node->dl_next;
    else
        hash_table.delta_head = node->dl_next;

    free (node);
    return 0;
}

/*-----------------------------------------------------------------------------
 * Returns true if the given element exists in the table, or false if it does
 * not */
//-----------------------------------------------------------------------------
bool ctable_contains (const data_t *data) {
    return get_node (data, NULL) ? true : false;
}

/*-----------------------------------------------------------------------------
 * Returns the element in the table equal to the given value (equal being
 * defined by the function dh_equals) if such an element exists.  Otherwise
 * returns NULL */
//-----------------------------------------------------------------------------
const data_t *ctable_get (const data_t *data) {
    struct ct_node *node;

    if ((node = get_node (data, NULL)))
        return node->data;
    return NULL;
}

/*-----------------------------------------------------------------------------
 * Empties the table */
//-----------------------------------------------------------------------------
void ctable_clear (void) {
    struct ct_node *it, *tmp;

    it = hash_table.delta_head;
    while (it) {
        tmp = it;
        it = it->dl_next;
        free (tmp);
    }

    hash_table.delta = 0;
    hash_table.delta_head = NULL;
    hash_table.delta_tail = NULL;

    for (int i = 0; i < HT_SIZE; i++)
        hash_table.table[i] = NULL;
}
