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
#include <pthread.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <time.h>

#include "logger.h"

#define NTOS(v) ((v)/1000000000) /* nSec -> Sec  */
#define STON(v) ((v)*1000000000) /*  Sec -> nSec */
#define MTON(v) ((v)*1000000)    /* mSec -> nSec */
#define MTOU(v) ((v)*1000)       /* mSec -> uSec */

typedef struct {
    unsigned long output_max;
    int uwait;
    int luck;
} _thread_params;

/* Test thread */
static void *thread_func_write(const _thread_params *thp)
{
    logger_write_queue_t *wrq = logger_std_get_write_queue();
    int th = wrq->queue_idx;

    for (int seq = 0; seq < thp->output_max; seq++) {
        int index = wrq->wr_seq % wrq->lines_nr;

        if (!(rand() % thp->luck)) {
            fprintf(stderr, "W%02d! Bad luck, waiting for %lu usec\n", th, thp->uwait);
            usleep(thp->uwait);
        }
        struct timespec before, after;
        clock_gettime(CLOCK_MONOTONIC, &before);

        if ( LOG_NOTICE("W%02d %lu => %d", th, seq, index) < 0 ) {
            fprintf(stderr, "W%02d! %lu => %d **LOST** (%m)\n", th, seq, index);
        }
        clock_gettime(CLOCK_MONOTONIC, &after);

        unsigned long usec = after.tv_nsec - before.tv_nsec;
        fprintf(stderr, "W%02d? %lu logger_std_printf took %lu ns (%d)\n",
                        th, after.tv_nsec, usec, index);
    }
    fprintf(stderr, "W%02d! Exit (%d lines dropped)\n", th, wrq->lost);
    return NULL;
}

int main(int argc, char **argv)
{
    if (argc < 6) {
        printf("%s <threads> <max lines/buffer> <max lines per thread> <us wait> <wait luck value> [delay sec]\n", argv[0]);
        return 1;
    }
    int	thread_max = atoi(argv[1]);
    int lines_max = atoi(argv[2]);
    _thread_params thp = {
        atoi(argv[3]),
        atoi(argv[4]),
        atoi(argv[5]),
    };
    int start_wait = 0;
    if ( argc > 6 ) {
        start_wait = atoi(argv[6]);
    }
    srand(time(NULL));

    fprintf(stderr, "cmdline: "); for (int i=0; i<argc; i++) { fprintf(stderr, "%s ", argv[i]); }
    fprintf(stderr, "\nthread_max[%d] lines_max[%d] output_max[%d] (1/%d chances to wait %d us)\n"
                    , thread_max, lines_max, thp.output_max, thp.luck, thp.uwait);
    fprintf(stderr, "Waiting for %d seconds after the logger-reader thread is started\n\n", start_wait);

    logger_std_init(thread_max, lines_max /* default buffer size */, LOGGER_OPT_NONBLOCK);
    sleep(start_wait);

    /* Writer threads */
    for (long i=0 ; i<thread_max ; i++ ) {
        pthread_t *tid = &stdlogger->queues[i]->thread;
        pthread_create(tid, NULL, (void *)thread_func_write, (void *)&thp);
    }
    for (int i=0 ; i<thread_max ; i++ ) {
        pthread_join(stdlogger->queues[i]->thread, NULL);
    }

    logger_deinit(NULL);
    return 0;
}
