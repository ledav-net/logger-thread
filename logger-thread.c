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
#include <errno.h>
#include <pthread.h>
#include <stdatomic.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include "logger.h"

#define STON(v) ((v)*1000000000) /* Sec -> nSec */
#define NTOS(v) ((v)/1000000000) /* nSec -> Sec */

#define swap_unsafe(a, b) { __typeof__ (a) _t = (a); (a) = (b); (b) = (_t); }

logger_t *stdlogger = NULL;

static int _logger_write_line(logger_opts_t options, logger_line_t *line)
{
    char buf[LOGGER_LINE_SZ+16];
    unsigned long ts = STON(line->ts.tv_sec) + line->ts.tv_nsec;
    int len = sprintf(buf, "LOG> %lu %d %s\n", ts, line->level, line->str);
    write(1, buf, len);
    return 0;
}

static inline void _bubble_fuse_up(_logger_fuse_entry_t *fuse, int fuse_nr)
{
    /* Sort the first entry following the bubble up logic */
    for (int i=0; i<fuse_nr-1 && fuse[i].ts > fuse[i+1].ts; i++) { // Bigger move up (& stack empty ones at the end)
        swap_unsafe(fuse[i], fuse[i+1]);
    }
}

static inline void _bubble_fuse_down(_logger_fuse_entry_t *fuse, int fuse_nr)
{
    /* Sort the last entry following the bubble down logic */
    for (int i=fuse_nr-1; i>0 && fuse[i].ts <= fuse[i-1].ts; i--) { // Lower move down
        swap_unsafe(fuse[i], fuse[i-1]);
    }
}

static inline int _logger_set_queue_entry(const logger_write_queue_t *wrq, _logger_fuse_entry_t *fuse)
{
    unsigned int index = wrq->rd_idx;

    if (wrq->lines[index].ready) { // when it's ready, we can proceed...
        struct timespec ts = wrq->lines[index].ts;
        fuse->ts = STON(ts.tv_sec) + ts.tv_nsec;
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

        wrq->lines[wrq->rd_idx].ready = false; // Free this line for the logger thread
        wrq->rd_idx = ++wrq->rd_seq % wrq->lines_nr;
        empty_nr += _logger_set_queue_entry(wrq, &fuse[0]); // Enqueue the next line

        _bubble_fuse_up(fuse, fuse_nr); /* Let him find it's place */
    }
    /* Let's get a look at the empty queues as well. While we are at it ... */
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
    _logger_fuse_entry_t fuse_queue[q->queues_nr];

    empty_nr = _logger_init_lines_queue(q, fuse_queue, q->queues_nr);

    while (1) {
        empty_nr = _logger_enqueue_next_lines(fuse_queue, q->queues_nr, empty_nr);

        if (fuse_queue[0].ts == ~0) {
            q->empty = true;
            fprintf(stderr, "RDR! Print queue empty ... Zzzzzzz\n");
            if (q->terminate) {
                break;
            }
            usleep(200);
            continue;
        }
        q->empty = false;

        logger_write_queue_t *wrq = fuse_queue[0].wrq;
        int rv = _logger_write_line(q->options, &wrq->lines[wrq->rd_idx]);
        if (rv < 0) {
            fprintf(stderr, "RDR: logger_write_line(): %m\n");
            return NULL;
        }
    }
    fprintf(stderr, "RDR! Exit\n");
    return NULL;
}

static inline void _logger_deinit_write_queue(logger_write_queue_t *wrq)
{
    free(wrq->lines);
}

static inline void _logger_init_write_queue(logger_write_queue_t *wrq, int lines_max)
{
    wrq->lines = calloc(lines_max, sizeof(logger_line_t));
    wrq->lines_nr = lines_max;
}

logger_t *logger_init(unsigned int write_queues_max, unsigned int lines_max, logger_opts_t options)
{
    logger_t *q;

    if (!write_queues_max) {
        return errno = ECHILD, NULL; // No child processes (No readers, nothing to do ...)
    }
    q = calloc(1, sizeof(logger_t));
    q->queues = calloc(write_queues_max, sizeof(logger_write_queue_t *));
    q->queues_nr = write_queues_max;
    q->options = options;

    for (int i=0 ; i < write_queues_max ; i++) {
        logger_write_queue_t *wrq = calloc(1, sizeof(logger_write_queue_t));
        _logger_init_write_queue(wrq, lines_max);
        wrq->queue_idx = i;
        q->queues[i] = wrq;
    }
    /* Reader thread */
    pthread_create(&q->reader_thread, NULL, (void *)_thread_logger, (void *)q);

    return stdlogger = q;
}

void logger_deinit(logger_t *_q)
{
    logger_t *q = _q ?: stdlogger;

    q->terminate = true;
    pthread_join(q->reader_thread, NULL);
    for (int i=0 ; i<q->queues_nr; i++) {
        _logger_deinit_write_queue(q->queues[i]);
    }
    free(q->queues);
    free(q);
}

logger_write_queue_t *logger_get_std_write_queue(void)
{
    if (!stdlogger) {
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

int logger_printf(logger_write_queue_t *wrq, logger_line_level_t level,
        const char *src,
        const char *func,
        unsigned int line,
        const char *format, ...)
{
    return 0;
}
