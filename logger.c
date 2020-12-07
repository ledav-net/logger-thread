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

#include <stdatomic.h>
#include <pthread.h>
#include <stdbool.h>
#include <string.h>
#include <stdarg.h>
#include <stdlib.h>
#include <unistd.h>
#include <limits.h>
#include <stdio.h>
#include <errno.h>
#include <time.h>

#include "logger.h"
#include "logger-thread.h"

// Uncomment to strip all the debug lines for this source.
//#define fprintf

logger_t logger;

logger_write_queue_t *logger_alloc_write_queue(pthread_t thread, int lines_max)
{
    if (logger.queues_nr == logger.queues_max) {
        return errno = ENOBUFS, NULL;
    }
    logger_write_queue_t *wrq = calloc(1, sizeof(logger_write_queue_t));
    wrq->lines = calloc(lines_max, sizeof(logger_line_t));
    wrq->lines_nr = lines_max;
    wrq->thread = thread;

    /* Ensure this is done atomically between writers. Reader is safe. */
    pthread_mutex_lock(&logger.queues_mx);
    wrq->queue_idx = logger.queues_nr;
    logger.queues[logger.queues_nr++] = wrq;
    pthread_mutex_unlock(&logger.queues_mx);

    /* Let the logger thread take this change into account when he can ... */
    atomic_compare_exchange_strong(&logger.reload, &(atomic_int){ 0 }, 1);
    return wrq;
}

int logger_init(unsigned int queues_max, logger_opts_t options)
{
    memset(&logger, 0, sizeof(logger_t));

    pthread_mutex_init(&logger.queues_mx, NULL);

    logger.queues = calloc(queues_max, sizeof(logger_write_queue_t *));
    logger.queues_max = queues_max;
    logger.options = options;

    /* Reader thread */
    pthread_create(&logger.reader_thread, NULL, (void *)_thread_logger, NULL);
    pthread_setname_np(logger.reader_thread, "logger-reader");
    return 0;
}

void logger_deinit(void)
{
    /* Sync with the logger & force him to double-check the queues */
    while (!logger.waiting) {
        fprintf(stderr, "Waiting for logger ...\n");
        usleep(100);
    }
    logger.terminate = true;
    atomic_store(&logger.waiting, 0);
    int r = futex_wake(&logger.waiting, 1);
    if (r <= 0) {
        fprintf(stderr, "Logger already woke up ?! (r=%d, %m)\n", r);
    }
    fprintf(stderr, "Joining logger ...\n");
    pthread_join(logger.reader_thread, NULL);

    int total = 0;
    for (int i = 0; i < logger.queues_nr; i++) {
        total += sizeof(logger_write_queue_t) + logger.queues[i]->lines_nr * sizeof(logger_line_t);
    }
    fprintf(stderr, "total memory allocated for the queues = %d kb\n", total/1024);

    for (int i=0 ; i<logger.queues_nr; i++) {
        free(logger.queues[i]->lines);
        free(logger.queues[i]);
    }
    free(logger.queues);
}

logger_write_queue_t *logger_get_write_queue(int lines_max)
{
    static _Thread_local logger_write_queue_t *wrq = NULL; /* Local thread variable */

    if (wrq && !atomic_load(&wrq->free)) {
        return wrq;
    }

    if (lines_max <= 0) {
        /* Caller don't want a specific size... */
        lines_max = logger.default_lines_nr;
    }
    logger_write_queue_t **queue = logger.queues;
    pthread_t th = pthread_self();
    int last_lines_nr;
retry:
    /* Searching first for a free queue previously allocated */
    last_lines_nr = INT_MAX;
    wrq = NULL;
    for (int i=0; i < logger.queues_nr; i++) {
        if (!atomic_load(&queue[i]->free)) {
            continue;
        }
        /* Find the best free queue ... */
        int lines_nr = queue[i]->lines_nr;
        if (lines_max <= lines_nr && lines_nr < last_lines_nr) {
                last_lines_nr = lines_nr;
                wrq = queue[i];
        }
    }
    if (wrq) {
        if (!atomic_compare_exchange_strong(&wrq->free, &(atomic_int){ 1 }, 0)) {
            /* Race condition, another thread took it right before us. Trying another one */
            goto retry;
        }
        wrq->thread = th;
        fprintf(stderr, "W%02d! Reusing queue %d: lines_max[%d] queue_nr[%d]\n",
                        wrq->thread_idx, wrq->queue_idx, lines_max, wrq->lines_nr);
    } else {
        /* No free queue that fits our needs... Adding a new one. */
        wrq = logger_alloc_write_queue(th, lines_max);
        if (wrq) {
            fprintf(stderr,
                "W%02d! New queue allocated: %d = %d x %d bytes (%d kb allocated)\n",
                wrq->thread_idx ?: -1, wrq->queue_idx, lines_max, sizeof(logger_line_t),
                (lines_max * sizeof(logger_line_t)) >> 10);
        }
    }
    return wrq;
}

int logger_free_write_queue(logger_write_queue_t *wrq)
{
    if (!wrq) {
        wrq = logger_get_write_queue(-1);
    }
    wrq->thread = 0;
    atomic_store(&wrq->free, 1);
    return 0;
}

int logger_printf(logger_write_queue_t *wrq, logger_line_level_t level,
        const char *src,
        const char *func,
        unsigned int line,
        const char *format, ...)
{
    va_list ap;
    int th, index;
    logger_line_t *l;

    if (logger.terminate) {
        return errno = ESHUTDOWN, -1;
    }
    if (!wrq) {
        wrq = logger_get_write_queue(-1);
    }
    th = wrq->thread_idx;

reindex:
    index = wrq->wr_seq % wrq->lines_nr;
    l = &wrq->lines[index];

    while (l->ready) {
        fprintf(stderr, "W%02d! Queue full ... (%d)\n", th, wrq->queue_idx);
        if (atomic_compare_exchange_strong(&logger.waiting, &(atomic_int){ 1 }, 0)) {
            /* Wake-up lazy guy, there is something to do ! */
            fprintf(stderr, "W%02d! Waking up the logger ...\n", th);
            if (futex_wake(&logger.waiting, 1) < 0) { /* (the only) 1 waiter to wakeup  */
                fprintf(stderr, "W%02d! ERROR: %m !\n", th);
                return -1;
            }
            usleep(1); // Let a chance to the logger to empty at least a cell before giving up...
            continue;
        }
        if (logger.options & LOGGER_OPT_NONBLOCK) {
            wrq->lost++;
            fprintf(stderr, "W%02d! Line dropped (%d %s) !\n", th, wrq->lost,
                    logger.options & LOGGER_OPT_PRINTLOST ? "since last print" : "so far");
            return errno = EAGAIN, -1;
        }
        usleep(50);
    }
    if (wrq->lost && logger.options & LOGGER_OPT_PRINTLOST) {
        int lost = wrq->lost;
        wrq->lost_total += lost;
        wrq->lost = 0;

        logger_printf(wrq, LOGGER_LEVEL_OOPS, __FILE__, __FUNCTION__, __LINE__,
            "Lost %d log line(s) (%d so far) !", lost, wrq->lost_total);

        goto reindex;
    }
    va_start(ap, format);

    clock_gettime(CLOCK_REALTIME, &l->ts);
    l->level = level;
    l->file = src;
    l->func = func;
    l->line = line;
    vsprintf(l->str, format, ap);

    fprintf(stderr, "W%02d> '%s' (%d)\n", th, l->str, wrq->queue_idx);

    l->ready = true;
    wrq->wr_seq++;

    if (atomic_compare_exchange_strong(&logger.waiting, &(atomic_int){ 1 }, 0)) {
        /* Wake-up lazy guy, there is something to do ! */
        fprintf(stderr, "W%02d! Waking up the logger ...\n", th);
        if (futex_wake(&logger.waiting, 1) < 0) { /* (the only) 1 waiter to wakeup  */
            return -1;
        }
    }
    return 0;
}
