/*
 *  Advanced Linux Sound Architecture Control Program
 *  Copyright (c) 1997 by Perex, APS, University of South Bohemia
 *
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
 *   Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 */

#include "alsactl.h"
#include "aconfig.h"
#include "version.h"
#include <getopt.h>
#include <stdarg.h>

#define HELPID_HELP             1000
#define HELPID_FILE             1001
#define HELPID_DEBUG            1002
#define HELPID_VERSION		1003

extern int yyparse(void);
extern int linecount;
extern FILE *yyin;
extern int yydebug;

int debugflag = 0;
char cfgfile[512] = ALSACTL_FILE;

void error(const char *fmt,...)
{
	va_list va;

	va_start(va, fmt);
	fprintf(stderr, "alsactl: ");
	vfprintf(stderr, fmt, va);
	fprintf(stderr, "\n");
	va_end(va);
}

static void help(void)
{
	printf("Usage: alsactl <options> command\n");
	printf("\nAvailable options:\n");
	printf("  -h,--help       this help\n");
	printf("  -f,--file #     configuration file (default " ALSACTL_FILE ")\n");
	printf("  -d,--debug      debug mode\n");
	printf("  -v,--version    print version of this program\n");
	printf("\nAvailable commands:\n");
	printf("  store <card #>  store current driver setup for one or each soundcards\n");
	printf("                  to configuration file\n");
	printf("  restore <card #>  restore current driver setup for one or each soundcards\n");
	printf("                    from configuration file\n");
}

static int store_setup(const char *cardname)
{
	int err;

	if (!cardname) {
		unsigned int card_mask, idx;

		card_mask = snd_cards_mask();
		if (!card_mask) {
			error("No soundcards found...");
			return 1;
		}
		soundcard_setup_init();
		for (idx = 0; idx < 32; idx++) {
			if (card_mask & (1 << idx)) {	/* find each installed soundcards */
				if ((err = soundcard_setup_collect_switches(idx))) {
					soundcard_setup_done();
					return err;
				}
				if ((err = soundcard_setup_collect_data(idx))) {
					soundcard_setup_done();
					return err;
				}
			}
		}
		err = soundcard_setup_write(cfgfile, -1);
		soundcard_setup_done();
	} else {
		int cardno;

		cardno = snd_card_name(cardname);
		if (cardno) {
			error("Cannot find soundcard '%s'...", cardname);
			return 1;
		}
		if ((err = soundcard_setup_collect_switches(cardno))) {
			soundcard_setup_done();
			return err;
		}
		if ((err = soundcard_setup_collect_data(cardno))) {
			soundcard_setup_done();
			return err;
		}
		err = soundcard_setup_write(cfgfile, cardno);
		soundcard_setup_done();
	}
	return err;
}

static int restore_setup(const char *cardname)
{
	int err, cardno = -1;

	if (cardname) {
		cardno = snd_card_name(cardname);
		if (cardno < 0) {
			error("Cannot find soundcard '%s'...", cardname);
			return 1;
		}
	}
	if ((err = soundcard_setup_load(cfgfile, 0)))
		return err;
	if ((err = soundcard_setup_collect_switches(cardno))) {
		soundcard_setup_done();
		return err;
	}
	if ((err = soundcard_setup_merge_switches(cardno))) {
		soundcard_setup_done();
		return err;
	}
	if ((err = soundcard_setup_process_switches(cardno))) {
		soundcard_setup_done();
		return err;
	}
	if ((err = soundcard_setup_collect_data(cardno))) {
		soundcard_setup_done();
		return err;
	}
	if ((err = soundcard_setup_merge_data(cardno))) {
		soundcard_setup_done();
		return err;
	}
	if ((err = soundcard_setup_process_data(cardno))) {
		soundcard_setup_done();
		return err;
	}
	soundcard_setup_done();
	return err;
}

int main(int argc, char *argv[])
{
	int morehelp;
	struct option long_option[] =
	{
		{"help", 0, NULL, HELPID_HELP},
		{"file", 1, NULL, HELPID_FILE},
		{"debug", 0, NULL, HELPID_DEBUG},
		{"version", 0, NULL, HELPID_VERSION},
		{NULL, 0, NULL, 0},
	};

	morehelp = 0;
	while (1) {
		int c;

		if ((c = getopt_long(argc, argv, "hf:dv", long_option, NULL)) < 0)
			break;
		switch (c) {
		case 'h':
		case HELPID_HELP:
			morehelp++;
			break;
		case 'f':
		case HELPID_FILE:
			strncpy(cfgfile, optarg, sizeof(cfgfile) - 1);
			cfgfile[sizeof(cfgfile) - 1] = 0;
			break;
		case 'd':
		case HELPID_DEBUG:
			debugflag = 1;
			break;
		case 'v':
		case HELPID_VERSION:
			printf("alsactl version " SND_UTIL_VERSION_STR "\n");
			return 1;
		default:
			fprintf(stderr, "\07Invalid switch or option needs an argument.\n");
			morehelp++;
		}
	}
	if (morehelp) {
		help();
		return 1;
	}
	if (argc - optind <= 0) {
		fprintf(stderr, "alsactl: Specify command...\n");
		return 0;
	}
	if (!strcmp(argv[optind], "store")) {
		return store_setup(argc - optind > 1 ? argv[optind + 1] : NULL) ?
		    1 : 0;
	} else if (!strcmp(argv[optind], "restore")) {
		return restore_setup(argc - optind > 1 ? argv[optind + 1] : NULL) ?
		    1 : 0;
	} else {
		fprintf(stderr, "alsactl: Unknown command '%s'...\n", argv[optind]);
	}

	return 0;
}
