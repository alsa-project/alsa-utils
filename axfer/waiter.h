// SPDX-License-Identifier: GPL-2.0
//
// waiter.h - a header for I/O event waiter.
//
// Copyright (c) 2018 Takashi Sakamoto <o-takashi@sakamocchi.jp>
//
// Licensed under the terms of the GNU General Public License, version 2.

#ifndef __ALSA_UTILS_AXFER_WAITER__H_
#define __ALSA_UTILS_AXFER_WAITER__H_

#include <poll.h>

enum waiter_type {
	WAITER_TYPE_DEFAULT = 0,
	WAITER_TYPE_POLL,
	WAITER_TYPE_SELECT,
	WAITER_TYPE_EPOLL,
	WAITER_TYPE_COUNT,
};

struct waiter_ops;

struct waiter_context {
	enum waiter_type type;
	const struct waiter_ops *ops;
	void *private_data;

	struct pollfd *pfds;
	unsigned int pfd_count;
};

enum waiter_type waiter_type_from_label(const char *label);
const char *waiter_label_from_type(enum waiter_type type);

int waiter_context_init(struct waiter_context *waiter,
			enum waiter_type type, unsigned int pfd_count);
int waiter_context_prepare(struct waiter_context *waiter);
int waiter_context_wait_event(struct waiter_context *waiter,
				int timeout_msec);
void waiter_context_release(struct waiter_context *waiter);
void waiter_context_destroy(struct waiter_context *waiter);

// For internal use in 'waiter' module.

struct waiter_ops {
	int (*prepare)(struct waiter_context *waiter);
	int (*wait_event)(struct waiter_context *waiter, int timeout_msec);
	void (*release)(struct waiter_context *waiter);
};

struct waiter_data {
	struct waiter_ops ops;
	unsigned int private_size;
};

extern const struct waiter_data waiter_poll;
extern const struct waiter_data waiter_select;
extern const struct waiter_data waiter_epoll;

#endif
