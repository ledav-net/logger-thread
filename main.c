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

/* Test thread */
static void *thread_func_write(unsigned long output_max)
{
    logger_write_queue_t *wrq = logger_std_get_write_queue();
    int th = wrq->queue_idx;

    for (int seq = 0; seq < output_max; seq++) {
        int index = wrq->wr_seq % wrq->lines_nr;

        unsigned long usec = MTOU(1 + rand() % 1000);
        if (usec > MTOU(900)) {
            usleep(usec);
        }
        struct timespec before, after;
        clock_gettime(CLOCK_MONOTONIC, &before);

        if ( LOG_NOTICE("W%02d %lu => %d", th, seq, index) < 0 ) {
            fprintf(stderr, "W%02d! %lu => %d **LOST** (%m)\n", th, seq, index);
        }
        clock_gettime(CLOCK_MONOTONIC, &after);

        usec = after.tv_nsec - before.tv_nsec;
        fprintf(stderr, "W%02d? %lu logger_std_printf took %lu ns (%d)\n",
                        th, after.tv_nsec, usec, index);
    }
    fprintf(stderr, "W%02d! Exit (%d lines dropped)\n", th, wrq->lost);
    return NULL;
}

int main(int argc, char **argv)
{
    if (argc < 4) {
        printf("%s <threads> <buffer> <output per thread>\n", argv[0]);
        return 1;
    }
    int	thread_max = atoi(argv[1]);
    int lines_max = atoi(argv[2]);
    unsigned long output_max = atoi(argv[3]);

    srand(time(NULL));

    logger_std_init(thread_max, lines_max /* default buffer size */, LOGGER_OPT_NONBLOCK);

    /* Writer threads */
    for (long i=0 ; i<thread_max ; i++ ) {
        pthread_create(&stdlogger->queues[i]->thread, NULL, (void *)thread_func_write, (void *)output_max);
    }
    for (int i=0 ; i<thread_max ; i++ ) {
        pthread_join(stdlogger->queues[i]->thread, NULL);
    }

    logger_deinit(NULL);
    return 0;
}
