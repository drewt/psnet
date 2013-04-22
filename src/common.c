#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>

#include "jsmn.h"

#include "common.h"

/*-----------------------------------------------------------------------------
 * Parses a message into jsmn tokens */
//-----------------------------------------------------------------------------
int parse_message (const char *msg, jsmntok_t *tok, size_t *ntok)
{
    int rc;
    jsmn_parser p;

    jsmn_init (&p);
    rc = jsmn_parse (&p, msg, tok, *ntok);
    if (rc != JSMN_SUCCESS || tok[0].type != JSMN_OBJECT)
        return -1;
    *ntok =  p.toknext;
    return jsmn_get_value (msg, tok, "method");
}

#ifdef DAEMON
/*-----------------------------------------------------------------------------
 * Run the server in the background */
//-----------------------------------------------------------------------------
void daemonize (void)
{
    pid_t pid, sid;

    pid = fork ();
    if (pid == -1) {
        perror ("fork");
        exit (EXIT_FAILURE);
    }
    if (pid > 0)
        exit (EXIT_SUCCESS);

    umask (0);

    freopen (LOG_FILE_PATH, "w", stdout);
    freopen (LOG_FILE_PATH, "w", stderr);
    fclose (stdin);

    sid = setsid ();
    if (sid == -1) {
        perror ("setsid");
        exit (EXIT_FAILURE);
    }

    if (chdir ("/") == -1) {
        perror ("chdir");
        exit (EXIT_FAILURE);
    }
}
#endif


