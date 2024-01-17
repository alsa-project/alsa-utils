/*
 *  Extract microphone configuration from the ACPI NHLT table
 *
 *  Specification:
 *         https://01.org/sites/default/files/595976_intel_sst_nhlt.pdf
 *
 *     Author: Jaroslav Kysela <perex@perex.cz>
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
 *   Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 *
 */

#include "aconfig.h"
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <getopt.h>
#include <sys/stat.h>
#include <arpa/inet.h>

int debug = 0;

/*
 * Dump dmic parameters in json
 */

#define ACPI_HDR_SIZE (4 + 4 + 1 + 1 + 6 + 8 + 4 + 4 + 4)
#define NHLT_EP_HDR_SIZE (4 + 1 + 1 + 2 + 2 + 2 + 4 + 1 + 1 + 1)
#define VENDOR_MIC_CFG_SIZE (1 + 1 + 2 + 2 + 2 + 1 + 1 + 2 + 2 + 2 + 2 + 2 + 2)

static const char *microphone_type(uint8_t type)
{
	switch (type) {
	case 0: return "omnidirectional";
	case 1: return "subcardoid";
	case 2: return "cardoid";
	case 3: return "supercardoid";
	case 4: return "hypercardoid";
	case 5: return "8shaped";
	case 7: return "vendor";
	}
	return "unknown";
}

static const char *microphone_location(uint8_t location)
{
	switch (location) {
	case 0: return "laptop-top-panel";
	case 1: return "laptop-bottom-panel";
	case 2: return "laptop-left-panel";
	case 3: return "laptop-right-panel";
	case 4: return "laptop-front-panel";
	case 5: return "laptop-rear-panel";
	}
	return "unknown";
}


static inline uint8_t get_u8(uint8_t *base, uint32_t off)
{
	return *(base + off);
}

static inline int32_t get_s16le(uint8_t *base, uint32_t off)
{
	uint32_t v =  *(base + off + 0) |
		      (*(base + off + 1) << 8);
	if (v & 0x8000)
		return -((int32_t)0x10000 - (int32_t)v);
	return v;
}

static inline uint32_t get_u32le(uint8_t *base, uint32_t off)
{
	return   *(base + off + 0) |
		(*(base + off + 1) << 8) |
		(*(base + off + 2) << 16) |
		(*(base + off + 3) << 24);
}

static int nhlt_dmic_config(FILE *out, uint8_t *dmic, uint8_t mic)
{
	int32_t angle_begin, angle_end;

	if (mic > 0)
		fprintf(out, ",\n");
	fprintf(out, "\t\t{\n");
	fprintf(out, "\t\t\t\"channel\":%i,\n", mic);
	fprintf(out, "\t\t\t\"type\":\"%s\",\n", microphone_type(get_u8(dmic, 0)));
	fprintf(out, "\t\t\t\"location\":\"%s\"", microphone_location(get_u8(dmic, 1)));
	if (get_s16le(dmic, 2) != 0)
		fprintf(out, ",\n\t\t\t\"speaker-distance\":%i", get_s16le(dmic, 2));
	if (get_s16le(dmic, 4) != 0)
		fprintf(out, ",\n\t\t\t\"horizontal-offset\":%i", get_s16le(dmic, 4));
	if (get_s16le(dmic, 6) != 0)
		fprintf(out, ",\n\t\t\t\"vertical-offset\":%i", get_s16le(dmic, 6));
	if (get_u8(dmic, 8) != 0)
		fprintf(out, ",\n\t\t\t\"freq-low-band\":%i", get_u8(dmic, 8) * 5);
	if (get_u8(dmic, 9) != 0)
		fprintf(out, ",\n\t\t\t\"freq-high-band\":%i", get_u8(dmic, 9) * 500);
	if (get_s16le(dmic, 10) != 0)
		fprintf(out, ",\n\t\t\t\"direction-angle\":%i", get_s16le(dmic, 10));
	if (get_s16le(dmic, 12) != 0)
		fprintf(out, ",\n\t\t\t\"elevation-angle\":%i", get_s16le(dmic, 12));
	angle_begin = get_s16le(dmic, 14);
	angle_end = get_s16le(dmic, 16);
	if (!((angle_begin == 180 && angle_end == -180) ||
	      (angle_begin == -180 && angle_end == 180))) {
		fprintf(out, ",\n\t\t\t\"vertical-angle-begin\":%i,\n", angle_begin);
		fprintf(out, "\t\t\t\"vertical-angle-end\":%i", angle_end);
	}
	angle_begin = get_s16le(dmic, 18);
	angle_end = get_s16le(dmic, 20);
	if (!((angle_begin == 180 && angle_end == -180) ||
	      (angle_begin == -180 && angle_end == 180))) {
		fprintf(out, ",\n\t\t\t\"horizontal-angle-begin\":%i,\n", angle_begin);
		fprintf(out, "\t\t\t\"horizontal-angle-end\":%i", angle_end);
	}
	fprintf(out, "\n\t\t}");
	return 0;
}

static int nhlt_dmic_ep_to_json(FILE *out, uint8_t *ep, uint32_t ep_size)
{
	uint32_t off, specific_cfg_size;
	uint8_t config_type, array_type, mic, num_mics;
	int res;

	off = NHLT_EP_HDR_SIZE;
	specific_cfg_size = get_u32le(ep, off);
	if (off + specific_cfg_size > ep_size)
		goto oob;
	off += 4;
	config_type = get_u8(ep, off + 1);
	if (config_type != 1)	/* mic array */
		return 0;
	array_type = get_u8(ep, off + 2);
	if ((array_type & 0x0f) != 0x0f) {
		fprintf(stderr, "Unsupported ArrayType %02x\n", array_type & 0x0f);
		return -EINVAL;
	}
	num_mics = get_u8(ep, off + 3);
	fprintf(out, "{\n");
	fprintf(out, "\t\"mics-data-version\":1,\n");
	fprintf(out, "\t\"mics-data-source\":\"acpi-nhlt\"");
	for (mic = 0; mic < num_mics; mic++) {
		if (off - NHLT_EP_HDR_SIZE + VENDOR_MIC_CFG_SIZE > specific_cfg_size) {
			fprintf(out, "\n}\n");
			goto oob;
		}
		if (mic == 0)
			fprintf(out, ",\n\t\"mics\":[\n");
		res = nhlt_dmic_config(out, ep + off + 4, mic);
		if (res < 0)
			return res;
		off += VENDOR_MIC_CFG_SIZE;
	}
	if (num_mics > 0)
		fprintf(out, "\n\t]\n");
	fprintf(out, "}\n");
	return num_mics;
oob:
	fprintf(stderr, "Data (out-of-bounds) error\n");
	return -EINVAL;
}

static int nhlt_table_to_json(FILE *out, uint8_t *nhlt, uint32_t size)
{
	uint32_t _size, off, ep_size;
	uint8_t sum = 0, ep, ep_count, link_type, dmics = 0;
	int res;

	_size = get_u32le(nhlt, 4);
	if (_size != size) {
		fprintf(stderr, "Table size mismatch (%08x != %08x)\n", _size, (uint32_t)size);
		return -EINVAL;
	}
	for (off = 0; off < size; off++)
		sum += get_u8(nhlt, off);
	if (sum != 0) {
		fprintf(stderr, "Checksum error (%02x)\n", sum);
		return -EINVAL;
	}
	/* skip header */
	off = ACPI_HDR_SIZE;
	ep_count = get_u8(nhlt, off++);
	for (ep = 0; ep < ep_count; ep++) {
		if (off + 17 > size)
			goto oob;
		ep_size = get_u32le(nhlt, off);
		if (off + ep_size > size)
			goto oob;
		link_type = get_u8(nhlt, off + 4);
		res = 0;
		if (link_type == 2) { 	/* PDM */
			res = nhlt_dmic_ep_to_json(out, nhlt + off, ep_size);
			if (res > 0)
				dmics++;
		}
		if (res < 0)
			return res;
		off += ep_size;
	}
	if (dmics == 0) {
		fprintf(stderr, "No dmic endpoint found\n");
		return -EINVAL;
	}
	return 0;
oob:
	fprintf(stderr, "Data (out-of-bounds) error\n");
	return -EINVAL;
}

static int nhlt_to_json(FILE *out, const char *nhlt_file)
{
	struct stat st;
	uint8_t *buf;
	int _errno, fd, res;
	size_t pos, size;
	ssize_t ret;

	if (stat(nhlt_file, &st))
		return -errno;
	size = st.st_size;
	if (size < 45)
		return -EINVAL;
	buf = malloc(size);
	if (buf == NULL)
		return -ENOMEM;
	fd = open(nhlt_file, O_RDONLY);
	if (fd < 0) {
		_errno = errno;
		fprintf(stderr, "Unable to open file '%s': %s\n", nhlt_file, strerror(errno));
		free(buf);
		return _errno;
	}
	pos = 0;
	while (pos < size) {
		ret = read(fd, buf + pos, size - pos);
		if (ret <= 0) {
			fprintf(stderr, "Short read\n");
			close(fd);
			free(buf);
			return -EIO;
		}
		pos += ret;
	}
	close(fd);
	res = nhlt_table_to_json(out, buf, size);
	free(buf);
	return res;
}

/*
 *
 */

#define PROG "nhlt-dmic-info"
#define PROG_VERSION "1"

#define NHLT_FILE "/sys/firmware/acpi/tables/NHLT"

#define TITLE	0x0100
#define HEADER	0x0200
#define FILEARG 0x0400

#define ARRAY_SIZE(a) (sizeof(a) / sizeof((a)[0]))

struct arg {
	int sarg;
	char *larg;
	char *comment;
};

static struct arg args[] = {
{ TITLE, NULL, "Usage: nhtl-dmic-json <options>" },
{ HEADER, NULL, "global options:" },
{ 'h', "help", "this help" },
{ 'v', "version", "print version of this program" },
{ FILEARG | 'f', "file", "NHLT file (default " NHLT_FILE ")" },
{ FILEARG | 'o', "output", "output file" },
{ 0, NULL, NULL }
};

static void help(void)
{
	struct arg *n = args, *a;
	char *larg, sa[4], buf[32];
	int sarg;

	sa[0] = '-';
	sa[2] = ',';
	sa[3] = '\0';
	while (n->comment) {
		a = n;
		n++;
		sarg = a->sarg;
		if (sarg & (HEADER|TITLE)) {
			printf("%s%s\n", (sarg & HEADER) != 0 ? "\n" : "",
								a->comment);
			continue;
		}
		buf[0] = '\0';
		larg = a->larg;
		sa[1] = a->sarg;
		sprintf(buf, "%s%s%s", sa[1] ? sa : "",
				larg ? "--" : "", larg ? larg : "");
		if (sarg & FILEARG)
			strcat(buf, " #");
		printf("  %-15s  %s\n", buf, a->comment);
	}
}

int main(int argc, char *argv[])
{
	char *nhlt_file = NHLT_FILE;
	char *output_file = "-";
	int i, j, k, res;
	struct arg *a;
	struct option *o, *long_option;
	char *short_option;
	FILE *output = NULL;

	long_option = calloc(ARRAY_SIZE(args), sizeof(struct option));
	if (long_option == NULL)
		exit(EXIT_FAILURE);
	short_option = malloc(128);
	if (short_option == NULL) {
		free(long_option);
		exit(EXIT_FAILURE);
	}
	for (i = j = k = 0; i < (int)ARRAY_SIZE(args); i++) {
		a = &args[i];
		if ((a->sarg & 0xff) == 0)
			continue;
		o = &long_option[j];
		o->name = a->larg;
		o->has_arg = (a->sarg & FILEARG) != 0;
		o->flag = NULL;
		o->val = a->sarg & 0xff;
		j++;
		short_option[k++] = o->val;
		if (o->has_arg)
			short_option[k++] = ':';
	}
	short_option[k] = '\0';
	while (1) {
		int c;

		if ((c = getopt_long(argc, argv, short_option, long_option,
								  NULL)) < 0)
			break;
		switch (c) {
		case 'h':
			help();
			res = EXIT_SUCCESS;
			goto out;
		case 'f':
			nhlt_file = optarg;
			break;
		case 'o':
			output_file = optarg;
			break;
		case 'd':
			debug = 1;
			break;
		case 'v':
			printf(PROG " version " PROG_VERSION "\n");
			res = EXIT_SUCCESS;
			goto out;
		case '?':		// error msg already printed
			help();
			res = EXIT_FAILURE;
			goto out;
		default:		// should never happen
			fprintf(stderr,
			"Invalid option '%c' (%d) not handled??\n", c, c);
		}
	}
	free(short_option);
	short_option = NULL;
	free(long_option);
	long_option = NULL;

	if (strcmp(output_file, "-") == 0) {
		output = stdout;
	} else {
		output = fopen(output_file, "w+");
		if (output == NULL) {
			fprintf(stderr, "Unable to create output file \"%s\": %s\n",
						output_file, strerror(errno));
			res = EXIT_FAILURE;
			goto out;
		}
	}

	if (argc - optind > 0)
		fprintf(stderr, PROG ": Ignoring extra parameters\n");

	res = 0;
	if (nhlt_to_json(output, nhlt_file))
		res = EXIT_FAILURE;

out:
	if (output)
		fclose(output);
	free(short_option);
	free(long_option);
	return res;
}
