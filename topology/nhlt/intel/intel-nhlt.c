// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2021 Intel Corporation. All rights reserved.
//
// Author: Jaska Uimonen <jaska.uimonen@linux.intel.com>

#include "intel-nhlt.h"

static int get_int_val(snd_config_t *input, long *int_val, snd_config_t *top)
{
	char tplg_define[128] = "Define.";
	snd_config_t *n;
	const char *s;
	int ret;

	if (snd_config_get_string(input, &s) < 0)
		return snd_config_get_integer(input, int_val);

	if (*s != '$')
		return 0;

	strcat(tplg_define, s + 1);

	ret = snd_config_search(top, tplg_define, &n);
	if (ret < 0)
		return ret;

	return snd_config_get_integer(n, int_val);
}

static int get_string_val(snd_config_t *input, const char **string_val, snd_config_t *top)
{
	char tplg_define[128] = "Define.";
	snd_config_t *n;
	int ret;

	if (snd_config_get_string(input, string_val) < 0)
		return -EINVAL;

	if (**string_val != '$')
		return 0;

	strcat(tplg_define, *string_val + 1);

	ret = snd_config_search(top, tplg_define, &n);
	if (ret < 0)
		return ret;

	return snd_config_get_string(n, string_val);
}

#ifdef NHLT_DEBUG
static void print_array_values(struct dai_values *values, int size)
{
	int i;

	fprintf(stdout,	"print parsed array:\n");
	for (i = 0; i < size; i++, values++) {
		if (values->type == SND_CONFIG_TYPE_INTEGER)
			fprintf(stdout,	"%s %ld\n", values->name, *values->int_val);
		else
			fprintf(stdout,	"%s %s\n", values->name, *values->string_val);
	}
	fprintf(stdout,	"\n");
}
#endif

int find_set_values(struct dai_values *values, int size, snd_config_t *dai_cfg,
		    snd_config_t *top, const char *class_name)
{
	snd_config_iterator_t i, next;
	struct dai_values *temp_val;
	snd_config_t *class_cfg;
	snd_config_t *n;
	const char *id;
	int ret;
	int j;

	/* get default values from class definition */
	ret = snd_config_search(top, class_name, &class_cfg);
	if (ret < 0)
		return ret;

	snd_config_for_each(i, next, class_cfg) {
		n = snd_config_iterator_entry(i);

		if (snd_config_get_id(n, &id) < 0)
			continue;

		for (j = 0, temp_val = values; j < size; j++, temp_val++) {
			if (!strcmp(id, temp_val->name)) {
				temp_val->data = n;
				break;
			}
		}
	}

	/* set instance specific values */
	snd_config_for_each(i, next, dai_cfg) {
		n = snd_config_iterator_entry(i);

		if (snd_config_get_id(n, &id) < 0)
			continue;

		for (j = 0, temp_val = values; j < size; j++, temp_val++) {
			if (!strcmp(id, temp_val->name)) {
				temp_val->data = n;
				break;
			}
		}
	}

	for (j = 0, temp_val = values; j < size; j++, temp_val++) {
		if (!temp_val->data)
			continue;
		if (temp_val->type == SND_CONFIG_TYPE_INTEGER)
			get_int_val(temp_val->data, temp_val->int_val, top);
		else
			get_string_val(temp_val->data, temp_val->string_val, top);
	}

#ifdef NHLT_DEBUG
	print_array_values(values, size);
#endif
	return 0;
}
