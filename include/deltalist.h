#ifndef _PSNET_DELTALIST_H_
#define _PSNET_DELTALIST_H_

#ifndef HT_SIZE
#define HT_SIZE 10
#endif

typedef void data_t;

struct delta_list {
    unsigned int resolution;       // seconds per tick
    unsigned int interval;         // expiration inverval (measured in ticks)
    unsigned int size;             // number of elements in the list
    unsigned int delta;            // sum of all individual deltas

    struct delta_node *delta_head; // head of the delta list
    struct delta_node *delta_tail; // tail of the delta list

    /* functions that operate on data_t */
    unsigned long (* const hash)(const data_t*);
    int (* const equals)(const data_t*,const data_t*);
    void (* const act)(const data_t*);
    void (* const free)(data_t*);

    pthread_mutex_t lock;

    struct delta_node *table[HT_SIZE]; // memory for the hash table
};

void delta_init (struct delta_list *table);
void delta_insert (struct delta_list *table, const data_t *data);
int delta_update (struct delta_list *table, const data_t *data);
int delta_remove (struct delta_list *table, const data_t *data);
int delta_contains (struct delta_list *table, const data_t *data);
const data_t *delta_get (struct delta_list *table, const data_t *data);
void delta_clear (struct delta_list *table);
void delta_foreach (struct delta_list *table,
        int (*fun)(const data_t *it, void *arg), void *arg);
unsigned int delta_size (struct delta_list *table);
#endif
