#pragma once
/*
 * SPDX-License-Identifier: MIT
 *
 * Copyright 2022 David De Grave <david@ledav.net>
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy of
 * this software and associated documentation files (the "Software"), to deal in
 * the Software without restriction, including without limitation the rights to
 * use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of
 * the Software, and to permit persons to whom the Software is furnished to do so,
 * subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS
 * FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR
 * COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER
 * IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
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
