/*
 *  Advanced Linux Sound Architecture Control Program
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

#include "aconfig.h"
#include "version.h"
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <string.h>
#include <signal.h>
#include <sys/time.h>
#include <sys/stat.h>
#include "alsactl.h"
#include "os_compat.h"

#define PATH_SIZE 512

static int alarm_flag;

static void signal_handler_alarm(int sig)
{
	alarm_flag = 1;
}

static int state_lock_(const char *lock_file, int lock, int timeout, int _fd)
{
	int fd = -1, err = 0;
	struct flock lck;
	struct stat st;
	char lcktxt[14];
	struct sigaction sig_alarm, sig_alarm_orig;
	struct itimerval itv;

	if (do_lock <= 0)
		return 0;

	lck.l_type = lock ? F_WRLCK : F_UNLCK;
	lck.l_whence = SEEK_SET;
	lck.l_start = 0;
	lck.l_len = 11;
	lck.l_pid = 0;
	if (lock) {
		snprintf(lcktxt, sizeof(lcktxt), "%10li\n", (long)getpid());
	} else {
		snprintf(lcktxt, sizeof(lcktxt), "%10s\n", "");
		fd = _fd;
	}
	while (fd < 0 && timeout-- > 0) {
		fd = open(lock_file, O_RDWR);
		if (!lock && fd < 0) {
			err = -EIO;
			goto out;
		}
		if (fd < 0) {
			fd = open(lock_file, O_RDWR|O_CREAT|O_EXCL, 0644);
			if (fd < 0) {
				if (errno == EBUSY || errno == EAGAIN) {
					sleep(1);
					continue;
				}
				if (errno == EEXIST) {
					fd = open(lock_file, O_RDWR);
					if (fd >= 0)
						break;
				}
				err = -errno;
				goto out;
			}
		}
	}
	if (fd < 0 && timeout <= 0) {
		err = -EBUSY;
		goto out;
	}
	if (fstat(fd, &st) < 0) {
		err = -errno;
		goto out;
	}
	if (st.st_size != 11 || !lock) {
		if (write(fd, lcktxt, 11) != 11) {
			err = -EIO;
			goto out;
		}
		if (lock && lseek(fd, 0, SEEK_SET)) {
			err = -errno;
			goto out;
		}
	}
	alarm_flag = 0;
	memset(&sig_alarm, 0, sizeof(sig_alarm));
	sigemptyset(&sig_alarm.sa_mask);
	sig_alarm.sa_handler = signal_handler_alarm;
	if (sigaction(SIGALRM, &sig_alarm, &sig_alarm_orig) < 0) {
		err = -ENXIO;
		goto out;
	}
	memset(&itv, 0, sizeof(itv));
	itv.it_value.tv_sec = timeout;
	if (setitimer(ITIMER_REAL, &itv, NULL) < 0) {
		err = -ENXIO;
		sigaction(SIGALRM, &sig_alarm_orig, NULL);
		goto out;
	}
	while (alarm_flag == 0) {
		if (fcntl(fd, F_SETLKW, &lck) == 0)
			break;
		if (errno == EAGAIN || errno == ERESTART)
			continue;
	}
	memset(&itv, 0, sizeof(itv));
	setitimer(ITIMER_REAL, &itv, NULL);
	sigaction(SIGALRM, &sig_alarm_orig, NULL);
	if (alarm_flag) {
		err = -EBUSY;
		goto out;
	}
	if (lock) {
		if (write(fd, lcktxt, 11) != 11) {
			err = -EIO;
			goto out;
		}
		return fd;
	}
	err = 0;

out:
	if (fd >= 0)
		close(fd);
	return err;
}

static void state_lock_file(char *buf, size_t buflen)
{
	if (lockfile[0] == '/')
		snprintf(buf, buflen, "%s", lockfile);
	else
		snprintf(buf, buflen, "%s/%s", lockpath, lockfile);
}

int state_lock(const char *file, int timeout)
{
	char fn[PATH_SIZE];
	int err;

	state_lock_file(fn, sizeof(fn));
	err = state_lock_(fn, 1, timeout, -1);
	if (err < 0)
		error("file %s lock error: %s", file, strerror(-err));
	return err;
}

int state_unlock(int _fd, const char *file)
{
	char fn[PATH_SIZE];
	int err;

	state_lock_file(fn, sizeof(fn));
	err = state_lock_(fn, 0, 10, _fd);
	if (err < 0)
		error("file %s unlock error: %s", file, strerror(-err));
	return err;
}

static void card_lock_file(char *buf, size_t buflen, int card_number)
{
	snprintf(buf, buflen, "%s/card%i.lock", lockpath, card_number);
}

int card_lock(int card_number, int timeout)
{
	char fn[PATH_SIZE];
	int err;

	card_lock_file(fn, sizeof(fn), card_number);
	err = state_lock_(fn, 1, timeout, -1);
	if (err < 0)
		error("card %d lock error: %s", card_number, strerror(-err));
	return err;
}

int card_unlock(int _fd, int card_number)
{
	char fn[PATH_SIZE];
	int err;

	card_lock_file(fn, sizeof(fn), card_number);
	err = state_lock_(fn, 0, 10, _fd);
	if (err < 0)
		error("card %d unlock error: %s", card_number, strerror(-err));
	return err;
}
