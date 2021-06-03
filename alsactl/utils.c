/*
 *  Advanced Linux Sound Architecture Control Program - Support routines
 *  Copyright (c) by Jaroslav Kysela <perex@perex.cz>
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

#include <stdlib.h>
#include <stdio.h>
#include <stddef.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <ctype.h>
#include <dirent.h>
#include <syslog.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <limits.h>
#include "alsactl.h"

int file_map(const char *filename, char **buf, size_t *bufsize)
{
	struct stat stats;
	int fd;

	fd = open(filename, O_RDONLY);
	if (fd < 0) {
		return -1;
	}

	if (fstat(fd, &stats) < 0) {
		close(fd);
		return -1;
	}

	*buf = mmap(NULL, stats.st_size, PROT_READ, MAP_SHARED, fd, 0);
	if (*buf == MAP_FAILED) {
		close(fd);
		return -1;
	}
	*bufsize = stats.st_size;

	close(fd);

	return 0;
}

void file_unmap(void *buf, size_t bufsize)
{
	munmap(buf, bufsize);
}

size_t line_width(const char *buf, size_t bufsize, size_t pos)
{
	int esc = 0;
	size_t count;
	
	for (count = pos; count < bufsize; count++) {
		if (!esc && buf[count] == '\n')
			break;
		esc = buf[count] == '\\';
	}

	return count - pos;
}

void initfailed(int cardnumber, const char *reason, int exitcode)
{
	int fp;
	char *str;
	char sexitcode[16];

	if (statefile == NULL)
		return;
	if (snd_card_get_name(cardnumber, &str) < 0)
		return;
	sprintf(sexitcode, "%i", exitcode);
	fp = open(statefile, O_WRONLY|O_CREAT|O_APPEND, 0644);
	(void)write(fp, str, strlen(str));
	(void)write(fp, ":", 1);
	(void)write(fp, reason, strlen(reason));
	(void)write(fp, ":", 1);
	(void)write(fp, sexitcode, strlen(sexitcode));
	(void)write(fp, "\n", 1);
	close(fp);
	free(str);
}

static void syslog_(int prio, const char *fcn, long line,
		    const char *fmt, va_list ap)
{
	char buf[1024];

	snprintf(buf, sizeof(buf), "%s: %s:%ld: ", command, fcn, line);
	buf[sizeof(buf)-1] = '\0';
	vsnprintf(buf + strlen(buf), sizeof(buf)-strlen(buf), fmt, ap);
	buf[sizeof(buf)-1] = '\0';
	syslog(prio, "%s", buf);
}

void info_(const char *fcn, long line, const char *fmt, ...)
{
	va_list ap;

	va_start(ap, fmt);
	if (use_syslog) {
		syslog_(LOG_INFO, fcn, line, fmt, ap);
	} else {
		fprintf(stdout, "%s: %s:%ld: ", command, fcn, line);
		vfprintf(stdout, fmt, ap);
		putc('\n', stdout);
	}
	va_end(ap);
}

void error_(const char *fcn, long line, const char *fmt, ...)
{
	va_list ap;

	va_start(ap, fmt);
	if (use_syslog) {
		syslog_(LOG_ERR, fcn, line, fmt, ap);
	} else {
		fprintf(stderr, "%s: %s:%ld: ", command, fcn, line);
		vfprintf(stderr, fmt, ap);
		putc('\n', stderr);
	}
	va_end(ap);
}

void cerror_(const char *fcn, long line, int cond, const char *fmt, ...)
{
	va_list ap;

	if (!cond && !debugflag)
		return;
	va_start(ap, fmt);
	if (use_syslog) {
		syslog_(LOG_ERR, fcn, line, fmt, ap);
	} else {
		fprintf(stderr, "%s: %s:%ld: ", command, fcn, line);
		vfprintf(stderr, fmt, ap);
		putc('\n', stderr);
	}
	va_end(ap);
}

void dbg_(const char *fcn, long line, const char *fmt, ...)
{
	va_list ap;

	if (!debugflag)
		return;
	va_start(ap, fmt);
	if (use_syslog) {
		syslog_(LOG_DEBUG, fcn, line, fmt, ap);
	} else {
		fprintf(stderr, "%s: %s:%ld: ", command, fcn, line);
		vfprintf(stderr, fmt, ap);
		putc('\n', stderr);
	}
	va_end(ap);
}

void error_handler(const char *file, int line, const char *function, int err, const char *fmt, ...)
{
	char buf[2048];
	va_list arg;

	va_start(arg, fmt);
	vsnprintf(buf, sizeof(buf), fmt, arg);
	va_end(arg);
	if (use_syslog)
		syslog(LOG_ERR, "alsa-lib %s:%i:(%s) %s%s%s\n", file, line, function,
				buf, err ? ": " : "", err ? snd_strerror(err) : "");
	else
		fprintf(stderr, "alsa-lib %s:%i:(%s) %s%s%s\n", file, line, function,
				buf, err ? ": " : "", err ? snd_strerror(err) : "");
}

int load_configuration(const char *file, snd_config_t **top, int *open_failed)
{
	snd_config_t *config;
	snd_input_t *in;
	int err, stdio_flag, lock_fd = -EINVAL;

	*top = NULL;
	if (open_failed)
		*open_failed = 0;
	err = snd_config_top(&config);
	if (err < 0) {
		error("snd_config_top error: %s", snd_strerror(err));
		return err;
	}
	stdio_flag = !strcmp(file, "-");
	if (stdio_flag) {
		err = snd_input_stdio_attach(&in, stdin, 0);
	} else {
		lock_fd = state_lock(file, 10);
		err = lock_fd >= 0 ? snd_input_stdio_open(&in, file, "r") : lock_fd;
	}
	if (err < 0) {
		if (open_failed)
			*open_failed = 1;
		goto out;
	}
	err = snd_config_load(config, in);
	snd_input_close(in);
	if (err < 0) {
		error("snd_config_load error: %s", snd_strerror(err));
out:
		if (lock_fd >= 0)
			state_unlock(lock_fd, file);
		snd_config_delete(config);
		snd_config_update_free_global();
		return err;
	} else {
		if (lock_fd >= 0)
			state_unlock(lock_fd, file);
		*top = config;
		return 0;
	}
}

void snd_card_iterator_init(struct snd_card_iterator *iter, int cardno)
{
	iter->card = cardno;
	iter->single = cardno >= 0;
	iter->first = true;
	iter->name[0] = '\0';
}

int snd_card_iterator_sinit(struct snd_card_iterator *iter, const char *cardname)
{
	int cardno = -1;

	if (cardname) {
		if (strncmp(cardname, "hw:", 3) == 0)
			cardname += 3;
		cardno = snd_card_get_index(cardname);
		if (cardno < 0) {
			error("Cannot find soundcard '%s'...", cardname);
			return cardno;
		}
	}
	snd_card_iterator_init(iter, cardno);
	return 0;
}

const char *snd_card_iterator_next(struct snd_card_iterator *iter)
{
	if (iter->single) {
		if (iter->first) {
			iter->first = false;
			goto retval;
		}
		return NULL;
	}
	if (snd_card_next(&iter->card) < 0) {
		if (!ignore_nocards && iter->first)
			error("No soundcards found...");
		return NULL;
	}
	iter->first = false;
	if (iter->card < 0)
		return NULL;
retval:
	snprintf(iter->name, sizeof(iter->name), "hw:%d", iter->card);

	return (const char *)iter->name;
}

int snd_card_iterator_error(struct snd_card_iterator *iter)
{
	return iter->first ? (ignore_nocards ? 0 : -ENODEV) : 0;
}

static int cleanup_filename_filter(const struct dirent *dirent)
{
	size_t flen;

	if (dirent == NULL)
		return 0;
	if (dirent->d_type == DT_DIR)
		return 0;

	flen = strlen(dirent->d_name);
	if (flen <= 5)
		return 0;

	if (strncmp(&dirent->d_name[flen-5], ".conf", 5) == 0)
		return 1;

	return 0;
}

int snd_card_clean_cfgdir(const char *cfgdir, int cardno)
{
	char path[PATH_MAX];
	struct dirent **list;
	int lasterr = 0, n, j;

	snprintf(path, sizeof(path), "%s/card%d.conf.d", cfgdir, cardno);
	n = scandir(path, &list, cleanup_filename_filter, NULL);
	if (n < 0) {
		if (errno == ENOENT)
			return 0;
		return -errno;
	}
	for (j = 0; j < n; j++) {
		snprintf(path, sizeof(path), "%s/card%d.conf.d/%s", cfgdir, cardno, list[j]->d_name);
		if (remove(path)) {
			error("Unable to remove file '%s'", path);
			lasterr = -errno;
		}
	}

	return lasterr;
}
