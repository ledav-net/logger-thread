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

logger_t *stdlogger = NULL;

logger_write_queue_t *logger_alloc_write_queue(logger_t *q, pthread_t thread, int lines_max)
{
    if (q->queues_nr == q->queues_max) {
        return errno = ENOBUFS, NULL;
    }
    logger_write_queue_t *wrq = calloc(1, sizeof(logger_write_queue_t));
    wrq->lines = calloc(lines_max, sizeof(logger_line_t));
    wrq->lines_nr = lines_max;
    wrq->thread = thread;
    wrq->logger = q;

    fprintf(stderr, "W..! logger_line_t = %d x %d bytes (%d kb allocated)\n",
                    lines_max, sizeof(logger_line_t), sizeof(logger_line_t) * lines_max / 1024);

    /* Ensure this is done atomically between writers. Reader is safe. */
    pthread_mutex_lock(&q->queues_lk);
    wrq->queue_idx = q->queues_nr;
    q->queues[q->queues_nr++] = wrq;
    pthread_mutex_unlock(&q->queues_lk);

    /* Let the logger thread take this change into account when he can ... */
    atomic_compare_exchange_strong(&q->reload, &(atomic_int){ 0 }, 1);
    return wrq;
}

logger_t *logger_init(unsigned int queues_max, logger_opts_t options)
{
    logger_t *q;

    q = calloc(1, sizeof(logger_t));
    q->queues = calloc(queues_max, sizeof(logger_write_queue_t *));
    q->queues_max = queues_max;
    q->options = options;

    /* Reader thread */
    pthread_create(&q->reader_thread, NULL, (void *)_thread_logger, (void *)q);
    pthread_setname_np(q->reader_thread, "logger-reader");
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
    int total = 0;
    for (int i = 0; i < q->queues_nr; i++) {
        total += sizeof(logger_write_queue_t) + q->queues[i]->lines_nr * sizeof(logger_line_t);
    }
    fprintf(stderr, "total memory allocated for the queues = %d kb\n", total/1024);

    for (int i=0 ; i<q->queues_nr; i++) {
        free(q->queues[i]->lines);
        free(q->queues[i]);
    }
    free(q->queues);
    free(q);
}

logger_write_queue_t *logger_get_write_queue(logger_t *logger, int lines_max)
{
    if (!logger) {
        return errno = EBADF, NULL;
    }
    static _Thread_local logger_write_queue_t *wrq = NULL; /* Local thread variable */

    if (wrq && !atomic_load(&wrq->free)) {
        /* Return the queue actually in use by this thread */
        fprintf(stderr, "W%02d! logger_get_write_queue(0x%p, %d) = 0x%p (%d)\n",
                        wrq->thread_idx, logger, lines_max, wrq, wrq->queue_idx);
        return wrq;
    }
    if (lines_max <= 0) {
        /* Caller don't want a specific size... */
        lines_max = logger->default_lines_nr;
    }
    logger_write_queue_t **queue = logger->queues;
    pthread_t th = pthread_self();
    int last_lines_nr;
retry:
    /* Searching first for a free queue previously allocated */
    last_lines_nr = INT_MAX;
    wrq = NULL;
    for (int i=0; i < logger->queues_nr; i++) {
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
    } else {
        /* No free queue that fits our needs... Adding a new one. */
        wrq = logger_alloc_write_queue(logger, th, lines_max);
    }
    fprintf(stderr, "W%02d! logger_get_write_queue(0x%p, %d) = 0x%p (%d)\n",
                    wrq->thread_idx, logger, lines_max, wrq, wrq->queue_idx);
    return wrq;
}

int logger_free_write_queue(logger_t *logger, logger_write_queue_t *wrq)
{
    if (!wrq) {
        wrq = logger_get_write_queue(logger, -1);
    }
    wrq->thread = 0;
    atomic_store(&wrq->free, 1);
    return 0;
}

int logger_printf(logger_t *logger, logger_write_queue_t *wrq, logger_line_level_t level,
        const char *src,
        const char *func,
        unsigned int line,
        const char *format, ...)
{
    va_list ap;
    int th, index;
    logger_line_t *l;

    if (logger->terminate) {
        fprintf(stderr, "Queue is closed !\n");
        return errno = ESHUTDOWN, -1;
    }
    if (!wrq) {
        wrq = logger_get_write_queue(logger, -1);
    }
    th = wrq->thread_idx;

reindex:
    index = wrq->wr_seq % wrq->lines_nr;
    l = &wrq->lines[index];

    while (l->ready) {
        fprintf(stderr, "W%02d! Queue full ... (%d)\n", th, wrq->queue_idx);
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
            wrq->lost++;
            fprintf(stderr, "W%02d! Line dropped (%d %s) !\n", th, wrq->lost,
                    logger->options & LOGGER_OPT_PRINTLOST ? "since last print" : "so far");
            return errno = EAGAIN, -1;
        }
        usleep(50);
    }
    if (wrq->lost && logger->options & LOGGER_OPT_PRINTLOST) {
        int lost = wrq->lost;
        wrq->lost_total += lost;
        wrq->lost = 0;

        logger_printf(logger, wrq, LOGGER_LEVEL_OOPS, __FILE__, __FUNCTION__, __LINE__,
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

    if (atomic_compare_exchange_strong(&logger->waiting, &(atomic_int){ 1 }, 0)) {
        /* Wake-up lazy guy, there is something to do ! */
        fprintf(stderr, "W%02d! Waking up the logger ...\n", th);
        if (futex_wake(&logger->waiting, 1) < 0) { /* (the only) 1 waiter to wakeup  */
            return -1;
        }
    }
    return 0;
}
