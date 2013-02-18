#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <pthread.h>
#include <unistd.h>

#include "ctable.h"

static void ctable_tick ();

struct ct_node {
    const data_t *data;
    unsigned int delta;      // delta for delta list
    struct ct_node *ht_next; // hash table next pointer
    struct ct_node *dl_next; // delta list next pointer
    struct ct_node *dl_prev; // delta list prev pointer
};

/* metadata structure */
static struct {
    unsigned int delta;         // sum of all individual deltas
    struct ct_node *delta_head; // head of the delta list
    struct ct_node *delta_tail; // tail of the delta list

    /* functions that operate on data_t */
    unsigned int (* const hash)(const data_t*);
    bool (* const equals)(const data_t*,const data_t*);
    void (* const act)(data_t*);

    struct ct_node *table[HT_SIZE]; // memory for the hash table
} hash_table = {
    .delta = 0,
    .delta_head = NULL,
    .delta_tail = NULL,
    .hash = ctable_hash,
    .equals = ctable_equals,
    .act = ctable_act
};

/* XXX: global mutex to protect data structure;
 * consider replacing with read-write lock */
static pthread_mutex_t ctable_lock;

/*-----------------------------------------------------------------------------
 * Clock thread: periodically calls the ctable_tick() function  */
//-----------------------------------------------------------------------------
static void __attribute((noreturn)) *ctable_clock_thread () {
    for (;;) {
        sleep (INTERVAL_SECONDS);
        ctable_tick ();
    }
}

/*-----------------------------------------------------------------------------
 * Initializes the ctable */
//-----------------------------------------------------------------------------
void ctable_init (void) {
    pthread_t tid;

    pthread_mutex_init (&ctable_lock, NULL);
    pthread_create (&tid, NULL, ctable_clock_thread, NULL);
}

/*-----------------------------------------------------------------------------
 * Increases "time" by one tick */
//-----------------------------------------------------------------------------
static void ctable_tick () {
    data_t *tmp_data;

    pthread_mutex_lock (&ctable_lock);

    if (!hash_table.delta_head) {
        pthread_mutex_unlock (&ctable_lock);
        return;
    }

    hash_table.delta--;
    hash_table.delta_head->delta--;

    // remove any expired elements
    while (hash_table.delta_head && !hash_table.delta_head->delta) {
        tmp_data = (data_t*) hash_table.delta_head->data;
        hash_table.delta_head = hash_table.delta_head->dl_next;

        pthread_mutex_unlock (&ctable_lock);
        ctable_remove (tmp_data);
        pthread_mutex_lock (&ctable_lock);

        hash_table.act (tmp_data);
    }
    pthread_mutex_unlock (&ctable_lock);
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
int ctable_insert (const data_t *data) {
    struct ct_node *node;

    ctable_remove (data);

    node = malloc (sizeof (struct ct_node));
    node->data = data;

    pthread_mutex_lock (&ctable_lock);
    hash_insert (node);
    delta_insert (node);
    pthread_mutex_unlock (&ctable_lock);

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
 * Removes an element from the table.  Returns 0 on success, or -1 if the given
 * element is not in the table */
//-----------------------------------------------------------------------------
int ctable_remove (const data_t *data) {
    unsigned int index;
    struct ct_node *node, *prev;

    pthread_mutex_lock (&ctable_lock);

    if (!(node = get_node (data, &prev))) {
        pthread_mutex_unlock (&ctable_lock);
        return -1;
    }

    // remove from hash table
    if (prev) {
        prev->ht_next = node->ht_next;
    } else {
        index = hash_table.hash (data) % HT_SIZE;
        hash_table.table[index] = node->ht_next;
    }

    // remove from delta list
    if (node->dl_next) {
        node->dl_next->delta += node->delta;
        node->dl_next->dl_prev = node->dl_prev;
    } else {
        hash_table.delta -= node->delta;
    }

    if (node->dl_prev)
        node->dl_prev->dl_next = node->dl_next;
    else
        hash_table.delta_head = node->dl_next;

    free (node);

    pthread_mutex_unlock (&ctable_lock);
    return 0;
}

/*-----------------------------------------------------------------------------
 * Returns true if the given element exists in the table, or false if it does
 * not */
//-----------------------------------------------------------------------------
bool ctable_contains (const data_t *data) {
    pthread_mutex_lock (&ctable_lock);
    struct ct_node *rv = get_node (data, NULL);
    pthread_mutex_unlock (&ctable_lock);

    return rv ? true : false;
}

/*-----------------------------------------------------------------------------
 * Returns the element in the table equal to the given value (equal being
 * defined by the function dh_equals) if such an element exists.  Otherwise
 * returns NULL */
//-----------------------------------------------------------------------------
const data_t *ctable_get (const data_t *data) {
    struct ct_node *node;

    pthread_mutex_lock (&ctable_lock);
    node = get_node (data, NULL);
    pthread_mutex_unlock (&ctable_lock);

    return node ? node->data : NULL;
}

/*-----------------------------------------------------------------------------
 * Empties the table */
//-----------------------------------------------------------------------------
void ctable_clear (void) {
    struct ct_node *it, *tmp;

    pthread_mutex_lock (&ctable_lock);

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

    pthread_mutex_unlock (&ctable_lock);
}

/*-----------------------------------------------------------------------------
 * Calls the function `fun' on each element in the list.  A non-zero return
 * value from `fun' is taken to indicate that iteration should cease */
//-----------------------------------------------------------------------------
void ctable_foreach (int (*fun)(const data_t *it, void *arg), void *arg) {
    struct ct_node *it;

    pthread_mutex_lock (&ctable_lock);
    for (it = hash_table.delta_head; it; it = it->dl_next) {
        if (fun (it->data, arg))
            break;
    }
    pthread_mutex_unlock (&ctable_lock);
}
