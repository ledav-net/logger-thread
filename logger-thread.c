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

#define _GNU_SOURCE

#include <linux/futex.h>
#include <sys/syscall.h>
#include <sys/time.h>
#include <stdatomic.h>
#include <pthread.h>
#include <stdbool.h>
#include <unistd.h>
#include <string.h>
#include <stdarg.h>
#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <time.h>

#include "logger.h"

#define NTOS(v) ((v)/1000000000) /* nSec -> Sec  */
#define STON(v) ((v)*1000000000) /*  Sec -> nSec */

#define MTON(v) ((v)*1000000)    /* mSec -> nSec */
#define MTOU(v) ((v)*1000)       /* mSec -> uSec */

#define NTOM(v) ((v)/1000000)    /* nSec -> mSec */
#define NTOU(v) ((v)/1000)       /* nSec -> XSec */

#define timespec_to_ns(a) ((STON((a).tv_sec) + (a).tv_nsec))
#define elapsed_ns(b,a) (timespec_to_ns(a) - timespec_to_ns(b))

typedef struct {
    unsigned long         ts;  /* Key to sort on (ts of current line) */
    logger_write_queue_t *wrq; /* Related write queue */
} _logger_fuse_entry_t;

const char *logger_level_label[LOGGER_LEVEL_COUNT] = {
    [LOGGER_LEVEL_EMERG]    = "EMERG ",
    [LOGGER_LEVEL_ALERT]    = "ALERT ",
    [LOGGER_LEVEL_CRITICAL] = "CRITIC",
    [LOGGER_LEVEL_ERROR]    = "ERROR ",
    [LOGGER_LEVEL_WARNING]  = "WARN  ",
    [LOGGER_LEVEL_NOTICE]   = "NOTICE",
    [LOGGER_LEVEL_INFO]     = "INFO  ",
    [LOGGER_LEVEL_DEBUG]    = "DEBUG ",
    [LOGGER_LEVEL_OKAY]     = "OKAY  ",
    [LOGGER_LEVEL_TRACE]    = "TRACE ",
    [LOGGER_LEVEL_OOPS]     = "OOPS  ",
};

logger_t *stdlogger = NULL;

// Uncomment to strip all the debug lines for this source.
//#define fprintf

#define futex_wait(addr, val)		_futex((addr), FUTEX_WAIT_PRIVATE, (val), NULL)
#define futex_timed_wait(addr, val, ts)	_futex((addr), FUTEX_WAIT_PRIVATE, (val), (ts))
#define futex_wake(addr, val)		_futex((addr), FUTEX_WAKE_PRIVATE, (val), NULL)

static inline int _futex(atomic_int *uaddr, int futex_op, int val, struct timespec *tv)
{
    /* Note: not using the last 2 parameters uaddr2 & val2 (see man futex(2)) */
    return syscall(SYS_futex, uaddr, futex_op, val, tv);
}

static const char *_logger_get_date(unsigned long sec)
{
    static char date[32];
    static unsigned long prev_sec = 0;

    if (sec - prev_sec >= 60*60*24) {
        struct tm tm;
        localtime_r(&sec, &tm);
        strftime(date, sizeof(date), "-- %Y-%m-%d --\n", &tm);
        prev_sec = sec;
        return date;
    }
    return "";
}

static const char *_logger_get_time(unsigned long sec)
{
    static char time[8];
    static unsigned long prev_sec = 0;

    if (sec - prev_sec >= 60) {
        struct tm tm;
        localtime_r(&sec, &tm);
        strftime(time, sizeof(time), "%H:%M", &tm);
        prev_sec = sec;
    }
    return time;
}

static int _logger_write_line(logger_opts_t options, logger_line_t *l)
{
    char linestr[LOGGER_LINE_SZ+256];

    char src_str[128], *b = src_str;
    int len = snprintf(src_str, sizeof(src_str), "%s:%s:%d", l->file, l->func, l->line);
    if ( len > 40 ) {
        b += len - 40;
    }

    unsigned long usec = NTOU(l->ts.tv_nsec) % 1000;
    unsigned long msec = NTOM(l->ts.tv_nsec) % 1000;
    int sec            = l->ts.tv_sec % 60;

    len = snprintf(linestr, sizeof(linestr),
            "%s%s:%02d.%03lu,%03lu [%s] %40s> %s\n",
            _logger_get_date(l->ts.tv_sec),
            _logger_get_time(l->ts.tv_sec),
            sec, msec, usec,
            logger_level_label[l->level],
            b, l->str);

    return write(1, linestr, len);
}

static inline void _bubble_fuse_up(_logger_fuse_entry_t *fuse, int fuse_nr)
{
    if (fuse_nr > 1 && fuse[0].ts > fuse[1].ts) {
        register int i = 0;
        _logger_fuse_entry_t newentry = fuse[0]; // Strictly bigger move up (stack empty ones at the end of smallers)
        do { fuse[i] = fuse[i+1]; i++; } while (i < fuse_nr-1 && newentry.ts > fuse[i+1].ts);
        fuse[i] = newentry;
    }
}

static inline void _bubble_fuse_down(_logger_fuse_entry_t *fuse, int fuse_nr)
{
    if (fuse_nr > 1 && fuse[fuse_nr-1].ts <= fuse[fuse_nr-2].ts) {
        register int i = fuse_nr-1;
        _logger_fuse_entry_t newentry = fuse[i]; // Smaller & same move down (so, stack empty ones at top of smallers)
        do { fuse[i] = fuse[i-1]; i--; } while (i > 0 && newentry.ts <= fuse[i-1].ts);
        fuse[i] = newentry;
    }
}

static inline int _logger_set_queue_entry(const logger_write_queue_t *wrq, _logger_fuse_entry_t *fuse)
{
    unsigned int index = wrq->rd_idx;

    if (wrq->lines[index].ready) { // when it's ready, we can proceed...
        struct timespec ts = wrq->lines[index].ts;
        fuse->ts = timespec_to_ns(ts);
    } else {
        fuse->ts = ~0; // Otherwise it's empty.
        return 1;
    }
    return 0;
}

static inline int _logger_init_lines_queue(const logger_t *q, _logger_fuse_entry_t *fuse, int fuse_nr)
{
    memset(fuse, 0, fuse_nr * sizeof(_logger_fuse_entry_t));

    for (int i=0 ; i<fuse_nr; i++) {
        logger_write_queue_t *wrq = q->queues[i];

        fuse[i].ts  = ~0; // Init all the queues as if they were empty
        fuse[i].wrq = wrq;
    }
    return fuse_nr; // Number of empty queues (all)
}

static int _logger_enqueue_next_lines(_logger_fuse_entry_t *fuse, int fuse_nr, int empty_nr)
{
    logger_write_queue_t *wrq;

    if (fuse[0].ts != ~0) { // This one should have been processed. Freeing it ...
        wrq = fuse[0].wrq;

        wrq->lines[wrq->rd_idx].ready = false; // Free this line for the writer thread
        wrq->rd_idx = ++wrq->rd_seq % wrq->lines_nr;
        empty_nr += _logger_set_queue_entry(wrq, &fuse[0]); // Enqueue the next line

        _bubble_fuse_up(fuse, fuse_nr); /* Let him find it's place */
    }
    /* Let see if there is something new in the empty queues... */
    int rv = 0;
    for (int i=0, last=fuse_nr-1; i<empty_nr; i++) {
        rv += _logger_set_queue_entry(fuse[last].wrq, &fuse[last]);
        _bubble_fuse_down(fuse, fuse_nr);
    }
    return rv; // return the number of remaining empty queues ...
}

static void *_thread_logger(logger_t *q)
{
    logger_write_queue_t *wrq;
    int empty_nr = 0;
    int really_empty = 0;
    _logger_fuse_entry_t fuse_queue[q->queues_nr];

    fprintf(stderr, "_logger_fuse_entry_t = %d (%d)\n",
                    sizeof(_logger_fuse_entry_t), sizeof(fuse_queue));

    empty_nr = _logger_init_lines_queue(q, fuse_queue, q->queues_nr);

    while (1) {
        empty_nr = _logger_enqueue_next_lines(fuse_queue, q->queues_nr, empty_nr);

        if (fuse_queue[0].ts == ~0) {
            q->empty = true;
            if (q->terminate) {
                break;
            }
            if (really_empty < 5) {
                fprintf(stderr, "RDR! Print queue empty. Double check in 0.2 ms ...\n");
                really_empty++;
                usleep(200);
                /* Double-check (5 times) if the queue is really empty in 0.2 ms */
                /* This avoid the writers to wakeup too frequently the reader in case of burst */
                continue;
            }
            really_empty = 0;
            fprintf(stderr, "RDR! Print queue REALLY empty ... Zzz\n");
            atomic_store(&q->waiting, 1);
            if (futex_wait(&q->waiting, 1) < 0 && errno != EAGAIN) {
                fprintf(stderr, "RDR! ERROR: %m !\n");
                break;
            }
            continue;
        }
        q->empty = false;
        really_empty = 0;

        logger_write_queue_t *wrq = fuse_queue[0].wrq;
        int rv = _logger_write_line(q->options, &wrq->lines[wrq->rd_idx]);
        if (rv < 0) {
            fprintf(stderr, "RDR: logger_write_line(): %m\n");
            break;
        }
    }
    fprintf(stderr, "RDR! Exit\n");
    return NULL;
}

logger_t *logger_init(unsigned int write_queues_max, unsigned int lines_max, logger_opts_t options)
{
    logger_t *q;

    if (!write_queues_max) {
        return errno = EINVAL, NULL; // No writers, nothing to do ...
    }
    q = calloc(1, sizeof(logger_t));
    q->queues = calloc(write_queues_max, sizeof(logger_write_queue_t *));
    q->queues_nr = write_queues_max;
    q->options = options;

    for (int i=0 ; i < write_queues_max ; i++) {
        logger_write_queue_t *wrq = calloc(1, sizeof(logger_write_queue_t));
        wrq->lines = calloc(lines_max, sizeof(logger_line_t));
        wrq->lines_nr = lines_max;
        wrq->queue_idx = i;
        wrq->logger = q;
        q->queues[i] = wrq;
    }
    /* Reader thread */
    pthread_create(&q->reader_thread, NULL, (void *)_thread_logger, (void *)q);
    pthread_setname_np(q->reader_thread, "logger-reader");

    fprintf(stderr, "logger_line_t = %d (%d)\n",
                    sizeof(logger_line_t), sizeof(logger_line_t) * lines_max);
    return q;
}

void logger_deinit(logger_t *_q)
{
    logger_t *q = _q ?: stdlogger;

    /* Sync with the logger & force him to double-check the queues */
    while (!q->waiting) {
        fprintf(stderr, "Waiting for logger ...\n");
        usleep(100);
    }
    q->terminate = true;
    atomic_store(&q->waiting, 0);
    if (futex_wake(&q->waiting, 1) <= 0) {
        fprintf(stderr, "Logger did not woke up (%m) !\n");
    } else {
        fprintf(stderr, "Joining logger ...\n");
        pthread_join(q->reader_thread, NULL);
    }
    for (int i=0 ; i<q->queues_nr; i++) {
        free(q->queues[i]->lines);
    }
    free(q->queues);
    free(q);
}

logger_write_queue_t *logger_get_write_queue(logger_t *logger)
{
    if (!logger) {
        return errno = EBADF, NULL;
    }
    pthread_t th = pthread_self();

    for (int i=0; i<stdlogger->queues_nr; i++) {
        if (th == stdlogger->queues[i]->thread) {
            return stdlogger->queues[i];
        }
    }
    return errno = ECHILD, NULL;
}

int logger_printf(logger_t *logger, logger_write_queue_t *wrq, logger_line_level_t level,
        const char *src,
        const char *func,
        unsigned int line,
        const char *format, ...)
{
    va_list ap;

    if (logger->terminate) {
        fprintf(stderr, "Queue is closed !\n");
        return errno = ESHUTDOWN, -1;
    }
    if (!wrq) {
        wrq = logger_get_write_queue(logger);
    }
    int index = wrq->wr_seq % wrq->lines_nr;
    int th = wrq->queue_idx;
    logger_line_t *l = &wrq->lines[index];

    while (l->ready) {
        fprintf(stderr, "W%02d! Queue full ...\n", th);
        if (atomic_compare_exchange_strong(&logger->waiting, &(atomic_int){ 1 }, 0)) {
            /* Wake-up lazy guy, there is something to do ! */
            fprintf(stderr, "W%02d! Waking up the logger ...\n", th);
            if (futex_wake(&logger->waiting, 1) < 0) { /* (the only) 1 waiter to wakeup  */
                fprintf(stderr, "W%02d! ERROR: %m !\n", th);
                return -1;
            }
            usleep(1); // Let a chance to the logger to empty at least a cell before giving up...
            continue;
        }
        if (logger->options & LOGGER_OPT_NONBLOCK) {
            fprintf(stderr, "W%02d! Line dropped (%d so far) !\n", th, ++wrq->lost);
            return errno = EAGAIN, -1;
        }
        usleep(50);
    }
    va_start(ap, format);

    clock_gettime(CLOCK_REALTIME, &l->ts);
    l->level = level;
    l->file = src;
    l->func = func;
    l->line = line;
    vsprintf(l->str, format, ap);

    fprintf(stderr, "W%02d> '%s'\n", th, l->str);

    l->ready = true;
    wrq->wr_seq++;

    if (atomic_compare_exchange_strong(&logger->waiting, &(atomic_int){ 1 }, 0)) {
        /* Wake-up lazy guy, there is something to do ! */
        fprintf(stderr, "W%02d! Waking up the logger ...\n", th);
        if (futex_wake(&logger->waiting, 1) < 0) { /* (the only) 1 waiter to wakeup  */
            return -1;
        }
    }
    return 0;
}
