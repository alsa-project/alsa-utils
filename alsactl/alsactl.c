/*
 *  Advanced Linux Sound Architecture Control Program
 *  Copyright (c) by Abramo Bagnara <abramo@alsa-project.org>
 *                   Jaroslav Kysela <perex@perex.cz>
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
#include <syslog.h>
#include <alsa/asoundlib.h>
#include "alsactl.h"

#ifndef SYS_ASOUNDRC
#define SYS_ASOUNDRC "/var/lib/alsa/asound.state"
#endif
#ifndef SYS_PIDFILE
#define SYS_PIDFILE "/var/run/alsactl.pid"
#endif

int debugflag = 0;
int force_restore = 1;
int ignore_nocards = 0;
int do_lock = 0;
int use_syslog = 0;
char *command;
char *statefile = NULL;

static void help(void)
{
	printf("Usage: alsactl <options> command\n");
	printf("\nAvailable global options:\n");
	printf("  -h,--help        this help\n");
	printf("  -d,--debug       debug mode\n");
	printf("  -v,--version     print version of this program\n");
	printf("\nAvailable state options:\n");
	printf("  -f,--file #      configuration file (default " SYS_ASOUNDRC ")\n");
	printf("  -l,--lock        use file locking to serialize concurrent access\n");
	printf("  -F,--force       try to restore the matching controls as much as possible\n");
	printf("                   (default mode)\n");
	printf("  -g,--ignore      ignore 'No soundcards found' error\n");
	printf("  -P,--pedantic    do not restore mismatching controls (old default)\n");
	printf("  -I,--no-init-fallback\n"
	       "                   don't initialize even if restore fails\n");
	printf("  -r,--runstate #  save restore and init state to this file (only errors)\n");
	printf("                   default settings is 'no file set'\n");
	printf("  -R,--remove      remove runstate file at first, otherwise append errors\n");
	printf("  -p,--period      store period in seconds for the daemon command\n");
	printf("  -e,--pid-file    pathname for the process id (daemon mode)\n");
	printf("\nAvailable init options:\n");
	printf("  -E,--env #=#	   set environment variable for init phase (NAME=VALUE)\n");
	printf("  -i,--initfile #  main configuation file for init phase (default " DATADIR "/init/00main)\n");
	printf("\n");
	printf("\nAvailable commands:\n");
	printf("  store    <card #> save current driver setup for one or each soundcards\n");
	printf("                    to configuration file\n");
	printf("  restore  <card #> load current driver setup for one or each soundcards\n");
	printf("                    from configuration file\n");
	printf("  nrestore <card #> like restore, but notify the daemon to rescan soundcards\n");
	printf("  init	   <card #> initialize driver to a default state\n");
	printf("  daemon   <card #> store state periodically for one or each soundcards\n");
	printf("  rdaemon  <card #> like daemon but do the state restore at first\n");
	printf("  kill     <cmd>    notify daemon to quit, rescan or save_and_quit\n");
}

int main(int argc, char *argv[])
{
	static const struct option long_option[] =
	{
		{"help", 0, NULL, 'h'},
		{"file", 1, NULL, 'f'},
		{"lock", 0, NULL, 'l'},
		{"env", 1, NULL, 'E'},
		{"initfile", 1, NULL, 'i'},
		{"no-init-fallback", 0, NULL, 'I'},
		{"force", 0, NULL, 'F'},
		{"ignore", 0, NULL, 'g'},
		{"pedantic", 0, NULL, 'P'},
		{"runstate", 0, NULL, 'r'},
		{"remove", 0, NULL, 'R'},
		{"period", 1, NULL, 'p'},
		{"pid-file", 1, NULL, 'e'},
		{"background", 0, NULL, 'b'},
		{"syslog", 0, NULL, 's'},
		{"debug", 0, NULL, 'd'},
		{"version", 0, NULL, 'v'},
		{NULL, 0, NULL, 0},
	};
	static const char *const devfiles[] = {
		"/dev/snd/controlC",
		"/dev/snd/pcmC",
		"/dev/snd/midiC",
		"/dev/snd/hwC",
		NULL
	};
	char *cfgfile = SYS_ASOUNDRC;
	char *initfile = DATADIR "/init/00main";
	char *pidfile = SYS_PIDFILE;
	char *cardname, ncardname[16];
	char *cmd;
	const char *const *tmp;
	int removestate = 0;
	int init_fallback = 1; /* new default behavior */
	int period = 5*60;
	int background = 0;
	int res;

	command = argv[0];
	while (1) {
		int c;

		if ((c = getopt_long(argc, argv, "hdvf:lFgE:i:IPr:Rp:e:bs", long_option, NULL)) < 0)
			break;
		switch (c) {
		case 'h':
			help();
			return EXIT_SUCCESS;
		case 'f':
			cfgfile = optarg;
			break;
		case 'l':
			do_lock = 1;
			break;
		case 'F':
			force_restore = 1;
			break;
		case 'g':
			ignore_nocards = 1;
			break;
		case 'E':
			if (putenv(optarg)) {
				fprintf(stderr, "environment string '%s' is wrong\n", optarg);
				return EXIT_FAILURE;
			}
			break;
		case 'i':
			initfile = optarg;
			break;
		case 'I':
			init_fallback = 0;
			break;
		case 'r':
			statefile = optarg;
			break;
		case 'R':
			removestate = 1;
			break;
		case 'P':
			force_restore = 0;
			break;
		case 'p':
			period = atoi(optarg);
			if (period < 10)
				period = 5*60;
			else if (period > 24*60*60)
				period = 24*60*60;
			break;
		case 'e':
			pidfile = optarg;
			break;
		case 'b':
			background = 1;
			break;
		case 's':
			use_syslog = 1;
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

	cardname = argc - optind > 1 ? argv[optind + 1] : NULL;
	for (tmp = devfiles; cardname != NULL && *tmp != NULL; tmp++) {
		int len = strlen(*tmp);
		if (!strncmp(cardname, *tmp, len)) {
			long l = strtol(cardname + len, NULL, 0);
			sprintf(ncardname, "%li", l);
			cardname = ncardname;
			break;
		}
	}

	/* the global system file should be always locked */
	if (strcmp(cfgfile, SYS_ASOUNDRC) == 0)
		do_lock = 1;

	/* when running in background, use syslog for reports */
	if (background) {
		use_syslog = 1;
		daemon(0, 0);
	}

	if (use_syslog) {
		openlog("alsactl", LOG_CONS|LOG_PID, LOG_DAEMON);
		syslog(LOG_INFO, "alsactl " SND_UTIL_VERSION_STR " daemon started");
	}

	cmd = argv[optind];
	if (!strcmp(cmd, "init")) {
		res = init(initfile, cardname);
		snd_config_update_free_global();
	} else if (!strcmp(cmd, "store")) {
		res = save_state(cfgfile, cardname);
	} else if (!strcmp(cmd, "restore") ||
                   !strcmp(cmd, "rdaemon") ||
		   !strcmp(cmd, "nrestore")) {
		if (removestate)
			remove(statefile);
		res = load_state(cfgfile, initfile, cardname, init_fallback);
		if (!strcmp(cmd, "rdaemon"))
			res = state_daemon(cfgfile, cardname, period, pidfile);
		if (!strcmp(cmd, "nrestore"))
			res = state_daemon_kill(pidfile, "rescan");
	} else if (!strcmp(cmd, "daemon")) {
		res = state_daemon(cfgfile, cardname, period, pidfile);
	} else if (!strcmp(cmd, "kill")) {
		res = state_daemon_kill(pidfile, cardname);
	} else {
		fprintf(stderr, "alsactl: Unknown command '%s'...\n", cmd);
		res = -ENODEV;
	}

	snd_config_update_free_global();
	if (use_syslog) {
		syslog(LOG_INFO, "alsactl daemon stopped");
		closelog();
	}
	return res < 0 ? -res : 0;
}
