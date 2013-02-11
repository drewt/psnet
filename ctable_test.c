#include <stdio.h>
#include <pthread.h>
#include <unistd.h>
#include "ctable.h"

#define LENGTH(arr) (sizeof (arr) / sizeof (*arr))

struct client {
    int id;
};

unsigned int ctable_hash (const struct client *client) {
    return client->id;
}
bool ctable_equals (const struct client *a, const struct client *b) {
    return a->id == b->id;
}
void ctable_act (const struct client *client) {
    printf ("Removed client %d\n", client->id);
}

int hash_test (void) {
    struct client data[HT_SIZE*2];

    for (int i = 0; i < HT_SIZE*2; i++)
        data[i].id = i;

    // repeatedly fill & empty buckets
    for (int i = 0; i < 2; i++) {
        for (int j = 0; i < HT_SIZE; i++) {
            if (ctable_contains (&data[j])) {
                printf ("error: %d in table\n", data[j].id);
                return 1;
            }
            if (ctable_contains (&data[j+HT_SIZE])) {
                printf ("error: %d in table\n", data[j+HT_SIZE].id);
                return 2;
            }
            ctable_insert (&data[j]);
            ctable_insert (&data[j+HT_SIZE]);
            if (!ctable_contains (&data[j])) {
                printf ("error: %d not in table\n", data[j].id);
                return 3;
            }
            else if (!ctable_contains (&data[j+HT_SIZE])) {
                printf ("error: %d not in table\n", data[j+HT_SIZE].id);
                return 4;
            }
            ctable_remove (&data[j]);
            if (ctable_contains (&data[j])) {
                printf ("error: %d still in table\n", data[j].id);
                return 5;
            }
            if (!ctable_contains (&data[j+HT_SIZE])) {
                printf ("error: %d not in table\n", data[j+HT_SIZE].id);
                return 6;
            }
            ctable_remove (&data[j+HT_SIZE]);
            if (ctable_contains (&data[j+HT_SIZE])) {
                printf ("error: %d still in table\n", data[j+HT_SIZE].id);
                return 7;
            }
        }
    }

    // remove from the head of a bucket
    ctable_insert (&data[0]);
    ctable_insert (&data[HT_SIZE]);
    ctable_remove (&data[HT_SIZE]);
    if (ctable_contains (&data[HT_SIZE])) {
        printf ("error: %d still in table\n", data[HT_SIZE].id);
        return 8;
    }
    if (!ctable_contains (&data[0])) {
        printf ("error: %d not in table\n", data[0].id);
        return 9;
    }
    ctable_remove (&data[0]);
    if (ctable_contains (&data[0])) {
        printf ("error: %d still in table\n", data[0].id);
        return 10;
    }

    return 0;
}

int delta_test (void) {
    struct client clients[5];

    for (size_t i = 0; i < LENGTH(clients); i++) {
        clients[i].id = i;
        ctable_insert (&clients[i]);
    }

    sleep (EXP_INTERVAL*INTERVAL_SECONDS+1);

    for (size_t i = 0; i < LENGTH(clients); i++) {
        if (ctable_contains (&clients[i])) {
            printf ("error: %d still in table\n", clients[i].id);
            return 1;
        }
    }

    for (size_t i = 0; i < LENGTH(clients); i++) {
        ctable_insert (&clients[i]);
        sleep (EXP_INTERVAL*INTERVAL_SECONDS+1);
        if (ctable_contains (&clients[i])) {
            printf ("error: %d still in table\n", clients[i].id);
            return 2;
        }
    }
    return 0;
}

int main (void) {
    int rc;

    ctable_init ();

    if ((rc = hash_test ())) {
        printf ("hash_test returned %d\n", rc);
        return 1;
    }
    if ((rc = delta_test ())) {
        printf ("delta_test returned %d\n", rc);
        return 2;
    }
    return 0;
}
