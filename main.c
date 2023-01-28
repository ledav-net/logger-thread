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

#include <pthread.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <time.h>

#include "logger.h"

// Comment this to turn off the debug lines to stderr for this source.
#define _DEBUG_LOGGER

#ifdef _DEBUG_LOGGER
#define dbg_printf(args...) fprintf(stderr, args)
#else
#define dbg_printf(...)
#endif

#define NTOS(v) ((v)/1000000000) /* nSec -> Sec  */
#define STON(v) ((v)*1000000000) /*  Sec -> nSec */
#define MTON(v) ((v)*1000000)    /* mSec -> nSec */
#define MTOU(v) ((v)*1000)       /* mSec -> uSec */

typedef struct {
    unsigned long print_max;
    int thread_max;
    int lines_min;
    int lines_max;
    int lines_total;
    int uwait;
    int chances;
    int opts;
} _thread_params;

typedef struct  {
    const _thread_params *params;  // Input parameters
    const unsigned int   *work;    // Input workset to print
    unsigned int         *printed; // Output printed lines (thread)
} _thread_args;

/* Test thread */
static void *thread_func_write(const _thread_args *tha)
{
    char th[LOGGER_MAX_THREAD_NAME_SZ];
    unsigned long elapsed = 0;
    unsigned int  count = 0;

    pthread_getname_np(pthread_self(), th, sizeof(th));

    for (int seq = 0; seq < *tha->work; seq++) {
        if (!(rand() % tha->params->chances)) {
            dbg_printf("<%s> Bad luck, waiting for %d usec\n", th, tha->params->uwait);
            usleep(tha->params->uwait);
        }
        struct timespec before, after;
        int level = rand() % LOGGER_LEVEL_COUNT, r;

        clock_gettime(CLOCK_MONOTONIC, &before);

        switch ( level ) {
        case LOGGER_LEVEL_EMERG:
            r = LOG_EMERGENCY("Message #%-5d (the previous call to logger_printf() took %lu ns)", seq, elapsed); break;
        case LOGGER_LEVEL_ALERT:
            r = LOG_ALERT("Message #%-5d (the previous call to logger_printf() took %lu ns)", seq, elapsed); break;
        case LOGGER_LEVEL_CRITICAL:
            r = LOG_CRITICAL("Message #%-5d (the previous call to logger_printf() took %lu ns)", seq, elapsed); break;
        case LOGGER_LEVEL_ERROR:
            r = LOG_ERROR("Message #%-5d (the previous call to logger_printf() took %lu ns)", seq, elapsed); break;
        case LOGGER_LEVEL_WARNING:
            r = LOG_WARNING("Message #%-5d (the previous call to logger_printf() took %lu ns)", seq, elapsed); break;
        case LOGGER_LEVEL_NOTICE:
            r = LOG_NOTICE("Message #%-5d (the previous call to logger_printf() took %lu ns)", seq, elapsed); break;
        case LOGGER_LEVEL_INFO:
            r = LOG_INFO("Message #%-5d (the previous call to logger_printf() took %lu ns)", seq, elapsed); break;
        case LOGGER_LEVEL_DEBUG:
            r = LOG_DEBUG("Message #%-5d (the previous call to logger_printf() took %lu ns)", seq, elapsed); break;
        case LOGGER_LEVEL_OKAY:
            r = LOG_OKAY("Message #%-5d (the previous call to logger_printf() took %lu ns)", seq, elapsed); break;
        case LOGGER_LEVEL_TRACE:
            r = LOG_TRACE("Message #%-5d (the previous call to logger_printf() took %lu ns)", seq, elapsed); break;
        default:
            r = LOG_OOPS("Message #%-5d (the previous call to logger_printf() took %lu ns)", seq, elapsed); break;
        }

        clock_gettime(CLOCK_MONOTONIC, &after);
        elapsed = elapsed_ns(before, after);

        if ( r < 0 ) {
            dbg_printf("<%s> Message #%d **LOST** (%m)\n", th, seq);
        }
        else count++;
    }
    *tha->printed = count;
    return NULL;
}

int main(int argc, char **argv)
{
    int start_wait = 0;

    if (argc < 7) {
        printf("%s <threads> <min q lines> <max q lines> <total lines> <print max/thd> <us wait> <wait chances> "
               "[blocking (0)] [printlost (0)] [noqueue (0)] [prealloc (0à)] [delay sec]\n", argv[0]);
        return 1;
    }
    _thread_params thp = {
        .thread_max  = atoi(argv[1]),
        .lines_min   = atoi(argv[2]),
        .lines_max   = atoi(argv[3]),
        .lines_total = atoi(argv[4]),
        .print_max   = atoi(argv[5]),
        .uwait       = atoi(argv[6]),
        .chances     = atoi(argv[7]),
        .opts        = LOGGER_OPT_NONE,
    };
    if (argc > 8) {
        if (atoi(argv[8])) {
            thp.opts |= LOGGER_OPT_NONBLOCK;
        }
    }
    if ( argc > 9) {
        if (atoi(argv[9]) && (thp.opts & LOGGER_OPT_NONBLOCK)) {
            thp.opts |= LOGGER_OPT_PRINTLOST;
        }
    }
    if ( argc > 10) {
        if (atoi(argv[10])) {
            thp.opts |= LOGGER_OPT_NOQUEUE;
        }
    }
    if ( argc > 11) {
        if (atoi(argv[11])) {
            thp.opts |= LOGGER_OPT_PREALLOC;
        }
    }
    if (argc > 12) {
        start_wait = atoi(argv[12]);
    }
    srand(time(NULL));

    dbg_printf("cmdline: "); for (int i=0; i<argc; i++) { dbg_printf("%s ", argv[i]); }
    dbg_printf("\nthreads[%d] q_min[%d] q_max[%d] lines_total[%d] max_lines/thr[%lu] (1/%d chances to wait %d us)%s%s%s%s\n",
                thp.thread_max, thp.lines_min, thp.lines_max, thp.lines_total, thp.print_max, thp.chances, thp.uwait,
                thp.opts & LOGGER_OPT_NONBLOCK  ? " non-blocking" : "",
                thp.opts & LOGGER_OPT_PRINTLOST ? "+printlost"    : "",
                thp.opts & LOGGER_OPT_NOQUEUE   ? " noqueue"      : "",
                thp.opts & LOGGER_OPT_PREALLOC  ? " prealloc"     : "");
    dbg_printf("Waiting for %d seconds after the logger-reader thread is started\n\n", start_wait);

    struct timespec before, after;
    clock_gettime(CLOCK_MONOTONIC, &before);
    dbg_printf("For reference, the call to dbg_printf() to print this line took: ");
    clock_gettime(CLOCK_MONOTONIC, &after);
    dbg_printf("%lu ns\n\n", elapsed_ns(before, after));

    /* Writer threads */
    char          tnm[thp.thread_max][LOGGER_MAX_THREAD_NAME_SZ]; // Thread NaMes
    pthread_t     tid[thp.thread_max]; // Thread IDs
    _thread_args  tha[thp.thread_max]; // THread Args
    unsigned int  twk[thp.thread_max]; // Thread WorK (lines asked to log)
    unsigned int  tpr[thp.thread_max]; // Thread PRinted (lines really logged)
    unsigned long dispatched_lines = 0;
    unsigned long printed_lines = 0;

    for (int i=0 ; i < thp.thread_max ; i++ ) {
        snprintf(tnm[i], LOGGER_MAX_THREAD_NAME_SZ, "writer-thd-%04d", (char)i);
        tha[i].params = &thp;
        tha[i].work = &twk[i];
        tha[i].printed = &tpr[i];
        tid[i] = tpr[i] = twk[i] = 0;
    }

    logger_init(thp.thread_max * 5, 50, LOGGER_LEVEL_DEFAULT, LOGGER_OPT_NONE);
    sleep(start_wait);

    int running;

    do {
        running = thp.thread_max;

        for (int i=0 ; i < thp.thread_max ; i++ ) {
            if ( tid[i] ) {
                if ( !tpr[i] ) {
                    // Not finished yet ...
                    continue;
                }
                if ( pthread_join(tid[i], NULL) < 0 ) {
                    dbg_printf("Thread %02d: pthread_join(%lu,NULL): %m\n", i, tid[i]);
                    exit(1);
                }
                if ( tpr[i] != twk[i] ) {
                    dbg_printf("Thread %02d did not printed all the lines ! Asked %u got %u (lost %u) ?!\n",
                        i, twk[i], tpr[i], twk[i] - tpr[i]);
                }
                printed_lines += tpr[i];
                tid[i] = tpr[i] = twk[i] = 0;
            }
            if ( dispatched_lines < thp.lines_total ) {
                // There are still lines to dispatch... (Re)starting a new thread
                int queue_size = thp.lines_min + rand() % (thp.lines_max - thp.lines_min + 1);
                int workset = (thp.lines_total - dispatched_lines) % thp.print_max;

                workset = workset ?: thp.print_max;
                twk[i] = workset; // Take in account the remaining lines

                logger_pthread_create(tnm[i], queue_size, thp.opts,
                    &tid[i], NULL, (void *)thread_func_write, (void *)&tha[i]);

                dispatched_lines += workset;
                dbg_printf("(Re)starting thread %02d (workset = %u)...\n", i, workset);
            }
            else running--;
        }
        usleep(100);
    }
    while ( running );

    logger_deinit();

    dbg_printf("%lu total lines dispatched and %lu lines printed (%lu lost) ...\n",
                dispatched_lines, printed_lines, dispatched_lines - printed_lines);
    return 0;
}
