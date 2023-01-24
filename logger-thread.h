#pragma once
/*
 * Copyright (C) 2020 David De Grave <david@ledav.net>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, version 3.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 */
#ifndef _LOGGER_THREAD_H
#define _LOGGER_THREAD_H

#include <sys/syscall.h>
#include <linux/futex.h>
#include <stdatomic.h>
#include <unistd.h>
#include <time.h>

#ifdef __cplusplus
extern "C" {
#endif

#include "logger.h"

#define NTOS(v) ((v)/1000000000) /* nSec -> Sec  */
#define STON(v) ((v)*1000000000) /*  Sec -> nSec */

#define MTON(v) ((v)*1000000)    /* mSec -> nSec */
#define MTOU(v) ((v)*1000)       /* mSec -> uSec */

#define NTOM(v) ((v)/1000000)    /* nSec -> mSec */
#define NTOU(v) ((v)/1000)       /* nSec -> XSec */

#define futex_wait(addr, val)		_futex((addr), FUTEX_WAIT_PRIVATE, (val), NULL)
#define futex_timed_wait(addr, val, ts)	_futex((addr), FUTEX_WAIT_PRIVATE, (val), (ts))
#define futex_wake(addr, val)		_futex((addr), FUTEX_WAKE_PRIVATE, (val), NULL)

inline int _futex(atomic_int *uaddr, int futex_op, int val, struct timespec *tv)
{
    /* Note: not using the last 2 parameters uaddr2 & val2 (see man futex(2)) */
    return syscall(SYS_futex, uaddr, futex_op, val, tv);
}

extern void * _thread_logger(void);

#ifdef __cplusplus
}
#endif
#endif // _LOGGER_THREAD_H
