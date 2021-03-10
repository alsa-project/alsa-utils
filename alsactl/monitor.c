/*
 *  Advanced Linux Sound Architecture Control Program
 *  Copyright (c) by Takashi Iwai <tiwai@suse.de>
 *
 *   This program is free software; you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation; either version 2 of the License, or
 *   (at your option) any later version.
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU General Public License for more details.
 *
 *   You should have received a copy of the GNU General Public License
 *   along with this program; if not, write to the Free Software
 *   Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 */

#include "aconfig.h"
#include "version.h"
#include <stdio.h>
#include <stdbool.h>
#include <string.h>
#include <sys/epoll.h>
#include <sys/inotify.h>
#include <limits.h>
#include <time.h>
#include <signal.h>
#include <sys/signalfd.h>

#include <stddef.h>
#include "list.h"

#include "alsactl.h"

struct src_entry {
	snd_ctl_t *handle;
	char *name;
	unsigned int pfd_count;
	struct list_head list;
};

static void remove_source_entry(struct src_entry *entry)
{
	list_del(&entry->list);
	if (entry->handle)
		snd_ctl_close(entry->handle);
	free(entry->name);
	free(entry);
}

static void clear_source_list(struct list_head *srcs)
{
	struct src_entry *entry, *tmp;

	list_for_each_entry_safe(entry, tmp, srcs, list)
		remove_source_entry(entry);
}

static int insert_source_entry(struct list_head *srcs, snd_ctl_t *handle,
			       const char *name)
{
	struct src_entry *entry;
	int count;
	int err;

	entry = calloc(1, sizeof(*entry));
	if (!entry)
		return -ENOMEM;
	INIT_LIST_HEAD(&entry->list);
	entry->handle = handle;

	entry->name = strdup(name);
	if (!entry->name) {
		err = -ENOMEM;
		goto error;
	}

	count = snd_ctl_poll_descriptors_count(handle);
	if (count < 0) {
		err = count;
		goto error;
	}
	if (count == 0) {
		err = -ENXIO;
		goto error;
	}
	entry->pfd_count = count;

	list_add_tail(&entry->list, srcs);

	return 0;
error:
	remove_source_entry(entry);
	return err;
}

static int open_ctl(const char *name, snd_ctl_t **ctlp)
{
	snd_ctl_t *ctl;
	int err;

	err = snd_ctl_open(&ctl, name, SND_CTL_READONLY);
	if (err < 0) {
		fprintf(stderr, "Cannot open ctl %s\n", name);
		return err;
	}
	err = snd_ctl_subscribe_events(ctl, 1);
	if (err < 0) {
		fprintf(stderr, "Cannot open subscribe events to ctl %s\n", name);
		snd_ctl_close(ctl);
		return err;
	}
	*ctlp = ctl;
	return 0;
}

static inline bool seek_entry_by_name(struct list_head *srcs, const char *name)
{
	struct src_entry *entry;

	list_for_each_entry(entry, srcs, list) {
		if (!strcmp(entry->name, name))
			return true;
	}

	return false;
}

static int prepare_source_entry(struct list_head *srcs, const char *name)
{
	snd_ctl_t *handle;
	int err;

	if (!name) {
		struct snd_card_iterator iter;
		const char *cardname;

		snd_card_iterator_init(&iter, -1);
		while ((cardname = snd_card_iterator_next(&iter))) {
			if (seek_entry_by_name(srcs, cardname))
				continue;
			err = open_ctl(cardname, &handle);
			if (err < 0)
				return err;
			err = insert_source_entry(srcs, handle, cardname);
			if (err < 0)
				return err;
		}
	} else {
		if (seek_entry_by_name(srcs, name))
			return 0;
		err = open_ctl(name, &handle);
		if (err < 0)
			return err;
		err = insert_source_entry(srcs, handle, name);
		if (err < 0)
			return err;
	}

	return 0;
}

static int check_control_cdev(int infd, bool *retry)
{
	struct inotify_event *ev;
	char *buf;
	int err = 0;

	buf = calloc(1, sizeof(*ev) + NAME_MAX);
	if (!buf)
		return -ENOMEM;

	while (1) {
		ssize_t len = read(infd, buf, sizeof(*ev) + NAME_MAX);
		if (len < 0) {
			if (errno != EAGAIN)
				err = errno;
			break;
		} else if (len == 0) {
			break;
		}

		size_t pos = 0;
		while (pos < len) {
			ev = (struct inotify_event *)(buf + pos);
			if ((ev->mask & IN_CREATE) &&
			    strstr(ev->name, "controlC") == ev->name)
				*retry = true;
			pos += sizeof(*ev) + ev->len;
		}
	}

	free(buf);

	return err;
}

static int print_event(snd_ctl_t *ctl, const char *name)
{
	snd_ctl_event_t *event;
	unsigned int mask;
	int err;

	snd_ctl_event_alloca(&event);
	err = snd_ctl_read(ctl, event);
	if (err < 0)
		return err;

	if (snd_ctl_event_get_type(event) != SND_CTL_EVENT_ELEM)
		return 0;

	printf("node %s, #%d (%i,%i,%i,%s,%i)",
	       name,
	       snd_ctl_event_elem_get_numid(event),
	       snd_ctl_event_elem_get_interface(event),
	       snd_ctl_event_elem_get_device(event),
	       snd_ctl_event_elem_get_subdevice(event),
	       snd_ctl_event_elem_get_name(event),
	       snd_ctl_event_elem_get_index(event));

	mask = snd_ctl_event_elem_get_mask(event);
	if (mask == SND_CTL_EVENT_MASK_REMOVE) {
		printf(" REMOVE\n");
		return 0;
	}

	if (mask & SND_CTL_EVENT_MASK_VALUE)
		printf(" VALUE");
	if (mask & SND_CTL_EVENT_MASK_INFO)
		printf(" INFO");
	if (mask & SND_CTL_EVENT_MASK_ADD)
		printf(" ADD");
	if (mask & SND_CTL_EVENT_MASK_TLV)
		printf(" TLV");
	printf("\n");
	return 0;
}

static int operate_dispatcher(int epfd, uint32_t op, struct epoll_event *epev,
			      struct src_entry *entry)
{
	struct pollfd *pfds;
	int count;
	int i;
	int err = 0;

	pfds = calloc(entry->pfd_count, sizeof(*pfds));
	if (!pfds)
		return -ENOMEM;

	count = snd_ctl_poll_descriptors(entry->handle, pfds, entry->pfd_count);
	if (count < 0) {
		err = count;
		goto end;
	}
	if (count != entry->pfd_count) {
		err = -EIO;
		goto end;
	}

	for (i = 0; i < entry->pfd_count; ++i) {
		err = epoll_ctl(epfd, op, pfds[i].fd, epev);
		if (err < 0)
			break;
	}
end:
	free(pfds);
	return err;
}

static int prepare_dispatcher(int epfd, int sigfd, int infd,
			      struct list_head *srcs)
{
	struct epoll_event ev = {0};
	struct src_entry *entry;
	int err = 0;

	ev.events = EPOLLIN;
	ev.data.fd = sigfd;
	if (epoll_ctl(epfd, EPOLL_CTL_ADD, sigfd, &ev) < 0)
		return -errno;

	ev.events = EPOLLIN;
	ev.data.fd = infd;
	if (epoll_ctl(epfd, EPOLL_CTL_ADD, infd, &ev) < 0)
		return -errno;

	list_for_each_entry(entry, srcs, list) {
		ev.events = EPOLLIN;
		ev.data.ptr = (void *)entry;
		err = operate_dispatcher(epfd, EPOLL_CTL_ADD, &ev, entry);
		if (err < 0)
			break;
	}

	return err;
}

static int run_dispatcher(int epfd, int sigfd, int infd, struct list_head *srcs,
			  bool *retry)
{
	struct src_entry *entry;
	unsigned int max_ev_count;
	struct epoll_event *epev;
	int err = 0;

	max_ev_count = 0;
	list_for_each_entry(entry, srcs, list)
		max_ev_count += entry->pfd_count;

	epev = calloc(max_ev_count, sizeof(*epev));
	if (!epev)
		return -ENOMEM;

	while (true) {
		int count;
		int i;

		count = epoll_wait(epfd, epev, max_ev_count, -1);
		if (count < 0) {
			if (errno == EINTR)
				continue;
			err = count;
			break;
		}
		if (count == 0)
			continue;

		for (i = 0; i < count; ++i) {
			struct epoll_event *ev = epev + i;

			if (ev->data.fd == sigfd)
				goto end;

			if (ev->data.fd == infd) {
				err = check_control_cdev(infd, retry);
				if (err < 0 || *retry)
					goto end;
				continue;
			}

			entry = ev->data.ptr;
			if (ev->events & EPOLLIN)
				print_event(entry->handle, entry->name);
			if (ev->events & EPOLLERR) {
				operate_dispatcher(epfd, EPOLL_CTL_DEL, NULL, entry);
				remove_source_entry(entry);
			}
		}
	}
end:
	free(epev);
	return err;
}

static void clear_dispatcher(int epfd, int sigfd, int infd,
			     struct list_head *srcs)
{
	struct src_entry *entry;

	list_for_each_entry(entry, srcs, list)
		operate_dispatcher(epfd, EPOLL_CTL_DEL, NULL, entry);

	epoll_ctl(epfd, EPOLL_CTL_DEL, infd, NULL);

	epoll_ctl(epfd, EPOLL_CTL_DEL, sigfd, NULL);
}

static int prepare_signalfd(int *sigfd)
{
	sigset_t mask;
	int fd;

	sigemptyset(&mask);
	sigaddset(&mask, SIGINT);
	sigaddset(&mask, SIGTERM);

	if (sigprocmask(SIG_BLOCK, &mask, NULL) < 0)
		return -errno;

	fd = signalfd(-1, &mask, 0);
	if (fd < 0)
		return -errno;
	*sigfd = fd;

	return 0;
}

int monitor(const char *name)
{
	LIST_HEAD(srcs);
	int sigfd = 0;
	int epfd;
	int infd;
	int wd = 0;
	bool retry;
	int err = 0;

	err = prepare_signalfd(&sigfd);
	if (err < 0)
		return err;

	epfd = epoll_create(1);
	if (epfd < 0) {
		close(sigfd);
		return -errno;
	}

	infd = inotify_init1(IN_NONBLOCK);
	if (infd < 0) {
		err = -errno;
		goto error;
	}
	wd = inotify_add_watch(infd, "/dev/snd/", IN_CREATE);
	if (wd < 0) {
		err = -errno;
		goto error;
	}
retry:
	retry = false;
	err = prepare_source_entry(&srcs, name);
	if (err < 0)
		goto error;

	err = prepare_dispatcher(epfd, sigfd, infd, &srcs);
	if (err >= 0)
		err = run_dispatcher(epfd, sigfd, infd, &srcs, &retry);
	clear_dispatcher(epfd, sigfd, infd, &srcs);

	if (retry) {
		// A simple makeshift for timing gap between creation of nodes
		// by devtmpfs and chmod() by udevd.
		struct timespec req = { .tv_sec = 1 };
		nanosleep(&req, NULL);
		goto retry;
	}
error:
	clear_source_list(&srcs);

	if (wd > 0)
		inotify_rm_watch(infd, wd);
	close(infd);

	close(epfd);

	close(sigfd);

	return err;
}
