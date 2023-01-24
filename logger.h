#pragma once
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
#ifndef _LOGGER_H
#define _LOGGER_H

#include <pthread.h>
#include <stdbool.h>
#include <stdatomic.h>
#include <time.h>

#ifdef __cplusplus
extern "C" {
#endif

#define LOGGER_LINE_SZ			1024	/* Maximum size per log msg (\0 included) */
#define LOGGER_MAX_PREFIX_SZ		256	/* Added to LOGGER_LINE_SZ for the date/time/... */
#define LOGGER_MAX_THREAD_NAME_SZ	16	/* Maximum size for the thread name (if it is set) */

#define LOGGER_MAX_SOURCE_LEN		50	/* Maximum length of "file:src:line" sub string */

typedef enum {
    /* Levels compatibles with syslog */
    LOGGER_LEVEL_EMERG		= 0,			/* Emergecy: System is unusable. Complete restart/checks must be done.	*/
    LOGGER_LEVEL_ALERT		= 1,			/* Alert:    Process can't continue working. Manual action must be done.*/
    LOGGER_LEVEL_CRITICAL	= 2,			/* Crit:     Process was entered in an unknown state.			*/
    LOGGER_LEVEL_ERROR		= 3,			/* Error:    Error is returned from function, etc...			*/
    LOGGER_LEVEL_WARNING	= 4,			/* Warning:  Message have to be checked further...			*/
    LOGGER_LEVEL_NOTICE		= 5,			/* Notice:   Message could be important/interresting to know.		*/
    LOGGER_LEVEL_INFO		= 6,			/* Info:     Message is symply informational...				*/
    LOGGER_LEVEL_DEBUG		= 7,			/* Debug:    Message is for debugging informations only.		*/
    /* Custom levels */
    LOGGER_LEVEL_OKAY,					/* Okay:     Commit. What is expected happened.				*/
    LOGGER_LEVEL_TRACE,					/* Trace:    Trace lines. To easily filter out hudge amount of lines    */
    LOGGER_LEVEL_OOPS,					/* Oops:     Something not foreseen happened (code mistakes, config, .) */
    /* Internal use */
    LOGGER_LEVEL_COUNT,					/* Number of levels */
    LOGGER_LEVEL_FIRST = LOGGER_LEVEL_EMERG,		/* First level value */
    LOGGER_LEVEL_LAST  = LOGGER_LEVEL_COUNT - 1,	/* Last level value */
} logger_line_level_t;

#define LOGGER_LEVEL_ALL LOGGER_LEVEL_FIRST ... LOGGER_LEVEL_LAST
#define LOGGER_LEVEL_DEFAULT LOGGER_LEVEL_LAST

typedef enum {
    LOGGER_OPT_NONE      = 0,	/* No options. Use default values ! */
    LOGGER_OPT_NONBLOCK  = 1,	/* return -1 and EAGAIN when the queue is full */
    LOGGER_OPT_PRINTLOST = 2,	/* Print lost lines soon as there is some free space again */
    LOGGER_OPT_PREALLOC  = 4,	/* Force the kernel to really allocate the bloc (to bypass the copy-on-write mechanism) */
} logger_opts_t;

/* Definition of a log line */
typedef struct {
    bool		ready;               /* Line ready to be printed */
    struct timespec	ts;                  /* Timestamp (key to order on) */
    logger_line_level_t level;               /* Level of this line */
    const char *	file;		     /* File who generated the log */
    const char *	func;		     /* Function */
    unsigned int	line;		     /* Line */
    char		str[LOGGER_LINE_SZ]; /* Line buffer */
} logger_line_t;

/* Write queue: 1 per thread */
typedef struct {
    logger_line_t	*lines;			/* Lines buffer */
    int			lines_nr;		/* Maximum number of buffered lines for this thread */
    int			queue_idx;		/* Index of the queue */
    logger_opts_t	opts;			/* Options for this queue. Set to default if not precised */
    unsigned int	rd_idx;			/* Actual read index */
    unsigned long	rd_seq;			/* Read sequence */
    unsigned long	wr_seq;			/* Write sequence */
    unsigned long	lost_total;		/* Total number of lost records so far */
    unsigned long	lost;			/* Number of lost records since last printed */
    atomic_int		free;			/* True (1) if this queue is not used */
    pthread_t		thread;			/* Thread owning this queue */
    char		thread_name[LOGGER_MAX_THREAD_NAME_SZ]; /* Thread name */
    int			thread_name_len;	/* Length of the thread name */
} logger_write_queue_t;

typedef struct {
    const char *level[LOGGER_LEVEL_COUNT];		/* Colors definition for the log levels */
    const char *reset;					/* Reset the color to default */
    const char *time;					/* Time string color */
    const char *date;					/* Date string color */
    const char *date_lines;				/* Lines surrounding the date color */
    const char *thread_name;				/* Thread name (or id) color */
} logger_line_colors_t;

typedef struct {
    logger_write_queue_t	**queues;		/* Write queues, 1 per thread */
    int			 	queues_nr;		/* Number of queues allocated */
    int			 	queues_max;		/* Maximum number of possible queues */
    int				default_lines_nr;	/* Default number of lines max / buffer to use */
    logger_line_level_t		level_min;		/* Minimum level to be printed/processed */
    bool		 	running;		/* Set to true when the reader thread is running */
    bool		 	empty;			/* Set to true when all the queues are empty */
    logger_opts_t	 	opts;			/* Default logger options. Some can be fine tuned by write queue */
    atomic_int		 	reload;			/* True (1) when new queue(s) are added */
    atomic_int		 	waiting;		/* True (1) if the reader-thread is sleeping ... */
    pthread_t		 	reader_thread;		/* TID of the reader thread */
    pthread_mutex_t	 	queues_mx;		/* Needed when extending the **queues array... */
    const logger_line_colors_t	*theme;			/* Color theme to use */
} logger_t;

int	logger_init(					/* Initialize the logger manager */
		int queues_max,				/* Hard limit of queues that can be allocated */
		int lines_max_def,			/* Recommended log lines to allocate by default */
		logger_line_level_t level_min,		/* Minimum level to be printed/processed */
		logger_opts_t options);			/* See options above. */

void	logger_deinit(void);				/* Empty the queues and free all the ressources */

int	logger_assign_write_queue(			/* Assign a queue to the calling thread */
		unsigned int lines_max,			/* Max lines buffer (=0 use default) */
		logger_opts_t opts);			/* If not precised, use the logger's options */

int	logger_free_write_queue(void);			/* Release the write queue for another thread */

int	logger_pthread_create(				/* Create a new thread with logger queue assignment */
		const char *thread_name,		/* Thread name to give */
		unsigned int max_lines,			/* Lines buffer to allocte for that thread (=0 use default) */
		logger_opts_t opts,			/* Options to used for this queue. (=0 use default) */
		pthread_t *thread,			/* See pthread_create(3) for these args */
		const pthread_attr_t *attr,
		void *(*start_routine)(void *),
		void *arg);

int	logger_printf(					/* Print a message */
		logger_line_level_t level,		/* Importance level of this print */
		const char *src,			/* Source file of this msg */
		const char *func,			/* Function of this msg */
		unsigned int line,			/* Line of this msg */
		const char *format, ...);		/* printf() like format & arguments ... */

extern logger_t logger; /* Global logger context */

extern const logger_line_colors_t logger_colors_bw;	/* No colors theme (black & white) */
extern const logger_line_colors_t logger_colors_default;/* Default theme */

#define timespec_to_ns(a)	((STON((a).tv_sec) + (a).tv_nsec))
#define elapsed_ns(b,a) 	(timespec_to_ns(a) - timespec_to_ns(b))

#if defined(LOGGER_USE_THREAD) && defined(LOGGER_USE_PRINTF)
#error Both LOGGER_USE_THREAD and LOGGER_USE_PRINTF are defined ! You need to choose...
#endif

/* If the following defines are defined (on the command line for example),
 * the code concerning the excluded levels are stripped off the the objets.
 * If not defined, include all the code.
 *
 * Take care for things like `LOG_DEBUG(function());` ... `function()` will
 * never be called as whatever is passed to LOG_DEBUG is stripped off.
 */
#if   defined(LOGGER_LEVEL_MIN_EMERG)
#define _MIN_LOGGER_LEVEL 0
#elif defined(LOGGER_LEVEL_MIN_ALERT)
#define _MIN_LOGGER_LEVEL 1
#elif defined(LOGGER_LEVEL_MIN_CRITICAL)
#define _MIN_LOGGER_LEVEL 2
#elif defined(LOGGER_LEVEL_MIN_ERROR)
#define _MIN_LOGGER_LEVEL 3
#elif defined(LOGGER_LEVEL_MIN_WARNING)
#define _MIN_LOGGER_LEVEL 4
#elif defined(LOGGER_LEVEL_MIN_NOTICE)
#define _MIN_LOGGER_LEVEL 5
#elif defined(LOGGER_LEVEL_MIN_INFO)
#define _MIN_LOGGER_LEVEL 6
#elif defined(LOGGER_LEVEL_MIN_DEBUG)
#define _MIN_LOGGER_LEVEL 7
#elif defined(LOGGER_LEVEL_MIN_OKAY)
#define _MIN_LOGGER_LEVEL 8
#elif defined(LOGGER_LEVEL_MIN_TRACE)
#define _MIN_LOGGER_LEVEL 9
#elif defined(LOGGER_LEVEL_MIN_OOPS)
#define _MIN_LOGGER_LEVEL 10
#else
#define _MIN_LOGGER_LEVEL 99
#endif

#if defined(LOGGER_USE_THREAD)

#define LOG_LEVEL(lvl, fmt, ...) logger_printf((lvl), __FILE__, __FUNCTION__, __LINE__, fmt, ## __VA_ARGS__)

#if _MIN_LOGGER_LEVEL >= 0
#define LOG_EMERGENCY(fmt, ...)	logger_printf(LOGGER_LEVEL_EMERG, __FILE__, __FUNCTION__, __LINE__, fmt, ## __VA_ARGS__)
#else
#define LOG_EMERGENCY(...)	({ (int)0; })
#endif
#if _MIN_LOGGER_LEVEL >= 1
#define LOG_ALERT(fmt, ...)	logger_printf(LOGGER_LEVEL_ALERT, __FILE__, __FUNCTION__, __LINE__, fmt, ## __VA_ARGS__)
#else
#define LOG_ALERT(...)		({ (int)0; })
#endif
#if _MIN_LOGGER_LEVEL >= 2
#define LOG_CRITICAL(fmt, ...)	logger_printf(LOGGER_LEVEL_CRITICAL, __FILE__, __FUNCTION__, __LINE__, fmt, ## __VA_ARGS__)
#else
#define LOG_CRITICAL(...)	({ (int)0; })
#endif
#if _MIN_LOGGER_LEVEL >= 3
#define LOG_ERROR(fmt, ...)	logger_printf(LOGGER_LEVEL_ERROR, __FILE__, __FUNCTION__, __LINE__, fmt, ## __VA_ARGS__)
#else
#define LOG_ERROR(...)		({ (int)0; })
#endif
#if _MIN_LOGGER_LEVEL >= 4
#define LOG_WARNING(fmt, ...)	logger_printf(LOGGER_LEVEL_WARNING, __FILE__, __FUNCTION__, __LINE__, fmt, ## __VA_ARGS__)
#else
#define LOG_WARNING(...)	({ (int)0; })
#endif
#if _MIN_LOGGER_LEVEL >= 5
#define LOG_NOTICE(fmt, ...)	logger_printf(LOGGER_LEVEL_NOTICE, __FILE__, __FUNCTION__, __LINE__, fmt, ## __VA_ARGS__)
#else
#define LOG_NOTICE(...)		({ (int)0; })
#endif
#if _MIN_LOGGER_LEVEL >= 6
#define LOG_INFO(fmt, ...)	logger_printf(LOGGER_LEVEL_INFO, __FILE__, __FUNCTION__, __LINE__, fmt, ## __VA_ARGS__)
#else
#define LOG_INFO(...)		({ (int)0; })
#endif
#if _MIN_LOGGER_LEVEL >= 7
#define LOG_DEBUG(fmt, ...)	logger_printf(LOGGER_LEVEL_DEBUG, __FILE__, __FUNCTION__, __LINE__, fmt, ## __VA_ARGS__)
#else
#define LOG_DEBUG(...)		({ (int)0; })
#endif
#if _MIN_LOGGER_LEVEL >= 8
#define LOG_OKAY(fmt, ...)	logger_printf(LOGGER_LEVEL_OKAY, __FILE__, __FUNCTION__, __LINE__, fmt, ## __VA_ARGS__)
#else
#define LOG_OKAY(...)		({ (int)0; })
#endif
#if _MIN_LOGGER_LEVEL >= 9
#define LOG_TRACE(fmt, ...)	logger_printf(LOGGER_LEVEL_TRACE, __FILE__, __FUNCTION__, __LINE__, fmt, ## __VA_ARGS__)
#else
#define LOG_TRACE(...)		({ (int)0; })
#endif
#if _MIN_LOGGER_LEVEL >= 10
#define LOG_OOPS(fmt, ...)	logger_printf(LOGGER_LEVEL_OOPS, __FILE__, __FUNCTION__, __LINE__, fmt, ## __VA_ARGS__)
#else
#define LOG_OOPS(...)		({ (int)0; })
#endif

#else

#if defined(LOGGER_USE_PRINTF)
#include <stdio.h>

#define _LOG_PRINTF(fmt, ...) \
        printf("%s: %s: (%d) " fmt "\n", __FILE__, __FUNCTION__, __LINE__, ## __VA_ARGS__)

#define LOG_LEVEL(lvl, fmt, ...) ({ \
        (void)(lvl); _LOG_PRINTF(fmt, ## __VA_ARGS__ ); \
})
#if _MIN_LOGGER_LEVEL >= 0
#define LOG_EMERGENCY		_LOG_PRINTF
#else
#define LOG_EMERGENCY(...)	({ (int)0; })
#endif
#if _MIN_LOGGER_LEVEL >= 1
#define LOG_ALERT		_LOG_PRINTF
#else
#define LOG_ALERT(...)		({ (int)0; })
#endif
#if _MIN_LOGGER_LEVEL >= 2
#define LOG_CRITICAL		_LOG_PRINTF
#else
#define LOG_CRITICAL(...)	({ (int)0; })
#endif
#if _MIN_LOGGER_LEVEL >= 3
#define LOG_ERROR		_LOG_PRINTF
#else
#define LOG_ERROR(...)		({ (int)0; })
#endif
#if _MIN_LOGGER_LEVEL >= 4
#define LOG_WARNING		_LOG_PRINTF
#else
#define LOG_WARNING(...)	({ (int)0; })
#endif
#if _MIN_LOGGER_LEVEL >= 5
#define LOG_NOTICE		_LOG_PRINTF
#else
#define LOG_NOTICE(...)		({ (int)0; })
#endif
#if _MIN_LOGGER_LEVEL >= 6
#define LOG_INFO		_LOG_PRINTF
#else
#define LOG_INFO(...)		({ (int)0; })
#endif
#if _MIN_LOGGER_LEVEL >= 7
#define LOG_DEBUG		_LOG_PRINTF
#else
#define LOG_DEBUG(...)		({ (int)0; })
#endif
#if _MIN_LOGGER_LEVEL >= 8
#define LOG_OKAY		_LOG_PRINTF
#else
#define LOG_OKAY(...)		({ (int)0; })
#endif
#if _MIN_LOGGER_LEVEL >= 9
#define LOG_TRACE		_LOG_PRINTF
#else
#define LOG_TRACE(...)		({ (int)0; })
#endif
#if _MIN_LOGGER_LEVEL >= 10
#define LOG_OOPS		_LOG_PRINTF
#else
#define LOG_OOPS(...)		({ (int)0; })
#endif

#define logger_init(...)		({ (int)0; })
#define logger_deinit(...)		({ (void)0; })
#define logger_assign_write_queue(...)	({ (int)0; })
#define logger_free_write_queue(...)	({ (int)0; })

#define logger_pthread_create(a, b, c, d, e, f, g) ({ \
            (void)(a); (void)(b); (void)(c); pthread_create(d, e, f, g); \
})

#else // default => Strip all

#define LOG_LEVEL(lvl, ...)	({ (void)(lvl); (int)0; })
#define LOG_EMERGENCY(...)	({ (int)0; })
#define LOG_ALERT(...)		({ (int)0; })
#define LOG_CRITICAL(...)	({ (int)0; })
#define LOG_ERROR(...)		({ (int)0; })
#define LOG_WARNING(...)	({ (int)0; })
#define LOG_NOTICE(...)		({ (int)0; })
#define LOG_INFO(...)		({ (int)0; })
#define LOG_DEBUG(...)		({ (int)0; })
#define LOG_OKAY(...)		({ (int)0; })
#define LOG_OOPS(...)		({ (int)0; })
#define LOG_TRACE(...)		({ (int)0; })

#define logger_init(...)		({ (int)0; })
#define logger_deinit(...)		({ (void)0; })
#define logger_assign_write_queue(...)	({ (int)0; })
#define logger_free_write_queue(...)	({ (int)0; })

#define logger_pthread_create(a, b, c, d, e, f, g) ({ \
            (void)(a); (void)(b); (void)(c); pthread_create(d, e, f, g); \
})
#endif // defined(LOGGER_USE_PRINTF)
#endif // defined(LOGGER_USE_THREAD)

#ifdef __cplusplus
}
#endif
#endif /* _LOGGER_H */
