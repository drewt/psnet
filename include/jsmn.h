/* Copyright 2013 Drew Thoreson */

#ifndef __JSMN_H_
#define __JSMN_H_

/**
 * JSON type identifier. Basic types are:
 * 	o Object
 * 	o Array
 * 	o String
 * 	o Other primitive: number, boolean (true/false) or null
 */
typedef enum {
	JSMN_PRIMITIVE = 0,
	JSMN_OBJECT = 1,
	JSMN_ARRAY = 2,
	JSMN_STRING = 3
} jsmntype_t;

typedef enum {
	/* Not enough tokens were provided */
	JSMN_ERROR_NOMEM = -1,
	/* Invalid character inside JSON string */
	JSMN_ERROR_INVAL = -2,
	/* The string is not a full JSON packet, more bytes expected */
	JSMN_ERROR_PART = -3,
	/* Everything was fine */
	JSMN_SUCCESS = 0
} jsmnerr_t;

/**
 * JSON token description.
 * @param		type	type (object, array, string etc.)
 * @param		start	start position in JSON data string
 * @param		end		end position in JSON data string
 */
typedef struct jsmntok {
	jsmntype_t type;
	int start;
	int end;
	int size;
#ifdef JSMN_PARENT_LINKS
	int parent;
#endif
} jsmntok_t;

/**
 * JSON parser. Contains an array of token blocks available. Also stores
 * the string being parsed now and current position in that string
 */
typedef struct {
	unsigned int pos; /* offset in the JSON string */
	int toknext; /* next token to allocate */
	int toksuper; /* superior token node, e.g parent object or array */
} jsmn_parser;

/**
 * Create JSON parser over an array of tokens
 */
void jsmn_init(jsmn_parser *parser);

/**
 * Run JSON parser. It parses a JSON data string into and array of tokens, each describing
 * a single JSON object.
 */
jsmnerr_t jsmn_parse(jsmn_parser *parser, const char *js, 
		jsmntok_t *tokens, unsigned int num_tokens);

// XXX: additions

#include <string.h>

int jsmn_get_value (const char *msg, jsmntok_t *tok, const char *key);
int jsmn_get_values (const char *msg, jsmntok_t *obj, ...);
char *jsmn_tokdup (const char *msg, jsmntok_t *tok);

static inline int jsmn_toklen (jsmntok_t *tok)
{
    return tok->end - tok->start;
}

static inline int jsmn_tokeq (const char *msg, jsmntok_t *tok,
        const char *str)
{
    int str_len = strlen (str);
    if (jsmn_toklen (tok) != str_len)
        return 0;
    return (!strncmp (msg + tok->start, str, str_len));
}

#endif /* __JSMN_H_ */
