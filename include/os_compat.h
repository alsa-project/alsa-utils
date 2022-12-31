/*
 * ALSA lib - compatibility header for supporting various OSes
 * Copyright (C) 2022 by Takayoshi SASANO <uaa@cvs.openbsd.org>
 *
 *
 *   This library is free software; you can redistribute it and/or modify
 *   it under the terms of the GNU Lesser General Public License as
 *   published by the Free Software Foundation; either version 2.1 of
 *   the License, or (at your option) any later version.
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU Lesser General Public License for more details.
 *
 *   You should have received a copy of the GNU Lesser General Public
 *   License along with this library; if not, write to the Free Software
 *   Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 *
 */

#ifndef __OS_COMPAT_H
#define __OS_COMPAT_H

#ifndef ESTRPIPE
#define ESTRPIPE ESPIPE
#endif

#ifndef ERESTART
#define ERESTART EINTR
#endif

#ifndef SCHED_IDLE
#define SCHED_IDLE SCHED_OTHER
#endif

#if defined(__OpenBSD__)
/* these functions in <sched.h> are not implemented */
#define sched_getparam(pid, param) (-1)
#define sched_setscheduler(pid, policy, param) (-1)
#endif

#endif
