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
 *  Authors: Liam Girdwood <lrg@slimlogic.co.uk>
 *           Stefan Schmidt <stefan@slimlogic.co.uk>
 *           Justin Xu <justinx@slimlogic.co.uk>
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <alsa/asoundlib.h>
#include <alsa/use-case.h>

#define MAX_BUF 256

enum uc_cmd {
	/* list verb, devices and modifiers */
	OM_UNKNOWN = 0,
	OM_LIST_VERBS,
	OM_LIST_DEVICES,
	OM_LIST_MODIFIERS,

	/* enable and disable verb, device and modifier */
	OM_SET_VERB,
	OM_GET_VERB,
	OM_ENABLE_DEVICE,
	OM_ENABLE_MODIFIER,
	OM_DISABLE_DEVICE,
	OM_DISABLE_MODIFIER,
	OM_SWITCH_DEVICE,
	OM_SWITCH_MODIFIER,

	/* dump sound card kcontrols */
	OP_DUMP,
	OP_HELP,
	OP_QUIT,
	OP_RESET,
};

static void dump_help(char *name)
{
	if (name)
		printf("Usage: \n"
			"  %s <card>"
			"  %s <card> <cmd> [use case] [<cmd> [use case]]\n",
			name, name);

	printf(	"  reset  - reset sound card to default state\n"
		"  listv  - list available use case verbs\n"
		"  getv - get current verb\n"
		"  setv <name> - apply use case verb <name>\n"
		"  setd <name> - enable use case device <name>\n"
		"  setm <name> - enable use case modifier <name>\n"
		"  switchd <old> <new> - disable <old> device enable <new> device\n"
		"  switchm <old> <new> - disable <old> modifier enable <new> modifier\n"
		"  cleard <name> - disable use case device <name>\n"
		"  clearm <name> - disable use case modifier <name>\n"
		"  listd  - list available use case devices for current verb\n"
		"  listm  - list available use case modifiers for current verb\n"
		"  dump - dump all mixer control values\n"
		"  h - help\n"
		"  q - quit\n");
}

/* list devices and status for current verb */
static int list_verb_device_status(snd_use_case_mgr_t *uc_mgr)
{
	const char **device_list, *verb;
	int i, enabled, num;

	verb = snd_use_case_get_verb(uc_mgr);
	if (verb == NULL) {
		printf(" no verb currently enabled.\n");
		return -ENODEV;
	}

	num = snd_use_case_get_device_list(uc_mgr, verb, &device_list);
	if (num <= 0) {
		printf(" no devices.\n");
		return 0;
	}

	for (i = 0; i < num; i++) {
		enabled = snd_use_case_get_device_status(uc_mgr,
				device_list[i]);
		if (enabled < 0)
			printf(" %s: failed to get status %d\n",
				device_list[i], enabled);
		else
			printf(" %s: %s\n", device_list[i],
				enabled ? "enabled" : "disabled");
	}
	return num;
}

/* list devices and status for current verb */
static int list_verb_modifiers_status(snd_use_case_mgr_t *uc_mgr)
{
	const char **modifier_list, *verb;
	int i, enabled, num;

	verb = snd_use_case_get_verb(uc_mgr);
	if (verb == NULL)
		return -ENODEV;

	num = snd_use_case_get_mod_list(uc_mgr, verb, &modifier_list);
	if (num <= 0) {
		printf(" no modifiers\n");
		return 0;
	}

	for (i = 0; i < num; i++) {
		enabled = snd_use_case_get_modifier_status(uc_mgr,
				modifier_list[i]);
		if (enabled < 0)
			printf(" %s: failed to get status %d\n", 
				modifier_list[i], enabled);
		else
			printf(" %s: %s\n", modifier_list[i],
				enabled ? "enabled" : "disabled");
	}
	return num;
}

static int list_verbs(snd_use_case_mgr_t *uc_mgr)
{
	const char **verb_list;
	int num, i;

	/* get list of use case verbs */
	num = snd_use_case_get_verb_list(uc_mgr, &verb_list);
	if (num == 0) {
		printf(" error: no verbs defined for sound card\n");
		return 0;
	} else if (num < 0) {
		printf(" error: can't get verbs for sound card\n");
		return num;
	}

	for (i = 0 ; i < num ; i++) {
		const char *verb = verb_list[i];
		printf(" %s\n", verb);
	}

	return 0;
}

static int parse_cmdline(char *cmd, enum uc_cmd *command)
{
	if (!strcmp(cmd, "listv"))
		*command = OM_LIST_VERBS;
	else if (!strcmp(cmd, "listd"))
		*command = OM_LIST_DEVICES;
	else if (!strcmp(cmd, "listm"))
		*command = OM_LIST_MODIFIERS;
	else if (!strcmp(cmd, "setv"))
		*command = OM_SET_VERB;
	else if (!strcmp(cmd, "getv"))
		*command = OM_GET_VERB;
	else if (!strcmp(cmd, "setd"))
		*command = OM_ENABLE_DEVICE;
	else if (!strcmp(cmd, "setm"))
		*command = OM_ENABLE_MODIFIER;
	else if (!strcmp(cmd, "cleard"))
		*command = OM_DISABLE_DEVICE;
	else if (!strcmp(cmd, "clearm"))
		*command = OM_DISABLE_MODIFIER;
	else if (!strcmp(cmd, "switchd"))
		*command = OM_SWITCH_DEVICE;
	else if (!strcmp(cmd, "switchm"))
		*command = OM_SWITCH_MODIFIER;
	else if  (!strcmp(cmd, "q"))
		*command = OP_QUIT;
	else if  (!strcmp(cmd, "h"))
		*command = OP_HELP;
	else if (!strcmp(cmd, "dump"))
		*command = OP_DUMP;
	else if (!strcmp(cmd, "reset"))
		*command = OP_RESET;

	if (*command)
		return 1;
	return -1;
}

static int parse_cmd(char *cmd, enum uc_cmd *command, const char **parameter)
{
	char *cmd_c = cmd;

	*parameter = NULL;
	*command = OM_UNKNOWN;

	/*
	 * fgets() reads '\n' from stdin
	 * it needs to be removed here
	 */
	cmd_c = strchr(cmd, '\n');
	if (cmd_c != NULL)
		*cmd_c = 0;

	cmd_c = cmd;

	/* Truncate spaces */
	while (*cmd_c == ' ')
		cmd_c++;

	if (*cmd_c == 0)
		return 0;

	cmd = cmd_c;

	cmd_c = strchr(cmd, ' ');
	if (cmd_c != NULL) {
		*cmd_c = 0;
		/* Truncate spaces */
		while (*++cmd_c == ' ');
		*parameter = cmd_c;
	}

	return parse_cmdline(cmd, command);
}

static int get_switch_parameter(const char *parameter, const char **old,
							const char **new)
{
	char *c;

	if (parameter == NULL)
		return -EINVAL;

	*old = parameter;

	c = strchr(parameter, ' ');
	if (c != NULL) {
		*c = 0;

		/* Truncate spaces */
		while (*++c == ' ');

		if (*c) {
			*new = c;
			return 0;
		}
	}

	return -EINVAL;
}

static int do_exit = 0;

static int handle_command(snd_use_case_mgr_t *uc_mgr, enum uc_cmd command,
							const char *parameter)
{
	int ret;
	const char *verb;
	const char *old = NULL, *new = NULL;

	switch (command) {
	case OM_UNKNOWN:
		printf(" error: unknown command\n");
		break;
	case OM_LIST_VERBS:
		return list_verbs(uc_mgr);
	case OM_LIST_DEVICES:
		return list_verb_device_status(uc_mgr);
	case OM_LIST_MODIFIERS:
		return list_verb_modifiers_status(uc_mgr);
	case OM_SET_VERB:
		return snd_use_case_set_verb(uc_mgr, parameter);
	case OM_GET_VERB:
		verb = snd_use_case_get_verb(uc_mgr);
		if (verb != NULL)
			printf(" current verb: %s\n", verb);
		else
			printf(" no verb enabled.\n");
		return 0;
	case OM_ENABLE_DEVICE:
		return snd_use_case_enable_device(uc_mgr, parameter);
	case OM_ENABLE_MODIFIER:
		return snd_use_case_enable_modifier(uc_mgr, parameter);
	case OM_DISABLE_DEVICE:
		return snd_use_case_disable_device(uc_mgr, parameter);
	case OM_DISABLE_MODIFIER:
		return snd_use_case_disable_modifier(uc_mgr, parameter);
	case OM_SWITCH_DEVICE:
		ret = get_switch_parameter(parameter, &old, &new);
		if (ret)
			return ret;
		else
			return snd_use_case_switch_device(uc_mgr, old, new);
	case OM_SWITCH_MODIFIER:
		ret = get_switch_parameter(parameter, &old, &new);
		if (ret)
			return ret;
		else
			return snd_use_case_switch_modifier(uc_mgr, old, new);
	case OP_HELP:
		dump_help(NULL);
		break;
	case OP_QUIT:
		do_exit = 1;
		break;
	case OP_RESET:
		return snd_use_case_mgr_reset(uc_mgr);
	default:
		break;
	}

	return 0;
}

int main(int argc, char *argv[])
{
	snd_use_case_mgr_t  *uc_mgr;
	const char *parameter;
	char cmd[MAX_BUF];
	enum uc_cmd command;
	int card_argv = 1, cmd_status, i, err;

	if (argc < 2) {
		dump_help(argv[0]);
		exit(1);
	}

	/* check for dump as we dont need to open UCM */
	if (argc == 3 && !strcmp(argv[2], "dump")) {
		snd_use_case_dump(argv[1]);
		return 0;
	}

	/* open library */
	uc_mgr = snd_use_case_mgr_open(argv[card_argv]);
	if (uc_mgr == NULL) {
		printf("%s: error failed to open sound card %s\n",
			argv[0], argv[card_argv]);
		return 0;
	}

	/* parse and execute any command line commands */
	if (argc >= 3) {
		for (i = 2; i < argc; i++) {
			if (parse_cmdline(argv[i], &command) == -1) {
				printf("error: unknown command %s\n", argv[i]);
				goto out;
			} else {
				switch(command) {
				case OM_SWITCH_DEVICE:
				case OM_SWITCH_MODIFIER:
					if (++i >= argc) {
						printf("error: %s missing argument\n", argv[i-1]);
						goto out;
					}
					if (++i >= argc) {
						printf("error: %s missing argument\n", argv[i-2]);
						goto out;
					}
					err = handle_command(uc_mgr, command, argv[i -1]);
					if (err < 0) {
						printf(" error: command '%s' failed: %d\n",
							argv[i - 2], err);
						goto out;
					}
				case OM_SET_VERB:
				case OM_ENABLE_DEVICE:
				case OM_ENABLE_MODIFIER:
				case OM_DISABLE_DEVICE:
				case OM_DISABLE_MODIFIER:
					if (++i >= argc) {
						printf("error: %s missing argument\n", argv[i-1]);
						goto out;
					}
					err = handle_command(uc_mgr, command, argv[i]);
					if (err < 0) {
						printf(" error: command '%s' failed: %d\n",
							argv[i - 1], err);
						goto out;
					}
					break;
				default:
					err = handle_command(uc_mgr, command, NULL);
					if (err < 0) {
						printf(" error: command '%s' failed: %d\n",
							argv[i], err);
						goto out;
					}
					break;
				}
			}
		}
	}

	printf("Starting %s - 'q' to quit\n", argv[0]);
	/* run the interactive command parser and handler */
	while (!do_exit) {
		printf("%s>> ", argv[0]);
		fflush(stdin);
		if (fgets(cmd, MAX_BUF, stdin) == NULL)
			break;
			cmd_status = parse_cmd(cmd, &command, &parameter);
		if (cmd_status == -1) {
			printf(" error: unknown command %s\n", cmd);
			continue;
		} else if (cmd_status == 1) {
			err = handle_command(uc_mgr, command, parameter);
			if (err < 0) {
				printf(" error: command '%s' failed: %d\n",
					cmd, err);
			}
		}
	}

out:
	snd_use_case_mgr_close(uc_mgr);
	return 0;
}
