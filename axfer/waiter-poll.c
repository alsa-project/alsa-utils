// SPDX-License-Identifier: GPL-2.0
//
// waiter-poll.c - Waiter for event notification by poll(2).
//
// Copyright (c) 2018 Takashi Sakamoto <o-takashi@sakamocchi.jp>
//
// Licensed under the terms of the GNU General Public License, version 2.

#include "waiter.h"
#include "misc.h"

#include <stdlib.h>
#include <errno.h>
#include <poll.h>

static int poll_prepare(struct waiter_context *waiter)
{
	// Nothing to do because an instance of waiter has required data.
	return 0;
}

static int poll_wait_event(struct waiter_context *waiter, int timeout_msec)
{
	int err;

	err = poll(waiter->pfds, waiter->pfd_count, timeout_msec);
	if (err < 0)
		return -errno;

	return err;
}

static void poll_release(struct waiter_context *waiter)
{
	// Nothing to do because an instance of waiter has required data.
	return;
}

const struct waiter_data waiter_poll = {
	.ops = {
		.prepare	= poll_prepare,
		.wait_event	= poll_wait_event,
		.release	= poll_release,
	},
};
