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
#include <stdio.h>
#include <errno.h>
#include <time.h>

#include "logger.h"
#include "logger-thread.h"

// Uncomment to strip all the debug lines for this source.
//#define fprintf

logger_t *stdlogger = NULL;

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
