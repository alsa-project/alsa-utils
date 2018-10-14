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
#include <alsa/asoundlib.h>

#define MAX_CARDS	256

struct snd_card_iterator {
        int card;
        char name[16];
};

void snd_card_iterator_init(struct snd_card_iterator *iter)
{
        iter->card = -1;
        memset(iter->name, 0, sizeof(iter->name));
}

static const char *snd_card_iterator_next(struct snd_card_iterator *iter)
{
        if (snd_card_next(&iter->card) < 0)
                return NULL;
        if (iter->card < 0)
                return NULL;
	if (iter->card >= MAX_CARDS) {
		fprintf(stderr, "alsactl: too many cards\n");
		return NULL;
	}


        snprintf(iter->name, sizeof(iter->name), "hw:%d", iter->card);

        return (const char *)iter->name;
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

static int print_event(int card, snd_ctl_t *ctl)
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

	if (card >= 0)
		printf("card %d, ", card);
	printf("#%d (%i,%i,%i,%s,%i)",
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
			      snd_ctl_t *ctl)
{
	struct pollfd *pfds;
	int count;
	unsigned int pfd_count;
	int i;
	int err;

	count = snd_ctl_poll_descriptors_count(ctl);
	if (count < 0)
		return count;
	if (count == 0)
		return -ENXIO;
	pfd_count = count;

	pfds = calloc(pfd_count, sizeof(*pfds));
	if (!pfds)
		return -ENOMEM;

	count = snd_ctl_poll_descriptors(ctl, pfds, pfd_count);
	if (count < 0) {
		err = count;
		goto end;
	}
	if (count != pfd_count) {
		err = -EIO;
		goto end;
	}

	for (i = 0; i < pfd_count; ++i) {
		err = epoll_ctl(epfd, op, pfds[i].fd, epev);
		if (err < 0)
			break;
	}
end:
	free(pfds);
	return err;
}

static int prepare_dispatcher(int epfd, snd_ctl_t **ctls, int ncards)
{
	int i;
	int err = 0;

	for (i = 0; i < ncards; ++i) {
		snd_ctl_t *ctl = ctls[i];
		struct epoll_event ev;
		ev.events = EPOLLIN;
		ev.data.ptr = (void *)ctl;
		err = operate_dispatcher(epfd, EPOLL_CTL_ADD, &ev, ctl);
		if (err < 0)
			break;
	}

	return err;
}

static int run_dispatcher(int epfd, unsigned int max_ev_count, int show_cards)
{
	struct epoll_event *epev;
	int err = 0;

	epev = calloc(max_ev_count, sizeof(*epev));
	if (!epev)
		return -ENOMEM;

	while (true) {
		int count;
		int i;

		count = epoll_wait(epfd, epev, max_ev_count, 200);
		if (count < 0) {
			err = count;
			break;
		}
		if (count == 0)
			continue;

		for (i = 0; i < count; ++i) {
			struct epoll_event *ev = epev + i;
			snd_ctl_t *handle = (snd_ctl_t *)ev->data.ptr;

			if (ev->events & EPOLLIN)
				print_event(show_cards ? i : -1, handle);
		}
	}

	free(epev);

	return err;
}

static void clear_dispatcher(int epfd, snd_ctl_t **ctls, int ncards)
{
	int i;

	for (i = 0; i < ncards; ++i) {
		snd_ctl_t *ctl = ctls[i];
		operate_dispatcher(epfd, EPOLL_CTL_DEL, NULL, ctl);
	}
}

int monitor(const char *name)
{
	snd_ctl_t *ctls[MAX_CARDS] = {0};
	int ncards = 0;
	int show_cards;
	int epfd;
	int i;
	int err = 0;

	epfd = epoll_create(1);
	if (epfd < 0)
		return -errno;

	if (!name) {
		struct snd_card_iterator iter;
		const char *cardname;

		snd_card_iterator_init(&iter);
		while ((cardname = snd_card_iterator_next(&iter))) {
			err = open_ctl(cardname, &ctls[ncards]);
			if (err < 0)
				goto error;
			ncards++;
		}
		show_cards = 1;
	} else {
		err = open_ctl(name, &ctls[0]);
		if (err < 0)
			goto error;
		ncards++;
		show_cards = 0;
	}

	err = prepare_dispatcher(epfd, ctls, ncards);
	if (err >= 0)
		err = run_dispatcher(epfd, ncards, show_cards);
	clear_dispatcher(epfd, ctls, ncards);

error:
	for (i = 0; i < ncards; i++) {
		if (ctls[i])
			snd_ctl_close(ctls[i]);
	}

	close(epfd);

	return err;
}
