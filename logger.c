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

#if defined(LOGGER_USE_THREAD)

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
#define fprintf(...)

logger_t logger;

static _Thread_local logger_write_queue_t *_own_wrq = NULL; /* Local thread variable */

static void _logger_set_thread_name(logger_write_queue_t *wrq)
{
    wrq->thread = pthread_self();
    pthread_getname_np(wrq->thread, wrq->thread_name, sizeof(wrq->thread_name));
    if (!wrq->thread_name[0]) {
        snprintf(wrq->thread_name, LOGGER_MAX_THREAD_NAME_SZ, "%lu", wrq->thread);
    }
    wrq->thread_name_len = strlen(wrq->thread_name);
}

logger_write_queue_t *_logger_alloc_write_queue(int lines_max, logger_opts_t opts)
{
    if (logger.queues_nr == logger.queues_max) {
        return errno = ENOBUFS, NULL;
    }
    logger_write_queue_t *wrq = calloc(1, sizeof(logger_write_queue_t));
    wrq->lines = calloc(lines_max, sizeof(logger_line_t));
    wrq->lines_nr = lines_max;
    wrq->opts = opts;
    _logger_set_thread_name(wrq);

    if (opts & LOGGER_OPT_PREALLOC) {
        /**
         * Pre-fill the queue with something so that Linux really allocate
         * the pages.  This is due to the 'copy-on-write' logic where the
         * page is really allocated (or copied) when the process try to
         * write something.
         * Note: Didn't found a better way to do this ...
         */
        for (int i=0 ; i<lines_max ; i++) {
            wrq->lines[i].ts.tv_nsec = ~0;
            for (int j=0, k=0 ; j < sizeof(wrq->lines[i].str)/64 ; j++ ) {
                wrq->lines[i].str[j] = k++;
            }
        }
    }

    /* Ensure this is done atomically between writers. Reader is safe. */
    pthread_mutex_lock(&logger.queues_mx);
    wrq->queue_idx = logger.queues_nr;
    logger.queues[logger.queues_nr++] = wrq;
    pthread_mutex_unlock(&logger.queues_mx);

    /* Let the logger thread take this change into account when he can ... */
    atomic_compare_exchange_strong(&logger.reload, &(atomic_int){ 0 }, 1);
    return wrq;
}

int logger_init(int queues_max, int lines_max, logger_line_level_t level_min, logger_opts_t opts)
{
    memset(&logger, 0, sizeof(logger_t));

    pthread_mutex_init(&logger.queues_mx, NULL);

    logger.queues = calloc(queues_max, sizeof(logger_write_queue_t *));
    logger.queues_max = queues_max;
    logger.opts = opts;
    logger.theme = &logger_colors_default;
    logger.default_lines_nr = lines_max;
    logger.level_min = level_min;
    logger.running = true;

    _own_wrq = NULL;

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
    logger.running = false;
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
    fprintf(stderr, "total memory allocated for %d queues = %d kb\n", logger.queues_nr, total/1024);

    for (int i=0 ; i<logger.queues_nr; i++) {
        free(logger.queues[i]->lines);
        free(logger.queues[i]);
    }
    free(logger.queues);
    memset(&logger, 0, sizeof(logger_t));
}

int logger_assign_write_queue(unsigned int lines_max, logger_opts_t opts)
{
    if (_own_wrq) {
        /* If this is already set, nothing else to do ... */
        return 0;
    }
    if (!lines_max) {
        /* Caller don't want a specific size... */
        lines_max = logger.default_lines_nr;
    }
    logger_write_queue_t **queue = logger.queues;
    logger_write_queue_t *fwrq;
    int last_lines_nr;

retry:
    /* Searching first for a free queue previously allocated */
    last_lines_nr = INT_MAX;
    fwrq = NULL;
    for (int i=0; i < logger.queues_nr; i++) {
        if (!atomic_load(&queue[i]->free)) {
            continue;
        }
        /* Find the best free queue ... */
        int lines_nr = queue[i]->lines_nr;
        if (lines_max <= lines_nr && lines_nr < last_lines_nr) {
            last_lines_nr = lines_nr;
            fwrq = queue[i];
        }
    }
    if (fwrq) {
        if (!atomic_compare_exchange_strong(&fwrq->free, &(atomic_int){ 1 }, 0)) {
            /* Race condition, another thread took it right before us. Trying another one */
            fprintf(stderr, "<?> Race condition when trying to reuse queue %d ! Retrying...\n", fwrq->queue_idx);
            goto retry;
        }
        _logger_set_thread_name(fwrq);
        fwrq->opts = opts ?: logger.opts;

        fprintf(stderr, "<%s> Reusing queue %d: lines_max[%d] queue_nr[%d]\n",
                        fwrq->thread_name, fwrq->queue_idx, lines_max, fwrq->lines_nr);
    } else {
        /* No free queue that fits our needs... Adding a new one. */
        fwrq = _logger_alloc_write_queue(lines_max, opts ?: logger.opts);
        if (!fwrq) {
            return -1;
        }
        fprintf(stderr, "<%s> New queue allocated: %d = %d x %lu bytes (%lu kb allocated)\n",
                        fwrq->thread_name, fwrq->queue_idx, lines_max, sizeof(logger_line_t),
                        (lines_max * sizeof(logger_line_t)) >> 10);
    }
    _own_wrq = fwrq;
    return 0;
}

static inline int _logger_wakeup_reader_if_needed(void)
{
    if (atomic_compare_exchange_strong(&logger.waiting, &(atomic_int){ 1 }, 0)) {
        /* Wake-up lazy guy, there is something to do ! */
        fprintf(stderr, "<%s> Waking up the logger ...\n", _own_wrq->thread_name);
        if (futex_wake(&logger.waiting, 1) < 0) { /* (the only) 1 waiter to wakeup  */
            return -1;
        }
        return 1;
    }
    return 0;
}

int logger_free_write_queue(void)
{
    fprintf(stderr, "<%s> Freeing queue %d ...\n", _own_wrq->thread_name, _own_wrq->queue_idx);
    while (_own_wrq->rd_seq != _own_wrq->wr_seq) {
        if (_logger_wakeup_reader_if_needed() < 0) {
            return -1;
        }
        /* Wait for the queue to be empty before leaving ... */
        usleep(100);
    }
    atomic_store(&_own_wrq->free, 1);
    _own_wrq = NULL;
    return 0;
}

typedef struct {
    void       *(*start_routine)(void *);
    void         *arg;
    int           max_lines;
    logger_opts_t opts;
    char          thread_name[LOGGER_MAX_THREAD_NAME_SZ];
} _thread_params;

static void _logger_pthread_wrapper(_thread_params *params)
{
    pthread_cleanup_push((void *)free, (void *)params);

    pthread_setname_np(pthread_self(), params->thread_name);
    /**
     * The name of the thread is fixed at allocation time so, the
     * pthread_setname_np() call must occur before the assignation bellow.
     * If there is no name specified, thread_id is used instead.
     */
    if (logger_assign_write_queue(params->max_lines, params->opts) < 0) {
        /**
         * Oops!  If this happen, it could mean the limit of queues to
         * allocate is too low !!
         */
        pthread_exit(NULL);
    }
    /**
     * Must be called when the thread don't need it anymore.  Otherwise it
     * will stay allocated for an unexistant thread for ever !  This is also
     * true for the local threads forked by the domains themself.
     *
     * Note also that if this thread never print something, you should NOT
     * use this wrapper but better use the native pthread_create(3) instead
     * as all this is useless and reserve a log queue for nothing ...
     */
    pthread_cleanup_push((void *)logger_free_write_queue, NULL);

    /* Let run the main thread function */
    pthread_exit(params->start_routine(params->arg));

    /* I know this sounds weird but it's the way it is... pushs needs pops ! */
    pthread_cleanup_pop(true);
    pthread_cleanup_pop(true);
}

int logger_pthread_create(const char *thread_name, unsigned int max_lines, logger_opts_t opts,
    pthread_t *thread, const pthread_attr_t *attr, void *(*start_routine)(void *), void *arg)
{
    _thread_params *params = malloc(sizeof(_thread_params));

    if (!params) {
        return -1;
    }
    strncpy(params->thread_name, thread_name, sizeof(params->thread_name)-1);
    params->thread_name[sizeof(params->thread_name)-1] = 0;
    params->max_lines = max_lines;
    params->opts = opts;
    params->start_routine = start_routine;
    params->arg = arg;

    return pthread_create(thread, attr, (void *)_logger_pthread_wrapper, (void *)params);
}

int logger_printf(logger_line_level_t level,
        const char *src,
        const char *func,
        unsigned int line,
        const char *format, ...)
{
    if (!logger.running) {
        return errno = ENOTCONN, -1;
    }
    if (level > logger.level_min) {
        return 0;
    }
    if (!_own_wrq && logger_assign_write_queue(0, LOGGER_OPT_NONE) < 0) {
        return -1;
    }
    va_list ap;
    int index;
    logger_line_t *l;

reindex:
    index = _own_wrq->wr_seq % _own_wrq->lines_nr;
    l = &_own_wrq->lines[index];

    while (l->ready) {
        fprintf(stderr, "<%s> Queue full ... (%d)\n", _own_wrq->thread_name, _own_wrq->queue_idx);

        int ret = _logger_wakeup_reader_if_needed();
        if (ret > 0) {
            usleep(1); // Let a chance to the logger to empty at least a cell before giving up...
            continue;
        }
        else if (ret < 0) {
            return -1;
        }
        if (_own_wrq->opts & LOGGER_OPT_NONBLOCK) {
            _own_wrq->lost++;
            fprintf(stderr, "<%s> Line dropped (%lu %s) !\n", _own_wrq->thread_name, _own_wrq->lost,
                    _own_wrq->opts & LOGGER_OPT_PRINTLOST ? "since last print" : "so far");
            return errno = EAGAIN, -1;
        }
        usleep(50);
    }
    if (_own_wrq->lost && _own_wrq->opts & LOGGER_OPT_PRINTLOST) {
        int lost = _own_wrq->lost;
        _own_wrq->lost_total += lost;
        _own_wrq->lost = 0;

        logger_printf(LOGGER_LEVEL_OOPS, __FILE__, __FUNCTION__, __LINE__,
            "Lost %d log line(s) (%d so far) !", lost, _own_wrq->lost_total);

        goto reindex;
    }
    va_start(ap, format);

    clock_gettime(CLOCK_REALTIME, &l->ts);
    l->level = level;
    l->file = src;
    l->func = func;
    l->line = line;
    vsnprintf(l->str, sizeof(l->str), format, ap);

    fprintf(stderr, "<%s> '%s' (%d)\n", _own_wrq->thread_name, l->str, _own_wrq->queue_idx);

    l->ready = true;
    _own_wrq->wr_seq++;

    if (_logger_wakeup_reader_if_needed() < 0) {
        return -1;
    }
    return 0;
}

#endif // defined(LOGGER_USE_THREAD)
