
#ifndef _CTABLE_H_
#define _CTABLE_H_

#include <stdbool.h>

#ifndef EXP_INTERVAL
#define EXP_INTERVAL 10
#endif

#ifndef INTERVAL_SECONDS
#define INTERVAL_SECONDS 60
#endif

#ifndef HT_SIZE
#define HT_SIZE 10
#endif

typedef struct net_node data_t;

/* defined in ctable.c */
void ctable_init (void);
int ctable_insert (const data_t *data);
int ctable_remove (const data_t *data);
bool ctable_contains (const data_t *data);
const data_t *ctable_get (const data_t *data);
void ctable_clear (void);
void ctable_foreach (int (*fun)(const data_t *it, void *arg), void *arg);

/* to be defined in another module */
extern unsigned int ctable_hash (const data_t*);
extern bool ctable_equals (const data_t*,const data_t*);
extern void ctable_act (const data_t*);
extern void ctable_free (data_t*);

#endif
