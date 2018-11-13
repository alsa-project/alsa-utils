// SPDX-License-Identifier: GPL-2.0
//
// waiter.c - I/O event waiter.
//
// Copyright (c) 2018 Takashi Sakamoto <o-takashi@sakamocchi.jp>
//
// Licensed under the terms of the GNU General Public License, version 2.

#include "waiter.h"

#include <stdlib.h>
#include <string.h>
#include <errno.h>

#include "misc.h"

static const char *const waiter_type_labels[] = {
	[WAITER_TYPE_DEFAULT] = "default",
	[WAITER_TYPE_POLL] = "poll",
	[WAITER_TYPE_SELECT] = "select",
	[WAITER_TYPE_EPOLL] = "epoll",
};

enum waiter_type waiter_type_from_label(const char *label)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(waiter_type_labels); ++i) {
		if (!strcmp(waiter_type_labels[i], label))
			return i;
	}

	return WAITER_TYPE_DEFAULT;
}

const char *waiter_label_from_type(enum waiter_type type)
{
	return waiter_type_labels[type];
}

int waiter_context_init(struct waiter_context *waiter,
			enum waiter_type type, unsigned int pfd_count)
{
	struct {
		enum waiter_type type;
		const struct waiter_data *waiter;
	} entries[] = {
		{WAITER_TYPE_POLL,	&waiter_poll},
		{WAITER_TYPE_SELECT,	&waiter_select},
		{WAITER_TYPE_EPOLL,	&waiter_epoll},
	};
	int i;

	if (pfd_count == 0)
		return -EINVAL;

	for (i = 0; i < ARRAY_SIZE(entries); ++i) {
		if (entries[i].type == type)
			break;
	}
	if (i == ARRAY_SIZE(entries))
		return -EINVAL;

	waiter->private_data = malloc(entries[i].waiter->private_size);
	if (waiter->private_data == NULL)
		return -ENOMEM;
	memset(waiter->private_data, 0, entries[i].waiter->private_size);

	waiter->type = type;
	waiter->ops = &entries[i].waiter->ops;

	waiter->pfds = calloc(pfd_count, sizeof(*waiter->pfds));
	if (waiter->pfds == NULL)
		return -ENOMEM;
	waiter->pfd_count = pfd_count;

	return 0;
}

int waiter_context_prepare(struct waiter_context *waiter)
{
	return waiter->ops->prepare(waiter);
}

int waiter_context_wait_event(struct waiter_context *waiter,
				int timeout_msec)
{
	return waiter->ops->wait_event(waiter, timeout_msec);
}

void waiter_context_release(struct waiter_context *waiter)
{
	waiter->ops->release(waiter);
}

void waiter_context_destroy(struct waiter_context *waiter)
{
	if (waiter->pfds)
		free(waiter->pfds);
	waiter->pfd_count = 0;
	if (waiter->private_data)
		free(waiter->private_data);
	waiter->private_data = NULL;
}
