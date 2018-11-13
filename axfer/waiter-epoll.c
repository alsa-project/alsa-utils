// SPDX-License-Identifier: GPL-2.0
//
// waiter-epoll.c - Waiter for event notification by epoll(7).
//
// Copyright (c) 2018 Takashi Sakamoto <o-takashi@sakamocchi.jp>
//
// Licensed under the terms of the GNU General Public License, version 2.

#include "waiter.h"
#include "misc.h"

#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/epoll.h>

struct epoll_state {
	int epfd;
	struct epoll_event *events;
	unsigned int ev_count;
};

static int epoll_prepare(struct waiter_context *waiter)
{
	struct epoll_state *state = waiter->private_data;
	int i;

	state->ev_count = waiter->pfd_count;
	state->events = calloc(state->ev_count, sizeof(*state->events));
	if (state->events == NULL)
		return -ENOMEM;

	state->epfd = epoll_create(1);
	if (state->epfd < 0)
		return -errno;

	for (i = 0; i < waiter->pfd_count; ++i) {
		struct epoll_event ev = {
			.data.fd = waiter->pfds[i].fd,
			.events = waiter->pfds[i].events,
		};
		if (epoll_ctl(state->epfd, EPOLL_CTL_ADD, ev.data.fd, &ev) < 0)
			return -errno;
	}

	return 0;
}

static int epoll_wait_event(struct waiter_context *waiter, int timeout_msec)
{
	struct epoll_state *state = waiter->private_data;
	unsigned int ev_count;
	int i, j;
	int err;

	memset(state->events, 0, state->ev_count * sizeof(*state->events));
	err = epoll_wait(state->epfd, state->events, state->ev_count,
			 timeout_msec);
	if (err < 0)
		return -errno;
	ev_count = (unsigned int)err;

	if (ev_count > 0) {
		// Reconstruct data of pollfd structure.
		for (i = 0; i < ev_count; ++i) {
			struct epoll_event *ev = &state->events[i];
			for (j = 0; j < waiter->pfd_count; ++j) {
				if (waiter->pfds[i].fd == ev->data.fd) {
					waiter->pfds[i].revents = ev->events;
					break;
				}
			}
		}
	}

	return ev_count;
}

static void epoll_release(struct waiter_context *waiter)
{
	struct epoll_state *state = waiter->private_data;
	int i;

	for (i = 0; i < waiter->pfd_count; ++i) {
		int fd = waiter->pfds[i].fd;
		epoll_ctl(state->epfd, EPOLL_CTL_DEL, fd, NULL);
	}

	free(state->events);
	state->events = NULL;

	close(state->epfd);

	state->ev_count = 0;
	state->epfd = 0;
}

const struct waiter_data waiter_epoll = {
	.ops = {
		.prepare	= epoll_prepare,
		.wait_event	= epoll_wait_event,
		.release	= epoll_release,
	},
	.private_size = sizeof(struct epoll_state),
};
