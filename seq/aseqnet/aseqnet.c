/*
 * network server/client for ALSA sequencer
 *   ver.0.1
 *
 * Copyright (C) 1999-2000 Takashi Iwai
 * 
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License version 2 as
 *  published by the Free Software Foundation.
 * 
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <string.h>
#include <netinet/in.h>
#include <netdb.h>
#include <sys/asoundlib.h>
#include <getopt.h>
#include <signal.h>

/*
 * prototypes
 */
static void usage(void);
static void init_buf(void);
static void close_files(void);
static void init_seq(char *source, char *dest);
static int get_port(char *service);
static void sigterm_exit(int sig);
static void init_server(int port);
static void init_client(char *server, int port);
static void do_loop(void);
static int copy_local_to_remote(void);
static int copy_remote_to_local(int fd);

/*
 * default TCP port number
 */
#define DEFAULT_PORT	40002

/*
 * local input buffer
 */
static char *readbuf;
static int max_rdlen;
static char *writebuf;
static int cur_wrlen, max_wrlen;

#define MAX_BUF_EVENTS	200
#define MAX_CONNECTION	10

static snd_seq_t *handle;
static int seqfd, sockfd, netfd[MAX_CONNECTION] = {[0 ... MAX_CONNECTION-1] = -1};
static int max_connection;
static int cur_connected;
static int seq_port;

static int server_mode;
static int verbose = 0;


/*
 * main routine
 */

static struct option long_option[] = {
	{"port", 1, NULL, 'p'},
	{"source", 1, NULL, 's'},
	{"dest", 1, NULL, 'd'},
	{"help", 0, NULL, 'h'},
	{"verbose", 0, NULL, 'v'},
	{NULL, 0, NULL, 0},
};

int main(int argc, char **argv)
{
	int c;
	int port = DEFAULT_PORT;
	char *source = NULL, *dest = NULL;

	while ((c = getopt_long(argc, argv, "p:s:d:v", long_option, NULL)) != -1) {
		switch (c) {
		case 'p':
			if (isdigit(*optarg))
				port = atoi(optarg);
			else
				port = get_port(optarg);
			break;
		case 's':
			source = optarg;
			break;
		case 'd':
			dest = optarg;
			break;
		case 'v':
			verbose++;
			break;
		default:
			usage();
			exit(1);
		}
	}

	signal(SIGINT, sigterm_exit);
	signal(SIGTERM, sigterm_exit);

	init_buf();
	init_seq(source, dest);

	if (optind >= argc) {
		server_mode = 1;
		max_connection = MAX_CONNECTION;
		init_server(port);
	} else {
		server_mode = 0;
		max_connection = 1;
		init_client(argv[optind], port);
	}

	do_loop();

	close_files();

	return 0;
}


/*
 * print usage
 */
static void usage(void)
{
	fprintf(stderr, "aseqnet - network client/server on ALSA sequencer\n");
	fprintf(stderr, "  Copyright (C) 1999 Takashi Iwai\n");
	fprintf(stderr, "usage:\n");
	fprintf(stderr, "  server mode: aseqnet [-options]\n");
	fprintf(stderr, "  client mode: aseqnet [-options] server_host\n");
	fprintf(stderr, "options:\n");
	fprintf(stderr, "  -p,--port # : sepcify TCP port (digit or service name)\n");
	fprintf(stderr, "  -s,--source addr : read from given addr (client:port)\n");
	fprintf(stderr, "  -d,--dest addr : write to given addr (client:port)\n");
	fprintf(stderr, "  -v, --verbose : print verbose messages\n");
}


/*
 * allocate and initialize buffers
 */
static void init_buf(void)
{
	max_wrlen = MAX_BUF_EVENTS * sizeof(snd_seq_event_t);
	max_rdlen = MAX_BUF_EVENTS * sizeof(snd_seq_event_t);
	writebuf = malloc(max_wrlen);
	readbuf = malloc(max_rdlen);
	if (writebuf == NULL || readbuf == NULL) {
		fprintf(stderr, "can't malloc\n");
		exit(1);
	}
	memset(writebuf, 0, max_wrlen);
	memset(readbuf, 0, max_rdlen);
	cur_wrlen = 0;
}

/*
 * parse command line to client:port
 * NB: the given string will be broken.
 */
static int parse_address(snd_seq_t *seq, snd_seq_addr_t *addr, char *arg)
{
	char *p;
	int client, port;

	if ((p = strpbrk(arg, ":.")) == NULL)
		return -1;
	if ((port = atoi(p + 1)) < 0)
		return -1;
	addr->port = port;
	if (isdigit(*arg)) {
		client = atoi(arg);
		if (client < 0)
			return -1;
		addr->client = client;
	} else {
		/* convert from the name */
		snd_seq_client_info_t cinfo;
		int len;

		*p = 0;
		len = strlen(arg);
		if (len <= 0)
			return -1;
		cinfo.client = -1;
		while (snd_seq_query_next_client(seq, &cinfo) >= 0) {
			if (! strncmp(cinfo.name, arg, len)) {
				addr->client = cinfo.client;
				return 0;
			}
		}
		return -1; /* not found */
	}
	return 0;
}


/*
 * close all files
 */
static void close_files(void)
{
	int i;
	if (verbose)
		fprintf(stderr, "closing files..\n");
	for (i = 0; i < max_connection; i++) {
		if (netfd[i] >= 0)
			close(netfd[i]);
	}
	if (sockfd >= 0)
		close(sockfd);
}


/*
 * initialize sequencer
 */
static void init_seq(char *source, char *dest)
{
	snd_seq_addr_t addr;

	if (snd_seq_open(&handle, SND_SEQ_OPEN) < 0) {
		perror("snd_seq_open");
		exit(1);
	}
	seqfd = snd_seq_poll_descriptor(handle);
	snd_seq_block_mode(handle, 0);

	/* set client info */
	if (server_mode)
		snd_seq_set_client_name(handle, "Net Server");
	else
		snd_seq_set_client_name(handle, "Net Client");

	/* create a port */
	seq_port = snd_seq_create_simple_port(handle, "Network",
					      SND_SEQ_PORT_CAP_READ |
					      SND_SEQ_PORT_CAP_WRITE |
					      SND_SEQ_PORT_CAP_SUBS_READ |
					      SND_SEQ_PORT_CAP_SUBS_WRITE,
					      SND_SEQ_PORT_TYPE_MIDI_GENERIC);
	if (seq_port < 0) {
		perror("create seq port");
		exit(1);
	}
	if (verbose)
		fprintf(stderr, "sequencer opened: %d:%d\n",
			snd_seq_client_id(handle), seq_port);

	/* explicit subscriptions */
	if (source) {
		/* read subscription */
		if (parse_address(handle, &addr, source) < 0) {
			fprintf(stderr, "invalid source address %s\n", source);
			exit(1);
		}
		if (snd_seq_connect_from(handle, seq_port, addr.client, addr.port)) {
			perror("read subscription");
			exit(1);
		}
	}
	if (dest) {
		/* write subscription */
		if (parse_address(handle, &addr, dest) < 0) {
			fprintf(stderr, "invalid destination address %s\n", dest);
			exit(1);
		}
		if (snd_seq_connect_to(handle, seq_port, addr.client, addr.port)) {
			perror("write subscription");
			exit(1);
		}
	}
}


/*
 * convert from string to TCP port number
 */
static int get_port(char *service)
{
	struct servent *sp;

	if ((sp = getservbyname(service, "tcp")) == NULL){
		fprintf(stderr, "service '%s' is not found in /etc/services\n", service);
		return -1;
	}
	return sp->s_port;
}

/*
 * signal handler
 */
static void sigterm_exit(int sig)
{
	close_files();
	exit(1);
}


/*
 * initialize network server
 */
static void init_server(int port)
{
	int i;
	int curstate = 1;
	struct sockaddr_in addr;

	memset(&addr, 0, sizeof(addr));

	addr.sin_family = AF_INET;
	addr.sin_addr.s_addr = INADDR_ANY;
	addr.sin_port = htons(port);

	sockfd = socket(AF_INET, SOCK_STREAM, 0);
	if (sockfd < 0)  {
		perror("create socket");
		exit(1);
	}
	setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &curstate, sizeof(curstate));
	/* the return value is ignored.. */

	if (bind(sockfd, &addr, sizeof(addr)) < 0)  {
		perror("can't bind");
		exit(1);
	}

	if (listen(sockfd, 5) < 0)  {
		perror("can't listen");
		exit(1);
	}

	cur_connected = 0;
	for (i = 0; i < max_connection; i++)
		netfd[i] = -1;
}

/*
 * start connection on server
 */
static void start_connection(void)
{
	struct sockaddr_in addr;
	int i;
	int addr_len;

	for (i = 0; i < max_connection; i++) {
		if (netfd[i] < 0)
			break;
	}
	if (i >= max_connection) {
		fprintf(stderr, "too many connections!\n");
		exit(1);
	}
	memset(&addr, 0, sizeof(addr));
	addr_len = sizeof(addr);
	netfd[i] = accept(sockfd, (struct sockaddr *)&addr, &addr_len);
	if (netfd[i] < 0) {
		perror("accept");
		exit(1);
	}
	if (verbose)
		fprintf(stderr, "accepted[%d]\n", netfd[i]);
	cur_connected++;
}

/*
 * initialize network client
 */
static void init_client(char *server, int port)
{
	struct sockaddr_in addr;
	struct hostent *host;
	int curstate = 1;
	int fd;

	if ((fd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP)) < 0){
		perror("create socket");
		exit(1);
	}
	if (setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &curstate, sizeof(curstate)) < 0) {
		perror("setsockopt");
		exit(1);
	}
	if ((host = gethostbyname(server)) == NULL){
		fprintf(stderr,"can't get address %s\n", server);
		exit(1);
	}
	addr.sin_port = htons(port);
	addr.sin_family = AF_INET;
	memcpy(&addr.sin_addr, host->h_addr, host->h_length);
	if (connect(fd, &addr, sizeof(addr)) < 0) {
		perror("connect");
		exit(1);
	}
	if (verbose)
		fprintf(stderr, "ok.. connected\n");
	netfd[0] = fd;
	cur_connected = 1;
}

/*
 * set file descriptor
 */
static void set_fd(int fd, fd_set *p, int *width)
{
	FD_SET(fd, p);
	if (fd >= *width)
		*width = fd + 1;
}

/*
 * event loop
 */
static void do_loop(void)
{
	fd_set rfd;
	int i, rc, width;

	for (;;) {
		FD_ZERO(&rfd);
		width = 0;
		set_fd(seqfd, &rfd, &width);
		if (server_mode)
			set_fd(sockfd, &rfd, &width);
		for (i = 0; i < max_connection; i++) {
			if (netfd[i] >= 0)
				set_fd(netfd[i], &rfd, &width);
		}
		rc = select(width, &rfd, NULL, NULL, NULL);
		if (rc <= 0)
			exit(1);
		if (server_mode) {
			if (FD_ISSET(sockfd, &rfd))
				start_connection();
		}
		if (FD_ISSET(seqfd, &rfd)) {
			if (copy_local_to_remote())
				break;
		}
		for (i = 0; i < max_connection; i++) {
			if (netfd[i] < 0)
				continue;
			if (FD_ISSET(netfd[i], &rfd)) {
				if (copy_remote_to_local(netfd[i])) {
					netfd[i] = -1;
					cur_connected--;
					if (cur_connected <= 0)
						return;
				}
			}
		}
	}
}


/*
 * flush write buffer - send data to the socket
 */
static void flush_writebuf(void)
{
	if (cur_wrlen) {
		int i;
		for (i = 0; i < max_connection; i++) {
			if (netfd[i] >= 0)
				write(netfd[i], writebuf, cur_wrlen);
		}
		cur_wrlen = 0;
	}
}

/*
 * get space from write buffer
 */
static char *get_writebuf(int len)
{
	char *buf;
	if (cur_wrlen + len >= max_wrlen)
		flush_writebuf();
	buf = writebuf + cur_wrlen;
	cur_wrlen += len;
	return buf;
}

/*
 * copy events from sequencer to port(s)
 */
static int copy_local_to_remote(void)
{
	int rc;
	snd_seq_event_t *ev;
	char *buf;

	while ((rc = snd_seq_event_input(handle, &ev)) >= 0 && ev) {
		if (ev->type >= SND_SEQ_EVENT_CLIENT_START &&
		    ! snd_seq_ev_is_variable_type(ev)) {
			snd_seq_free_event(ev);
			continue;
		}
		if (snd_seq_ev_is_variable(ev)) {
			int len;
			len = sizeof(snd_seq_event_t) + ev->data.ext.len;
			buf = get_writebuf(len);
			memcpy(buf, ev, sizeof(snd_seq_event_t));
			memcpy(buf + sizeof(snd_seq_event_t), ev->data.ext.ptr, ev->data.ext.len);
		} else {
			buf = get_writebuf(sizeof(snd_seq_event_t));
			memcpy(buf, ev, sizeof(snd_seq_event_t));
		}
		snd_seq_free_event(ev);
	}
	flush_writebuf();
	return 0;
}

/*
 * copy events from a port to sequencer
 */
static int copy_remote_to_local(int fd)
{
	int count;
	char *buf;
	snd_seq_event_t *ev;

	count = read(fd, readbuf, MAX_BUF_EVENTS * sizeof(snd_seq_event_t));
	buf = readbuf;

	if (count == 0) {
		if (verbose)
			fprintf(stderr, "disconnected\n");
		return 1;
	}

	while (count > 0) {
		ev = (snd_seq_event_t*)buf;
		buf += sizeof(snd_seq_event_t);
		count -= sizeof(snd_seq_event_t);
		if (snd_seq_ev_is_variable(ev) && ev->data.ext.len > 0) {
			ev->data.ext.ptr = buf;
			buf += ev->data.ext.len;
			count -= ev->data.ext.len;
		}
		snd_seq_ev_set_direct(ev);
		snd_seq_ev_set_source(ev, seq_port);
		snd_seq_ev_set_subs(ev);
		snd_seq_event_output(handle, ev);
	}

	snd_seq_flush_output(handle);
	return 0;
}

