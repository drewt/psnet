/* Copyright 2013 Drew Thoreson */

/* This file is part of psnet
 *
 * This program is free software: you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the Free
 * Software Foundation, either version 3 of the License, or (at your option)
 * any later version.
 *
 * psnet is distributed in the hope that it will be useful, but WITHOUT ANY
 * WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 * FOR A PARTICULAR PURPOSE.  See the GNU General Public License for more
 * details.
 *
 * You should have received a copy of the GNU General Public License along with
 * psnet.  If not, see <http://www.gnu.org/licenses/>
 */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <pthread.h>
#include <getopt.h>

#include "ini.h"
#define JSMN_STRICT
#include "jsmn.h"

#include "client.h"
#include "misc.h"
#include "msgcache.h"
#include "network.h"
#include "parse.h"
#include "protocol.h"
#include "router.h"
#include "server.h"

#define RC_FILE "/etc/psnetrc"

#define MAX_HOPS 4

#define HDR_OK_FMT "{\"status\":\"okay\",\"size\":%d}\r\n\r\n"
#define HDR_OK_STRLEN (29 + 5)

static struct settings {
	int max_threads;
	char *dir_addr;
	char *dir_port;
	char *listen_port;
} settings = {
	.max_threads = 1000,
	.dir_addr = "psnet.no-ip.biz",
	.dir_port = "6666",
	.listen_port = "5555",
};

#define node_error(sock, no) psnet_send_error(sock, no, psnode_strerror[no])
enum input_errors { ENOMETHOD, ENONUM, EBADMETHOD, EBADNUM };
static const char *psnode_strerror[] = {
	[ENOMETHOD]  = "no method given",
	[ENONUM]     = "missing argument 'num'",
	[EBADMETHOD] = "unrecognized method",
	[EBADNUM]    = "invalid argument 'num'"
};

int num_threads;
pthread_mutex_t num_threads_lock;

static void process_ip(struct msg_info *mi, jsmntok_t *tok, size_t ntok)
{
	char addr[INET6_ADDRSTRLEN];
	char hdr[HDR_OK_STRLEN];
	char rsp[13 + INET6_ADDRSTRLEN];
	int hdr_len, rsp_len;

	inet_ntop(mi->addr.ss_family, get_in_addr((struct sockaddr*) &mi->addr),
			addr, sizeof addr);
	rsp_len = sprintf(rsp, "{\"ip\":\"%s\"}\r\n\r\n", addr);
	hdr_len = sprintf(hdr, HDR_OK_FMT, rsp_len);

	tcp_send_bytes(mi->sock, hdr, hdr_len);
	tcp_send_bytes(mi->sock, rsp, rsp_len);
}

static void process_info(struct msg_info *mi, jsmntok_t *tok, size_t ntok)
{
	char hdr[HDR_OK_STRLEN];
	char rsp[60 + 10 + 10]; /* 20 digits for clients and cache-load */
	int hdr_len, rsp_len;

	rsp_len = sprintf(rsp, "{\"name\":\"generic psnet router\","
			"\"clients\":%d,\"cache-load\":%d}\r\n\r\n",
			client_list_size(), msg_cache_size());
	hdr_len = sprintf(hdr, HDR_OK_FMT, rsp_len);

	tcp_send_bytes(mi->sock, hdr, hdr_len);
	tcp_send_bytes(mi->sock, rsp, rsp_len);
}

static void process_ping(struct msg_info *mi, jsmntok_t *tok, int ntok)
{
	psnet_send_ok(mi->sock);

#ifdef PSNETLOG
	printf(ANSI_YELLOW "P %s %d\n" ANSI_RESET, mi->paddr,
			get_in_port((struct sockaddr*)&mi->addr));
#endif
}

static void process_connect(struct msg_info *mi, jsmntok_t *tok, int ntok)
{
	int port;

	if((port = jsmn_get_value(mi->msg, tok, "port")) == -1)
		return;

	mi->msg[tok[port].end] = '\0';

	if(add_client(&mi->addr, mi->msg + tok[port].start))
		return;

#ifdef PSNETLOG
	printf(ANSI_GREEN "+ %s %s\n" ANSI_RESET, mi->paddr,
			mi->msg + tok[port].start);
#endif
}

/*
 * Processes a search query: increments the 'hops' field (discarding the
 * message if it's reached the hop limit) and forwards the message to all known
 * routers and clients.
 */
static void process_broadcast(struct msg_info *mi, jsmntok_t *tok, int ntok)
{
	int hops, id;
	char *msg = mi->msg;
	char *msgid;
	char v;

	if (jsmn_get_values(msg, tok, "hops", &hops, "id", &id, (void*) NULL)
			== -1)
	return;

	v = msg[tok[hops].start];
	if (v < '0' || v >= '0' + MAX_HOPS - 1)
		return; // hop limit reached
	msg[tok[hops].start]++;

	msgid = jsmn_tokdup(msg, &tok[id]);
	if (cache_msg(msgid)) {
		free(msgid);
		return;
	}

	flood_message(mi);

#ifdef PSNETLOG
	printf(ANSI_YELLOW "F %s\n" ANSI_RESET, msgid);
#endif
}

static void process_discover(struct msg_info *mi, jsmntok_t *tok, int ntok)
{
	LIST_HEAD(head);
	LIST_HEAD(jlist);
	int num;

	if ((num = jsmn_get_value(mi->msg, tok, "num")) == -1) {
		node_error(mi->sock, ENONUM);
		return;
	}

	if (!(num = atoi(mi->msg + tok[num].start))) {
		node_error(mi->sock, EBADNUM);
		return;
	}

	routers_to_json(&jlist, num);
	make_response_with_body(&head, &jlist);
	psnet_send_response(mi->sock, &head);
	free_response(&head);

#ifdef PSNETLOG
	printf(ANSI_YELLOW "L %s\n" ANSI_RESET, mi->paddr);
#endif
}

/*
 * Handles a TCP connection (callback for tcp_server_main())
 */
static void *handle_connection(void *data)
{
	struct msg_info *mi = data;
	jsmntok_t tok[JSMN_NTOK];
	size_t ntok = JSMN_NTOK;
	int method;

	for(;;) {

		if (tcp_read_msg(mi->sock, mi->msg, MSG_MAX) <= 0)
			break; /* connection closed by client */

		/* dispatch */
		#define cmd_equal(cmd) jsmn_tokeq(mi->msg, &tok[method], cmd)
		if ((method = parse_message(mi->msg, tok, &ntok)) == -1) {
			node_error(mi->sock, ENOMETHOD);
			break;
		} else if (cmd_equal("broadcast")) {
			process_broadcast(mi, tok, ntok);
		} else if (cmd_equal("ip")) {
			process_ip(mi, tok, ntok);
		} else if (cmd_equal("info")) {
			process_info(mi, tok, ntok);
		} else if (cmd_equal("ping")) {
			process_ping(mi, tok, ntok);
		} else if (cmd_equal("discover")) {
			process_discover(mi, tok, ntok);
		} else {
			node_error(mi->sock, EBADMETHOD);
			break;
		}
		#undef cmd_equal
	}

	/* clean up */
	close(mi->sock);
	pthread_mutex_lock(&num_threads_lock);
	num_threads--;
	pthread_mutex_unlock(&num_threads_lock);
#ifdef PSNETLOG
	printf("D %s\n", mi->paddr);
#endif
	free(mi);
	pthread_exit(NULL);
}

/*
 * Handles a UDP message (callback for udp_server_main())
 */
static void *handle_message(void *data)
{
	struct msg_info *mi = data;
	jsmntok_t tok[JSMN_NTOK];
	size_t ntok = JSMN_NTOK;
	int method;

	/* dispatch */
	#define cmd_equal(cmd) jsmn_tokeq(mi->msg, &tok[method], cmd)
	if ((method = parse_message(mi->msg, tok, &ntok)) == -1)
		goto cleanup;
	else if (cmd_equal("connect"))
		process_connect(mi, tok, ntok);
	else if (cmd_equal("broadcast"))
		process_broadcast(mi, tok, ntok);
	#undef cmd_equal

cleanup:
	pthread_mutex_lock(&num_threads_lock);
	num_threads--;
	pthread_mutex_unlock(&num_threads_lock);
	free(mi);
#ifdef PSNETLOG
	printf("-M %s\n", mi->paddr);
#endif
	pthread_exit(NULL);
}

static _Noreturn void usage(void)
{
	puts("usage: infranode [port]\n"
		"       where 'port' is the port number to listen on");
	exit(EXIT_FAILURE);
}

static void *udp_serve(void *data)
{
	int sockfd;

	pthread_detach(pthread_self());

	sockfd = udp_server_init(((struct settings*)data)->listen_port);
	udp_server_main(sockfd,((struct settings*)data)->max_threads,
			handle_message);
}

static int ini_handler(void *user, const char *section, const char *name,
        const char *value)
{
	int val;

	if (strcmp(section, "Router"))
		return 1;

	if (!strcmp(name, "directory-address")) {
		settings.dir_addr = strdup(value);
	} else if (!strcmp(name, "directory-port")) {
		if (!(val = atoi(value))) {
			printf("%s: error: directory-port must be a positive integer\n",
				(char*) user);
		} else {
			settings.dir_port = strdup(value);
		}
	} else if (!strcmp(name, "listen-port")) {
		if (!(val = atoi(value))) {
			printf("%s: error: listen-port must be a positive integer\n",
				(char*) user);
		} else {
			settings.listen_port = strdup(value);
		}
	} else if (!strcmp(name, "max-threads")) {
		if (!(val = atoi(value))) {
			printf("%s: error: max-threads must be a positive integer\n",
				(char*) user);
		} else {
			settings.max_threads = val;
		}
	}
	return 1;
}

void parse_opts(int argc, char *argv[], struct settings *dst)
{
	int c;
	char *endptr;

	for(;;) {
		static struct option long_options[] = {
			{ "max-threads",       required_argument, 0, 't' },
			{ "listen-port",       required_argument, 0, 'l' },
			{ "directory-address", required_argument, 0, 'a' },
			{ "directory-port",    required_argument, 0, 'p' },
			{ 0, 0, 0, 0 }
		};

		int options_index = 0;

		c = getopt_long(argc, argv, "t:l:a:p:", long_options, &options_index);

		if (c == -1)
			break;

		switch (c) {
		case 't':
			endptr = NULL;
			dst->max_threads = (int) strtol(optarg, &endptr, 10);
			if (dst->max_threads < 1 || (endptr && *endptr != '\0')) {
				puts("error: --threads argument must be a positive integer");
				usage();
			}
			break;

		case 'l':
			endptr = NULL;
			dst->listen_port = optarg;
			if (strtol(optarg, &endptr, 10) < 1
					|| (endptr && *endptr != '\0')) {
				puts("error: --listen-port argument "
					"must be a positive integer");
				usage();
			}
			break;

		case 'a':
			dst->dir_addr = optarg; // XXX no validity check
			break;

		case 'p':
			endptr = NULL;
			dst->dir_port = optarg;
			if (strtol(optarg, &endptr, 10) < 1
					|| (endptr && *endptr != '\0')) {
				puts("error: --directory-port argument "
					"must be a positive integer");
				usage();
			}
			break;

		case '?':
			break;

		default:
			usage();
		}
	}
}

int main(int argc, char *argv[])
{
	int sockfd;
	pthread_t tid;

	if (ini_parse(RC_FILE, ini_handler, RC_FILE))
		printf("Warning: failed to parse %s\n", RC_FILE);
	parse_opts(argc, argv, &settings);

#ifdef DAEMON
	daemonize();
#endif

	num_threads = 0;
	pthread_mutex_init(&num_threads_lock, NULL);

	clients_init();
	msg_cache_init();
	router_init(settings.dir_addr, settings.dir_port, settings.listen_port);

	if (pthread_create(&tid, NULL, udp_serve, &settings))
		perror("pthread_create");

	sockfd = tcp_server_init(settings.listen_port);
	tcp_server_main(sockfd, settings.max_threads, handle_connection);
}
