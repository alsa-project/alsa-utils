/*
 * alsamixer - curses mixer for the ALSA project
 * Copyright (c) 1998,1999 Tim Janik
 *                         Jaroslav Kysela <perex@perex.cz>
 * Copyright (c) 2009      Clemens Ladisch <clemens@ladisch.de>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "aconfig.h"
#include <stdio.h>
#include <stdlib.h>
#include <locale.h>
#include <getopt.h>
#include <alsa/asoundlib.h>
#include "gettext_curses.h"
#include "mixer_widget.h"
#include "mainloop.h"
#include "configparser.h"
#include "colors.h"

static int black_background = 0;
static int use_color = 1;
static int use_mouse = 1;
static const char* config_file = CONFIG_DEFAULT;
static struct snd_mixer_selem_regopt selem_regopt = {
	.ver = 1,
	.abstract = SND_MIXER_SABSTRACT_NONE,
	.device = "default",
};

static void show_help(void)
{
	puts(_("Usage: alsamixer [options]"));
	puts(_("Useful options:\n"
	       "  -h, --help              this help\n"
	       "  -c, --card=NUMBER       sound card number or id\n"
	       "  -D, --device=NAME       mixer device name\n"
	       "  -m, --mouse             enable mouse\n"
	       "  -M, --no-mouse          disable mouse\n"
	       "  -f, --config=FILE       configuration file\n"
	       "  -F, --no-config         do not load configuration file\n"
	       "  -V, --view=MODE         starting view mode: playback/capture/all\n"
	       "  -B, --black-background  use black background color"));
	puts(_("Debugging options:\n"
	       "  -g, --no-color          toggle using of colors\n"
	       "  -a, --abstraction=NAME  mixer abstraction level: none/basic"));
}

static void parse_options(int argc, char *argv[])
{
	static const char short_options[] = "hc:f:FD:mMV:Bga:";
	static const struct option long_options[] = {
		{ .name = "help", .val = 'h' },
		{ .name = "card", .has_arg = 1, .val = 'c' },
		{ .name = "config", .has_arg = 1, .val = 'f' },
		{ .name = "no-config", .val = 'F' },
		{ .name = "device", .has_arg = 1, .val = 'D' },
		{ .name = "mouse", .val = 'm' },
		{ .name = "no-mouse", .val = 'M' },
		{ .name = "view", .has_arg = 1, .val = 'V' },
		{ .name = "black-background", .val = 'B' },
		{ .name = "no-color", .val = 'g' },
		{ .name = "abstraction", .has_arg = 1, .val = 'a' },
		{ 0 }
	};
	int option;
	int card_index;
	static char name_buf[24];

	while ((option = getopt_long(argc, argv, short_options,
				     long_options, NULL)) != -1) {
		switch (option) {
		case '?':
		case 'h':
			show_help();
			exit(EXIT_SUCCESS);
		case 'c':
			card_index = snd_card_get_index(optarg);
			if (card_index < 0) {
				fprintf(stderr, _("invalid card index: %s\n"), optarg);
				goto fail;
			}
#if defined(SND_LIB_VER) && SND_LIB_VER(1, 2, 5) <= SND_LIB_VERSION
			sprintf(name_buf, "sysdefault:%d", card_index);
#else
			sprintf(name_buf, "hw:%d", card_index);
#endif
			selem_regopt.device = name_buf;
			break;
		case 'D':
			selem_regopt.device = optarg;
			break;
		case 'f':
			config_file = optarg;
			break;
		case 'F':
			config_file = NULL;
			break;
		case 'm':
			use_mouse = 1;
			break;
		case 'M':
			use_mouse = 0;
			break;
		case 'V':
			if (*optarg == 'p' || *optarg == 'P')
				view_mode = VIEW_MODE_PLAYBACK;
			else if (*optarg == 'c' || *optarg == 'C')
				view_mode = VIEW_MODE_CAPTURE;
			else
				view_mode = VIEW_MODE_ALL;
			break;
		case 'B':
			black_background = 1;
			break;
		case 'g':
			use_color = !use_color;
			break;
		case 'a':
			if (!strcmp(optarg, "none"))
				selem_regopt.abstract = SND_MIXER_SABSTRACT_NONE;
			else if (!strcmp(optarg, "basic"))
				selem_regopt.abstract = SND_MIXER_SABSTRACT_BASIC;
			else {
				fprintf(stderr, _("unknown abstraction level: %s\n"), optarg);
				goto fail;
			}
			break;
		default:
			fprintf(stderr, _("unknown option: %c\n"), option);
fail:
			fputs(_("try `alsamixer --help' for more information\n"), stderr);
			exit(EXIT_FAILURE);
		}
	}
}

int main(int argc, char *argv[])
{
	if (!isatty(fileno(stdin)))
		return 0;

	setlocale(LC_ALL, "");
#ifdef ENABLE_NLS_IN_CURSES
	textdomain(PACKAGE);
#endif

	parse_options(argc, argv);

	create_mixer_object(&selem_regopt);

	initialize_curses(use_color, use_mouse);

	if (config_file == CONFIG_DEFAULT)
		parse_default_config_file();
	else if (config_file)
		parse_config_file(config_file);

	if (black_background)
		reinit_colors(COLOR_BLACK);

	create_mixer_widget();

	mainloop();

	app_shutdown();
	return 0;
}
