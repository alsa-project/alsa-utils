// SPDX-License-Identifier: GPL-2.0
//
// waiter-select.c - Waiter for event notification by select(2).
//
// Copyright (c) 2018 Takashi Sakamoto <o-takashi@sakamocchi.jp>
//
// Licensed under the terms of the GNU General Public License, version 2.

#include "waiter.h"

#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <sys/select.h>

// Except for POLLERR.
#ifdef POLLRDNORM
// This program is for userspace compliant to POSIX 2008 (IEEE 1003.1:2008).
// This is the default compliance level since glibc-2.12 or later.
# define POLLIN_SET	(POLLRDNORM | POLLRDBAND | POLLIN | POLLHUP)
# define POLLOUT_SET	(POLLWRBAND | POLLWRNORM | POLLOUT)
#else
// However it's allowed to be for old compliance levels.
# define POLLIN_SET	(POLLIN | POLLHUP)
# define POLLOUT_SET	(POLLOUT)
#endif
#define POLLEX_SET	(POLLPRI)


struct select_state {
	fd_set rfds_rd;
	fd_set rfds_wr;
	fd_set rfds_ex;
};

static int select_prepare(struct waiter_context *waiter)
{
	return 0;
}

static int select_wait_event(struct waiter_context *waiter, int timeout_msec)
{
	struct select_state *state = waiter->private_data;
	struct pollfd *pfd;
	int fd_max;
	struct timeval tv, *tv_ptr;
	int i;
	int err;

	FD_ZERO(&state->rfds_rd);
	FD_ZERO(&state->rfds_wr);
	FD_ZERO(&state->rfds_ex);

	fd_max = 0;
	for (i = 0; i < waiter->pfd_count; ++i) {
		pfd = &waiter->pfds[i];

		if (pfd->events & POLLIN_SET)
			FD_SET(pfd->fd, &state->rfds_rd);
		if (pfd->events & POLLOUT_SET)
			FD_SET(pfd->fd, &state->rfds_wr);
		if (pfd->events & POLLEX_SET)
			FD_SET(pfd->fd, &state->rfds_ex);
		if (pfd->fd > fd_max)
			fd_max = pfd->fd;
	}

	if (timeout_msec < 0) {
		tv_ptr = NULL;
	} else {
		tv.tv_sec = 0;
		tv.tv_usec = timeout_msec * 1000;
		tv_ptr = &tv;
	}

	err = select(fd_max + 1, &state->rfds_rd, &state->rfds_wr,
		     &state->rfds_ex, tv_ptr);
	if (err < 0)
		return -errno;

	for (i = 0; i < waiter->pfd_count; ++i) {
		pfd = &waiter->pfds[i];

		pfd->revents = 0;
		if (FD_ISSET(pfd->fd, &state->rfds_rd))
			pfd->revents |= POLLIN;
		if (FD_ISSET(pfd->fd, &state->rfds_wr))
			pfd->revents |= POLLOUT;
		if (FD_ISSET(pfd->fd, &state->rfds_ex))
			pfd->revents |= POLLHUP;
	}

	return err;
}

static void select_release(struct waiter_context *waiter)
{
	return;
}

const struct waiter_data waiter_select = {
	.ops = {
		.prepare	= select_prepare,
		.wait_event	= select_wait_event,
		.release	= select_release,
	},
	.private_size = sizeof(struct select_state),
};
