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

#include <sys/time.h>
#include <stdatomic.h>
#include <stdbool.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <time.h>

#include "logger.h"
#include "logger-thread.h"

static const char * const _logger_level_label[LOGGER_LEVEL_COUNT] = {
    [LOGGER_LEVEL_EMERG]    = "EMERG",
    [LOGGER_LEVEL_ALERT]    = "ALERT",
    [LOGGER_LEVEL_CRITICAL] = "CRIT ",
    [LOGGER_LEVEL_ERROR]    = "ERROR",
    [LOGGER_LEVEL_WARNING]  = "WARN!",
    [LOGGER_LEVEL_NOTICE]   = "NOTCE",
    [LOGGER_LEVEL_INFO]     = "INFO ",
    [LOGGER_LEVEL_DEBUG]    = "DEBUG",
    [LOGGER_LEVEL_OKAY]     = "OKAY ",
    [LOGGER_LEVEL_TRACE]    = "TRACE",
    [LOGGER_LEVEL_OOPS]     = "OOPS!",
};

typedef struct {
    unsigned long         ts;  /* Key to sort on (ts of current line) */
    logger_write_queue_t *wrq; /* Related write queue */
} _logger_fuse_entry_t;

// Uncomment to strip all the debug lines for this source.
//#define fprintf

static const char *_logger_get_date(unsigned long sec, const logger_line_colors_t *c)
{
    static char date[64];
    static unsigned long prev_sec = 0;

    if (sec - prev_sec >= 60*60*24) {
        char tmp[16];
        struct tm tm;
        localtime_r(&sec, &tm);
        strftime(tmp, sizeof(tmp), "%Y-%m-%d", &tm);
        sprintf(date, "%s-- %s%s%s --%s\n",
                    c->date_lines, c->date, tmp, c->date_lines, c->reset);
        prev_sec = sec;
        return date;
    }
    return "";
}

static const char *_logger_get_time(unsigned long sec, const logger_line_colors_t *c)
{
    static char time[32];
    static unsigned long prev_sec = 0;

    if (sec - prev_sec >= 60) {
        char tmp[8];
        struct tm tm;
        localtime_r(&sec, &tm);
        strftime(tmp, sizeof(tmp), "%H:%M", &tm);
        sprintf(time, "%s%s%s", c->time, tmp, c->reset);
        prev_sec = sec;
    }
    return time;
}

static int _logger_write_line(const logger_write_queue_t *wrq, const logger_line_t *l)
{
    char linestr[LOGGER_LINE_SZ + LOGGER_MAX_PREFIX_SZ];
    const logger_line_colors_t *c = logger.theme;

    /* File/Function/Line */
    char src_str[128], *start_of_src_str = src_str;
    int len = snprintf(src_str, sizeof(src_str), "%24s %20s %4d"
                                               , l->file, l->func, l->line);
    if ( len > LOGGER_MAX_SOURCE_LEN ) {
        start_of_src_str += len - LOGGER_MAX_SOURCE_LEN;
    }
    /* Time stamp calculations */
    unsigned long usec = NTOU(l->ts.tv_nsec) % 1000;
    unsigned long msec = NTOM(l->ts.tv_nsec) % 1000;
    int sec            = l->ts.tv_sec % 60;
    /* Format all together */
    len = snprintf(linestr, sizeof(linestr),
            "%s%s:%02d.%03lu,%03lu [%s%s%s] %*s <%s%*s%s> %s\n",
            _logger_get_date(l->ts.tv_sec, c),
            _logger_get_time(l->ts.tv_sec, c),
            sec, msec, usec,
            c->level[l->level], _logger_level_label[l->level], c->reset,
            LOGGER_MAX_SOURCE_LEN, start_of_src_str,
            c->thread_name, sizeof(wrq->thread_name)-1, wrq->thread_name, c->reset, l->str);
    /* Print */
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

static inline int _logger_init_lines_queue(_logger_fuse_entry_t *fuse, int fuse_nr)
{
    memset(fuse, 0, fuse_nr * sizeof(_logger_fuse_entry_t));

    for (int i=0 ; i<fuse_nr; i++) {
        fuse[i].ts  = ~0; // Init all the queues as if they were empty
        fuse[i].wrq = logger.queues[i];
    }
    return fuse_nr; // Number of empty queues (all)
}

void *_thread_logger(void)
{
    bool terminate = logger.terminate;

    fprintf(stderr, "RDR! Starting...\n");

    while (!terminate) {
        logger_write_queue_t *wrq;
        int empty_nr = 0;
        int really_empty = 0;
        int fuse_nr = logger.queues_nr;

        if (!fuse_nr) {
            fprintf(stderr, "RDR! Wake me up when there is somet'n... Zzz\n");
            atomic_store(&logger.waiting, 1);
            if (futex_wait(&logger.waiting, 1) < 0 && errno != EAGAIN) {
                    fprintf(stderr, "RDR! ERROR: %m !\n");
                    break;
            }
            continue;
        }
        _logger_fuse_entry_t fuse_queue[fuse_nr];

        fprintf(stderr, "RDR! (re)loading... _logger_fuse_entry_t = %d x %d bytes (%d bytes total)\n",
                        fuse_nr, sizeof(_logger_fuse_entry_t), sizeof(fuse_queue));

        empty_nr = _logger_init_lines_queue(fuse_queue, fuse_nr);

        while (1) {
            empty_nr = _logger_enqueue_next_lines(fuse_queue, fuse_nr, empty_nr);

            if (atomic_compare_exchange_strong(&logger.reload, &(atomic_int){ 1 }, 0)) {
                break;
            }
            if (fuse_queue[0].ts == ~0) {
                logger.empty = true;
                if (logger.terminate) {
                    /* We want to terminate when all the queues are empty ! */
                    terminate = true;
                    break; // while (1)
                }
                if (really_empty < 5) {
                    int wait = 32 << really_empty++;
                    fprintf(stderr, "RDR! Print queue empty. Double check in %d us ...\n", wait);
                    usleep(wait);
                    /* Double-check (5 times) if the queue is really empty for ~1ms */
                    /* This avoid the writers to wakeup too frequently the reader in case of burst */
                    continue;
                }
                really_empty = 0;
                fprintf(stderr, "RDR! Print queue REALLY empty ... Zzz\n");
                atomic_store(&logger.waiting, 1);
                if (futex_wait(&logger.waiting, 1) < 0 && errno != EAGAIN) {
                    fprintf(stderr, "RDR! ERROR: %m !\n");
                    terminate = true;
                    break;
                }
                continue;
            }
            logger.empty = false;
            really_empty = 0;

            logger_write_queue_t *wrq = fuse_queue[0].wrq;
            int rv = _logger_write_line(wrq, &wrq->lines[wrq->rd_idx]);
            if (rv < 0) {
                fprintf(stderr, "RDR: logger_write_line(): %m\n");
                break;
            }
        }
    }
    fprintf(stderr, "RDR! Exit\n");
    return NULL;
}
