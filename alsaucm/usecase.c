/*
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Lesser General Public
 *  License as published by the Free Software Foundation; either
 *  version 2 of the License, or (at your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Lesser General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 *
 *  Support for the verb/device/modifier core logic and API, 
 *  command line tool and file parser was kindly sponsored by 
 *  Texas Instruments Inc.
 *  Support for multiple active modifiers and devices, 
 *  transition sequences, multiple client access and user defined use
 *  cases was kindly sponsored by Wolfson Microelectronics PLC.
 * 
 *  Copyright (C) 2008-2010 SlimLogic Ltd
 *  Copyright (C) 2010 Wolfson Microelectronics PLC
 *  Copyright (C) 2010 Texas Instruments Inc.
 *  Copyright (C) 2010 Red Hat Inc.
 *  Authors: Liam Girdwood <lrg@slimlogic.co.uk>
 *           Stefan Schmidt <stefan@slimlogic.co.uk>
 *           Justin Xu <justinx@slimlogic.co.uk>
 *           Jaroslav Kysela <perex@perex.cz>
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <getopt.h>
#include <alsa/asoundlib.h>
#include <alsa/use-case.h>
#include "usecase.h"
#include "aconfig.h"
#include "version.h"

#define MAX_BUF 256

enum uc_cmd {
	/* management */
	OM_UNKNOWN = 0,
	OM_OPEN,
	OM_RESET,
	OM_RELOAD,
	OM_LISTCARDS,
	OM_DUMP,
	OM_LIST2,
	OM_LIST1,

	/* set/get */
	OM_SET,
	OM_GET,
	OM_GET_VAL,
	OM_GETI,
	OM_GETI_VAL,

	/* misc */
	OM_HELP,
	OM_QUIT,
};

struct cmd {
	int code;
	int args;
	unsigned int opencard:1;
	const char *id;
};

static struct cmd cmds[] = {
	{ OM_OPEN, 1, 0, "open" },
	{ OM_RESET, 0, 1, "reset" },
	{ OM_RELOAD, 0, 1, "reload" },
	{ OM_LISTCARDS, 0, 0, "listcards" },
	{ OM_DUMP, 1, 1, "dump" },
	{ OM_LIST1, 1, 1, "list1" },
	{ OM_LIST2, 1, 1, "list" },
	{ OM_SET, 2, 1, "set" },
	{ OM_GET, 1, 1, "get" },
	{ OM_GET_VAL, 1, 1, "getval" },
	{ OM_GETI, 1, 1, "geti" },
	{ OM_GETI_VAL, 1, 1, "getival" },
	{ OM_DUMP, 1, 1, "dump" },
	{ OM_HELP, 0, 0, "help" },
	{ OM_QUIT, 0, 0, "quit" },
	{ OM_HELP, 0, 0, "h" },
	{ OM_HELP, 0, 0, "?" },
	{ OM_QUIT, 0, 0, "q" },
	{ OM_UNKNOWN, 0, 0, NULL }
};

static void dump_help(struct context *context)
{
	if (context->command)
		printf("Usage: %s <options> [command]\n", context->command);
	printf(
"\nAvailable options:\n"
"  -h,--help                  this help\n"
"  -c,--card NAME             open card NAME\n"
"  -i,--interactive           interactive mode\n"
"  -b,--batch FILE            batch mode (use '-' for the stdin input)\n"
"  -n,--no-open               do not open first card found\n"
"\nAvailable commands:\n"
"  open NAME                  open card NAME\n"
"  reset                      reset sound card to default state\n"
"  reload                     reload configuration\n"
"  listcards                  list available cards\n"
"  dump FORMAT                dump all config information (format: text,json)\n"
"  list IDENTIFIER            list command, for items with value + comment\n"
"  list1 IDENTIFIER           list command, for items without comments\n"
"  get IDENTIFIER             get string value\n"
"  geti IDENTIFIER            get integer value\n"
"  set IDENTIFIER VALUE       set string value\n"
"  h,help                     help\n"
"  q,quit                     quit\n"
);
}

int is_long(const char *str)
{
	char *end;
	if (!*str)
		return 0;
	errno = 0;
	strtol(str, &end, 10);
	if (errno)
		return 0;
	if (*end)
		return 0;
	return 1;
}

static int parse_line(struct context *context, char *line)
{
	char *start, **nargv;
	int c;

	context->argc = 0;
	while (*line) {
		while (*line && (*line == ' ' || *line == '\t' ||
							*line == '\n'))
			line++;
		c = *line;
		if (c == '\0')
			return 0;
		if (c == '\"' || c == '\'') {
			start = ++line;
			while (*line && *line != c)
				line++;
			if (*line) {
				*line = '\0';
				line++;
			}
		} else {
			start = line;
			while (*line && *line != ' ' && *line != '\t' &&
			       *line != '\n')
				line++;
			if (*line) {
				*line = '\0';
				line++;
			}
		}
		if (start[0] == '\0' && context->argc == 0)
			return 0;
		if (context->argc + 1 >= context->arga) {
			context->arga += 4;
			nargv = realloc(context->argv,
					context->arga * sizeof(char *));
			if (nargv == NULL)
				return -ENOMEM;
			context->argv = nargv;
		}
		context->argv[context->argc++] = start;
	}
	return 0;
}

static void my_exit(struct context *context, int exitcode)
{
	if (context->uc_mgr)
		snd_use_case_mgr_close(context->uc_mgr);
	if (context->arga > 0)
		free(context->argv);
	if (context->card)
		free(context->card);
	if (context->batch)
		free(context->batch);
	free(context);
	snd_config_update_free_global();
	exit(exitcode);
}
static void do_initial_open(struct context *context)
{
	int card, err;
	char name[16];

	if (!context->no_open && context->card == NULL) {
		card = -1;
		err = snd_card_next(&card);
		if (err < 0) {
			fprintf(stderr, "%s: no sound card found: %s\n",
					context->command, snd_strerror(err));
			my_exit(context, EXIT_FAILURE);
		}
		snprintf(name, sizeof(name), "hw:%d", card);
		context->card = strdup(name);
	}

	/* open library */
	if (!context->no_open) {
		if (is_long(context->card)) {
			snprintf(name, sizeof(name), "hw:%s", context->card);
			free(context->card);
			context->card = strdup(name);
		}
		if (context->card == NULL) {
			fprintf(stderr, "%s: empty card name\n", context->command);
			my_exit(context, EXIT_FAILURE);
		}
		err = snd_use_case_mgr_open(&context->uc_mgr,
					    context->card);
		if (err < 0) {
			fprintf(stderr,
				"%s: error failed to open sound card %s: %s\n",
				context->command, context->card, snd_strerror(err));
			my_exit(context, EXIT_FAILURE);
		}
	}
}

static int do_one(struct context *context, struct cmd *cmd, char **argv)
{
	const char **list, *str;
	long lval;
	int err, i, j, entries;

	if (cmd->opencard && context->uc_mgr == NULL) {
		if (!context->no_open) {
			do_initial_open(context);
			context->no_open = 1;
		} else {
			fprintf(stderr, "%s: command '%s' requires an open card\n",
					context->command, cmd->id);
			return 0;
		}
	}
	switch (cmd->code) {
	case OM_OPEN:
		if (context->uc_mgr)
			snd_use_case_mgr_close(context->uc_mgr);
		context->uc_mgr = NULL;
		free(context->card);
		context->card = strdup(argv[0]);
		err = snd_use_case_mgr_open(&context->uc_mgr, context->card);
		if (err < 0) {
			fprintf(stderr,
				"%s: error failed to open sound card %s: %s\n",
				context->command, context->card,
				snd_strerror(err));
			return err;
		}
		break;
	case OM_RESET:
		err = snd_use_case_mgr_reset(context->uc_mgr);
		if (err < 0) {
			fprintf(stderr,
				"%s: error failed to reset sound card %s: %s\n",
				context->command, context->card,
				snd_strerror(err));
			return err;
		}
		break;
	case OM_RELOAD:
		err = snd_use_case_mgr_reload(context->uc_mgr);
		if (err < 0) {
			fprintf(stderr,
				"%s: error failed to reload manager %s: %s\n",
				context->command, context->card,
				snd_strerror(err));
			return err;
		}
		break;
	case OM_LISTCARDS:
		err = snd_use_case_card_list(&list);
		if (err < 0) {
			fprintf(stderr,
				"%s: error failed to get card list: %s\n",
				context->command,
				snd_strerror(err));
			return err;
		}
		if (err == 0) {
			printf("  list is empty\n");
			return 0;
		}
		for (i = 0; i < err / 2; i++) {
			printf("  %i: %s\n", i, list[i*2]);
			if (list[i*2+1])
				printf("    %s\n", list[i*2+1]);
		}
		snd_use_case_free_list(list, err);
		break;
	case OM_DUMP:
		dump(context, argv[0]);
		break;
	case OM_LIST1:
	case OM_LIST2:
		switch (cmd->code) {
		case OM_LIST1:
		    entries = 1;
		    break;
		case OM_LIST2:
		    entries = 2;
		    break;
		}

		err = snd_use_case_get_list(context->uc_mgr,
					    argv[0],
					    &list);
		if (err < 0) {
			fprintf(stderr,
				"%s: error failed to get list %s: %s\n",
				context->command, argv[0],
				snd_strerror(err));
			return err;
		}
		if (err == 0) {
			printf("  list is empty\n");
			return 0;
		}
		for (i = 0; i < err / entries; i++) {
			printf("  %i: %s\n", i, list[i*entries]);
			for (j = 0; j < entries - 1; j++)
				if (list[i*entries+j+1])
					printf("    %s\n", list[i*entries+j+1]);
		}
		snd_use_case_free_list(list, err);
		break;
	case OM_SET:
		err = snd_use_case_set(context->uc_mgr, argv[0], argv[1]);
		if (err < 0) {
			fprintf(stderr,
				"%s: error failed to set %s=%s: %s\n",
				context->command, argv[0], argv[1],
				snd_strerror(err));
			return err;
		}
		break;
	case OM_GET:
	case OM_GET_VAL:
		err = snd_use_case_get(context->uc_mgr, argv[0], &str);
		if (err < 0) {
			fprintf(stderr,
				"%s: error failed to get %s: %s\n",
				context->command, argv[0],
				snd_strerror(err));
			return err;
		}
		if (cmd->code == OM_GET)
			printf("  %s=%s\n", argv[0], str);
		else
			printf("%s\n", str);
		free((void *)str);
		break;
	case OM_GETI:
	case OM_GETI_VAL:
		err = snd_use_case_geti(context->uc_mgr, argv[0], &lval);
		if (err < 0) {
			fprintf(stderr,
				"%s: error failed to get integer %s: %s\n",
				context->command, argv[0],
				snd_strerror(err));
			return lval;
		}
		if (cmd->code == OM_GETI)
			printf("  %s=%li\n", argv[0], lval);
		else
			printf("%li\n", lval);
		break;
	case OM_QUIT:
		context->do_exit = 1;
		break;
	case OM_HELP:
		dump_help(context);
		break;
	default:
		fprintf(stderr, "%s: unimplemented command '%s'\n",
				context->command, cmd->id);
		return -EINVAL;
	}
	return 0;
}

static int do_commands(struct context *context)
{
	char *command, **argv;
	struct cmd *cmd;
	int i, acnt, err;

	for (i = 0; i < context->argc && !context->do_exit; i++) {
		command = context->argv[i];
		for (cmd = cmds; cmd->id != NULL; cmd++) {
			if (strcmp(cmd->id, command) == 0)
				break;
		}
		if (cmd->id == NULL) {
			fprintf(stderr, "%s: unknown command '%s'\n",
						context->command, command);
			return -EINVAL;
		}
		acnt = context->argc - (i + 1);
		if (acnt < cmd->args) {
			fprintf(stderr, "%s: expected %i arguments (got %i)\n",
					context->command, cmd->args, acnt);
			return -EINVAL;
		}
		argv = context->argv + i + 1;
		err = do_one(context, cmd, argv);
		if (err < 0)
			return err;
		i += cmd->args;
	}
	return 0;
}

enum {
	OPT_VERSION = 1,
};

int main(int argc, char *argv[])
{
	static const char short_options[] = "hb:c:in";
	static const struct option long_options[] = {
		{"help", 0, 0, 'h'},
		{"version", 0, 0, OPT_VERSION},
		{"card", 1, 0, 'c'},
		{"interactive", 0, 0, 'i'},
		{"batch", 1, 0, 'b'},
		{"no-open", 0, 0, 'n'},
		{0, 0, 0, 0}
	};
	struct context *context;
	const char *command = argv[0];
	int c, err, option_index;
	char cmd[MAX_BUF];
	FILE *in;

	context = calloc(1, sizeof(*context));
	if (context == NULL)
		return EXIT_FAILURE;
	context->command = command;
	while ((c = getopt_long(argc, argv, short_options,
				 long_options, &option_index)) != -1) {
		switch (c) {
		case 'h':
			dump_help(context);
			break;
		case OPT_VERSION:
			printf("%s: version " SND_UTIL_VERSION_STR "\n", command);
			break;
		case 'c':
			if (context->card)
				free(context->card);
			context->card = strdup(optarg);
			break;
		case 'i':
			context->interactive = 1;
			context->batch = NULL;
			break;
		case 'b':
			context->batch = strdup(optarg);
			context->interactive = 0;
			break;
		case 'n':
			context->no_open = 1;
			break;
		default:
			fprintf(stderr, "Try '%s --help' for more information.\n", command);
			my_exit(context, EXIT_FAILURE);
		}
	}

	/* parse and execute any command line commands */
	if (argc > optind) {
		context->argv = argv + optind;
		context->argc = argc - optind;
		err = do_commands(context);
		if (err < 0)
			my_exit(context, EXIT_FAILURE);
	}

	if (!context->interactive && !context->batch)
		my_exit(context, EXIT_SUCCESS);

	if (context->interactive) {
		printf("%s: Interacive mode - 'q' to quit\n", command);
		in = stdin;
	} else {
		if (strcmp(context->batch, "-") == 0) {
			in = stdin;
		} else {
			in = fopen(context->batch, "r");
			if (in == NULL) {
				fprintf(stderr, "%s: error failed to open file '%s': %s\n",
					command, context->batch, strerror(-errno));
				my_exit(context, EXIT_FAILURE);
			}
		}
	}

	/* run the interactive command parser and handler */
	while (!context->do_exit && !feof(in)) {
		if (context->interactive)
			printf("%s>> ", argv[0]);
		fflush(stdin);
		if (fgets(cmd, MAX_BUF, in) == NULL)
			break;
		err = parse_line(context, cmd);
		if (err < 0) {
			fprintf(stderr, "%s: unable to parse line\n",
				command);
			my_exit(context, EXIT_FAILURE);
		}
		err = do_commands(context);
		if (err < 0) {
			if (context->interactive)
				printf("^^^ error, try again\n");
			else
				my_exit(context, EXIT_FAILURE);
		}
	}
	
	if (in != stdin)
		fclose(in);

	my_exit(context, EXIT_SUCCESS);
	return EXIT_SUCCESS;
}
