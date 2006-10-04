/*
 *  Advanced Linux Sound Architecture Control Program
 *  Copyright (c) by Abramo Bagnara <abramo@alsa-project.org>
 *                   Jaroslav Kysela <perex@suse.cz>
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
 *   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307 USA
 *
 */

#include "aconfig.h"
#include "version.h"
#include <getopt.h>
#include <stdarg.h>
#include <stdio.h>
#include <assert.h>
#include <errno.h>
#include <alsa/asoundlib.h>
#include "alsactl.h"

#define SYS_ASOUNDRC "/etc/asound.state"
#define SYS_ASOUNDNAMES "/etc/asound.names"

int debugflag = 0;
int force_restore = 0;
char *command;

static void help(void)
{
	printf("Usage: alsactl <options> command\n");
	printf("\nAvailable options:\n");
	printf("  -h,--help       this help\n");
	printf("  -f,--file #     configuration file (default " SYS_ASOUNDRC " or " SYS_ASOUNDNAMES ")\n");
	printf("  -F,--force      try to restore the matching controls as much as possible\n");
	printf("  -d,--debug      debug mode\n");
	printf("  -v,--version    print version of this program\n");
	printf("\nAvailable commands:\n");
	printf("  store <card #>  save current driver setup for one or each soundcards\n");
	printf("                  to configuration file\n");
	printf("  restore<card #> load current driver setup for one or each soundcards\n");
	printf("                  from configuration file\n");
}

int main(int argc, char *argv[])
{
	struct option long_option[] =
	{
		{"help", 0, NULL, 'h'},
		{"file", 1, NULL, 'f'},
		{"force", 0, NULL, 'F'},
		{"debug", 0, NULL, 'd'},
		{"version", 0, NULL, 'v'},
		{NULL, 0, NULL, 0},
	};
	char *cfgfile = SYS_ASOUNDRC;
	int res;

	command = argv[0];
	while (1) {
		int c;

		if ((c = getopt_long(argc, argv, "hf:Fdv", long_option, NULL)) < 0)
			break;
		switch (c) {
		case 'h':
			help();
			return EXIT_SUCCESS;
		case 'f':
			cfgfile = optarg;
			break;
		case 'F':
			force_restore = 1;
			break;
		case 'd':
			debugflag = 1;
			break;
		case 'v':
			printf("alsactl version " SND_UTIL_VERSION_STR "\n");
			return EXIT_SUCCESS;
		case '?':		// error msg already printed
			help();
			return EXIT_FAILURE;
			break;
		default:		// should never happen
			fprintf(stderr, 
			"Invalid option '%c' (%d) not handled??\n", c, c);
		}
	}
	if (argc - optind <= 0) {
		fprintf(stderr, "alsactl: Specify command...\n");
		return 0;
	}

	if (!strcmp(argv[optind], "store")) {
		res =  save_state(cfgfile,
		   argc - optind > 1 ? argv[optind + 1] : NULL);
	} else if (!strcmp(argv[optind], "restore")) {
		res = load_state(cfgfile, 
		   argc - optind > 1 ? argv[optind + 1] : NULL);
	} else if (!strcmp(argv[optind], "names")) {
		if (!strcmp(cfgfile, SYS_ASOUNDRC))
			cfgfile = SYS_ASOUNDNAMES;
		res = generate_names(cfgfile);
	} else {
		fprintf(stderr, "alsactl: Unknown command '%s'...\n", 
			argv[optind]);
		res = -ENODEV;
	}

	return res < 0 ? res : 0;
}
