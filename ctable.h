
#ifndef _DELTAHASH_H_
#define _DELTAHASH_H_

#include <stdbool.h>

#ifndef EXP_INTERVAL
#define EXP_INTERVAL 10
#endif

#ifndef HT_SIZE
#define HT_SIZE 10
#endif

typedef struct client data_t;

/* defined in ctable.c */
int ctable_tick (void);
int *ctable_insert (data_t *data);
int *ctable_remove (data_t *data);
bool ctable_contains (data_t *data);
data_t *ctable_get (data_t *data);
void ctable_clear (void);

/* to be defined in another module */
extern unsigned int ctable_hash (data_t*);
extern bool ctable_equals (data_t*,data_t*);
extern void ctable_act (data_t*);

#endif
