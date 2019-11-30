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
 *  Copyright (C) 2019 Red Hat Inc.
 *  Authors: Jaroslav Kysela <perex@perex.cz>
 */

#include <stdio.h>
#include <string.h>
#include <alsa/asoundlib.h>
#include <alsa/use-case.h>
#include "usecase.h"
#include "aconfig.h"
#include "version.h"

struct renderer {
	int (*init)(struct renderer *r);
	void (*done)(struct renderer *r);
	int (*verb_begin)(struct renderer *r,
			  const char *verb,
			  const char *comment);
	int (*verb_end)(struct renderer *r);
	int (*device_block_begin)(struct renderer *r);
	int (*device_block_end)(struct renderer *r);
	int (*device_begin)(struct renderer *r,
			    const char *device,
			    const char *comment);
	int (*device_end)(struct renderer *r);
	int (*modifier_block_begin)(struct renderer *r);
	int (*modifier_block_end)(struct renderer *r);
	int (*modifier_begin)(struct renderer *r,
			      const char *device,
			      const char *comment);
	int (*modifier_end)(struct renderer *r);
	int (*supported_begin)(struct renderer *r);
	int (*supported_value)(struct renderer *r, const char *value, int last);
	int (*supported_end)(struct renderer *r);
	int (*conflict_begin)(struct renderer *r);
	int (*conflict_value)(struct renderer *r, const char *value, int last);
	int (*conflict_end)(struct renderer *r);
	int (*value_begin)(struct renderer *r);
	int (*value_end)(struct renderer *r);
	int (*value)(struct renderer *r, const char *ident, const char *value);
	void *opaque;
};

/*
 * Text renderer
 */

struct text {
	char a[1];
};

static char *tesc(const char *s, char *buf, size_t buf_len)
{
	char *dst = buf;
	char c = '\0';
	if (strchr(s, '"') || strchr(s, ' ') || strchr(s, '.')) {
		*dst++ = c = '"';
		buf_len--;
	}
	while (*s && buf_len > 2) {
		if (*s == '"') {
			if (buf_len > 3) {
				*dst++ = '\\';
				*dst++ = *s++;
				buf_len -= 2;
				continue;
			} else {
				break;
			}
		}
		*dst++ = *s++;
	}
	if (c)
		*dst++ = c;
	*dst = '\0';
	return buf;
}

#define ESC(s, esc) tesc((s), (esc), sizeof(esc))

static int text_verb_start(struct renderer *r, const char *verb, const char *comment)
{
	char buf1[128], buf2[128];
	printf("Verb.%s {\n", ESC(verb, buf1));
	if (comment && comment[0])
		printf("\tComment %s\n", ESC(comment, buf2));
	return 0;
}

static int text_verb_end(struct renderer *r)
{
	printf("}\n");
	return 0;
}

static int text_2nd_level_begin(struct renderer *r,
				const char *key,
				const char *val,
				const char *comment)
{
	char buf1[128], buf2[128];
	printf("\t%s.%s {\n", key, ESC(val, buf1));
	if (comment && comment[0])
		printf("\t\tComment %s\n", ESC(comment, buf2));
	return 0;
}

static int text_2nd_level_end(struct renderer *r)
{
	printf("\t}\n");
	return 0;
}

static int text_2nd_level(struct renderer *r, const char *txt)
{
	printf("\t\t%s", txt);
	return 0;
}

static int text_3rd_level(struct renderer *r, const char *txt)
{
	printf("\t\t\t%s", txt);
	return 0;
}

static int text_dev_start(struct renderer *r, const char *dev, const char *comment)
{
	return text_2nd_level_begin(r, "Device", dev, comment);
}

static int text_mod_start(struct renderer *r, const char *dev, const char *comment)
{
	return text_2nd_level_begin(r, "Modifier", dev, comment);
}

static int text_supcon_start(struct renderer *r, const char *key)
{
	if (text_2nd_level(r, key))
		return 1;
	printf(" [\n");
	return 0;
}

static int text_supcon_value(struct renderer *r, const char *value, int last)
{
	char buf[256];
	ESC(value, buf);
	if (!last && strlen(buf) < sizeof(buf) - 2)
		strcat(buf, ",");
	if (text_3rd_level(r, buf))
		return 1;
	printf("\n");
	return 0;
}

static int text_supcon_end(struct renderer *r)
{
	return text_2nd_level(r, "]\n");
}

static int text_sup_start(struct renderer *r)
{
	return text_supcon_start(r, "SupportedDevices");
}

static int text_con_start(struct renderer *r)
{
	return text_supcon_start(r, "ConflictingDevices");
}

static int text_value_begin(struct renderer *r)
{
	return text_2nd_level(r, "Values {\n");
}

static int text_value_end(struct renderer *r)
{
	return text_2nd_level(r, "}\n");
}

static int text_value(struct renderer *r, const char *ident, const char *value)
{
	char buf1[256], buf2[256];
	int err;

	ESC(ident, buf1);
	err = text_3rd_level(r, buf1);
	if (err < 0)
		return err;
	ESC(value, buf2);
	printf(" %s\n", buf2);
	return 0;
}

static struct renderer text_renderer = {
	.verb_begin = text_verb_start,
	.verb_end = text_verb_end,
	.device_begin = text_dev_start,
	.device_end = text_2nd_level_end,
	.modifier_begin = text_mod_start,
	.modifier_end = text_2nd_level_end,
	.supported_begin = text_sup_start,
	.supported_value = text_supcon_value,
	.supported_end = text_supcon_end,
	.conflict_begin = text_con_start,
	.conflict_value = text_supcon_value,
	.conflict_end = text_supcon_end,
	.value_begin = text_value_begin,
	.value_end = text_value_end,
	.value = text_value,
};

/*
 * JSON renderer
 */

struct json {
	int block[5];
};

static char *jesc(const char *s, char *buf, size_t buf_len)
{
	char *dst = buf;
	char c = '"';
	*dst++ = c;
	buf_len--;
	while (*s && buf_len > 2) {
		if (*s == '"') {
			if (buf_len > 3) {
				*dst++ = '\\';
				*dst++ = *s++;
				buf_len -= 2;
				continue;
			} else {
				break;
			}
		}
		*dst++ = *s++;
	}
	*dst++ = c;
	*dst = '\0';
	return buf;
}

#define JESC(s, esc) jesc((s), (esc), sizeof(esc))

static void json_block(struct renderer *r, int level, int last)
{
	struct json *j = r->opaque;
	printf((j->block[level] && !last) ? ",\n" : "\n");
	j->block[level] = last ? 0 : 1;
}

static int json_init(struct renderer *r)
{
	printf("{\n  \"Verbs\": {");
	return 0;
}

static void json_done(struct renderer *r)
{
	json_block(r, 0, 1);
	printf("  }\n}\n");
}

static int json_verb_start(struct renderer *r, const char *verb, const char *comment)
{
	char buf[256];
	json_block(r, 0, 0);
	printf("    %s: {", JESC(verb, buf));
	if (comment && comment[0]) {
		json_block(r, 1, 0);
		printf("      \"Comment\": %s", JESC(comment, buf));
	}
	return 0;
}

static int json_verb_end(struct renderer *r)
{
	json_block(r, 1, 1);
	printf("    }");
	return 0;
}

static int json_2nd_level_block_end(struct renderer *r)
{
	json_block(r, 2, 1);
	printf("      }");
	return 0;
}

static int json_2nd_level_begin(struct renderer *r,
				const char *val,
				const char *comment)
{
	char buf[256];
	json_block(r, 2, 0);
	printf("        %s: {", JESC(val, buf));
	if (comment && comment[0]) {
		json_block(r, 3, 0);
		printf("          \"Comment\": %s", JESC(comment, buf));
	}
	return 0;
}

static int json_2nd_level_end(struct renderer *r)
{
	json_block(r, 3, 1);
	printf("        }");
	return 0;
}

static int json_2nd_level(struct renderer *r, const char *txt)
{
	printf("          %s", txt);
	return 0;
}

static int json_3rd_level(struct renderer *r, const char *txt)
{
	printf("            %s", txt);
	return 0;
}

static int json_dev_block_start(struct renderer *r)
{
	json_block(r, 1, 0);
	printf("      \"Devices\": {");
	return 0;
}

static int json_mod_block_start(struct renderer *r)
{
	json_block(r, 1, 0);
	printf("      \"Modifiers\": {");
	return 0;
}

static int json_supcon_start(struct renderer *r, const char *key)
{
	json_block(r, 3, 0);
	if (json_2nd_level(r, key))
		return 1;
	printf(": [");
	return 0;
}

static int json_supcon_value(struct renderer *r, const char *value, int last)
{
	char buf[256];
	JESC(value, buf);
	json_block(r, 4, 0);
	return json_3rd_level(r, buf);
}

static int json_supcon_end(struct renderer *r)
{
	json_block(r, 4, 1);
	return json_2nd_level(r, "]");
}

static int json_sup_start(struct renderer *r)
{
	return json_supcon_start(r, "\"SupportedDevices\"");
}

static int json_con_start(struct renderer *r)
{
	return json_supcon_start(r, "\"ConflictingDevices\"");
}

static int json_value_begin(struct renderer *r)
{
	json_block(r, 3, 0);
	return json_2nd_level(r, "\"Values\": {");
}

static int json_value_end(struct renderer *r)
{
	json_block(r, 4, 1);
	return json_2nd_level(r, "}");
}

static int json_value(struct renderer *r, const char *ident, const char *value)
{
	char buf[256];
	int err;

	json_block(r, 4, 0);
	JESC(ident, buf);
	err = json_3rd_level(r, buf);
	if (err < 0)
		return err;
	JESC(value, buf);
	printf(": %s", buf);
	return 0;
}

static struct renderer json_renderer = {
	.init = json_init,
	.done = json_done,
	.verb_begin = json_verb_start,
	.verb_end = json_verb_end,
	.device_block_begin = json_dev_block_start,
	.device_block_end = json_2nd_level_block_end,
	.device_begin = json_2nd_level_begin,
	.device_end = json_2nd_level_end,
	.modifier_block_begin = json_mod_block_start,
	.modifier_block_end = json_2nd_level_block_end,
	.modifier_begin = json_2nd_level_begin,
	.modifier_end = json_2nd_level_end,
	.supported_begin = json_sup_start,
	.supported_value = json_supcon_value,
	.supported_end = json_supcon_end,
	.conflict_begin = json_con_start,
	.conflict_value = json_supcon_value,
	.conflict_end = json_supcon_end,
	.value_begin = json_value_begin,
	.value_end = json_value_end,
	.value = json_value,
};

/*
 * universal dump functions
 */

static int render_devlist(struct context *context,
			  struct renderer *render,
			  const char *verb,
			  const char *device,
			  const char *list,
			  int (*begin)(struct renderer *),
			  int (*value)(struct renderer *, const char *value, int last),
			  int (*end)(struct renderer *))
{
	snd_use_case_mgr_t *uc_mgr = context->uc_mgr;
	const char **dev_list;
	char buf[256];
	int err = 0, j, dev_num;

	snprintf(buf, sizeof(buf), "%s/%s/%s", list, device, verb);
	dev_num = snd_use_case_get_list(uc_mgr, buf, &dev_list);
	if (dev_num < 0) {
		fprintf(stderr, "%s: unable to get %s for verb '%s' for device '%s'\n",
			context->command, list, verb, device);
		return dev_num;
	}
	if (dev_num > 0) {
		err = begin(render);
		if (err < 0)
			goto __err;
		for (j = 0; j < dev_num; j++) {
			err = value(render, dev_list[j], j + 1 == dev_num);
			if (err < 0)
				goto __err;
		}
		err = end(render);
	}
__err:
	snd_use_case_free_list(dev_list, dev_num);
	return err;
}

static int render_values(struct context *context,
			 struct renderer *render,
			 const char *verb,
			 const char *device)
{
	snd_use_case_mgr_t *uc_mgr = context->uc_mgr;
	const char **list, *value;
	char buf[256];
	int err = 0, j, num;

	snprintf(buf, sizeof(buf), "_identifiers/%s/%s", device, verb);
	num = snd_use_case_get_list(uc_mgr, buf, &list);
	if (num < 0) {
		fprintf(stderr, "%s: unable to get _identifiers for verb '%s' for device '%s': %s\n",
			context->command, verb, device, snd_strerror(num));
		return num;
	}
	if (num == 0)
		goto __err;
	if (render->value_begin) {
		err = render->value_begin(render);
		if (err < 0)
			goto __err;
	}
	for (j = 0; j < num; j++) {
		snprintf(buf, sizeof(buf), "%s/%s/%s", list[j], device, verb);
		err = snd_use_case_get(uc_mgr, buf, &value);
		if (err < 0) {
			fprintf(stderr, "%s: unable to get value '%s' for verb '%s' for device '%s': %s\n",
				context->command, list[j], verb, device, snd_strerror(err));
			goto __err;
		}
		err = render->value(render, list[j], value);
		free((char *)value);
		if (err < 0)
			goto __err;
	}
	if (render->value_end)
		err = render->value_end(render);
__err:
	snd_use_case_free_list(list, num);
	return err;
}

static int render_device(struct context *context,
			 struct renderer *render,
			 const char *verb,
			 const char *device)
{
	int err;

	err = render_devlist(context, render, verb, device,
			     "_supporteddevs",
			     render->supported_begin,
			     render->supported_value,
			     render->supported_end);
	if (err < 0)
		return err;
	err = render_devlist(context, render, verb, device,
			     "_conflictingdevs",
			     render->conflict_begin,
			     render->conflict_value,
			     render->conflict_end);
	if (err < 0)
		return err;
	return render_values(context, render, verb, device);
}

static void render(struct context *context, struct renderer *render)
{
	snd_use_case_mgr_t *uc_mgr = context->uc_mgr;
	int i, j, num, dev_num;
	const char **list, **dev_list, *verb, *comment;
	char buf[256];

	num = snd_use_case_verb_list(uc_mgr, &list);
	if (num < 0) {
		fprintf(stderr, "%s: no verbs found\n", context->command);
		return;
	}
	if (render->init && render->init(render))
		goto __end;
	for (i = 0; i < num; i += 2) {
		/* verb */
		verb = list[i + 0];
		comment = list[i + 1];
		if (render->verb_begin(render, verb, comment))
			break;
		/* devices */
		snprintf(buf, sizeof(buf), "_devices/%s", verb);
		dev_num = snd_use_case_get_list(uc_mgr, buf, &dev_list);
		if (dev_num < 0) {
			fprintf(stderr, "%s: unable to get devices for verb '%s'\n",
							context->command, verb);
			continue;
		}
		if (dev_num == 0)
			goto __mods;
		if (render->device_block_begin && render->device_block_begin(render)) {
			snd_use_case_free_list(dev_list, dev_num);
			goto __end;
		}
		for (j = 0; j < dev_num; j += 2) {
			render->device_begin(render, dev_list[j + 0], dev_list[j + 1]);
			if (render_device(context, render, verb, dev_list[j + 0])) {
				snd_use_case_free_list(dev_list, dev_num);
				goto __end;
			}
			render->device_end(render);
		}
		snd_use_case_free_list(dev_list, dev_num);
		if (render->device_block_end && render->device_block_end(render))
			goto __end;
__mods:
		/* modifiers */
		snprintf(buf, sizeof(buf), "_modifiers/%s", verb);
		dev_num = snd_use_case_get_list(uc_mgr, buf, &dev_list);
		if (dev_num < 0) {
			fprintf(stderr, "%s: unable to get modifiers for verb '%s'\n",
							context->command, verb);
			continue;
		}
		if (dev_num == 0)
			goto __verb_end;
		if (render->modifier_block_begin && render->modifier_block_begin(render)) {
			snd_use_case_free_list(dev_list, dev_num);
			goto __end;
		}
		for (j = 0; j < dev_num; j += 2) {
			render->modifier_begin(render, dev_list[j + 0], dev_list[j + 1]);
			render->modifier_end(render);
		}
		snd_use_case_free_list(dev_list, dev_num);
		if (render->modifier_block_end && render->modifier_block_end(render))
			goto __end;
__verb_end:
		/* end */
		if (render->verb_end(render))
			break;
	}
	if (render->done)
		render->done(render);
__end:
	snd_use_case_free_list(list, num);
}

void dump(struct context *context, const char *format)
{
	struct renderer r;
	struct text t;
	struct json j;

	r.opaque = NULL;
	if (strcasecmp(format, "text") == 0 ||
	    strcasecmp(format, "txt") == 0) {
		memset(&t, 0, sizeof(t));
		r = text_renderer;
		r.opaque = &t;
	} else if (strcasecmp(format, "json") == 0) {
		memset(&j, 0, sizeof(j));
		r = json_renderer;
		r.opaque = &j;
	}
	if (r.opaque != NULL) {
		render(context, &r);
		return;
	}
	fprintf(stderr, "%s: unknown dump format '%s'\n",
					context->command, format);
}
