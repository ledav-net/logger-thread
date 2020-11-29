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

#define LOGGER_LINE_SZ		2048			/* Maximum size per log msg (\0 included) */

typedef enum {
    LOGGER_LEVEL_FIRST		= 0,			/* First level value */

    /* Levels compatibles with syslog */
    LOGGER_LEVEL_EMERG		= LOGGER_LEVEL_FIRST,	/* Emergecy: System is unusable. Complete restart/checks must be done.	*/
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

    LOGGER_LEVEL_COUNT,					/* Number of levels */
    LOGGER_LEVEL_LAST  = LOGGER_LEVEL_COUNT - 1,	/* Last level value */
} logger_line_level_t;

typedef enum {
    LOGGER_OPT_NONE	= 0,	/* No options. Use default values ! */
    LOGGER_OPT_PID	= 1,	/* Add the pid of the actual process on each lines. */
    LOGGER_OPT_NONBLOCK	= 2,	/* return -1 and EAGAIN when the queue is full */
    LOGGER_OPT_SYNC	= 4,	/* Return from printlog when message is really saved. */
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

struct logger_t;

/* Write queue: 1 per thread */
typedef struct {
    logger_line_t	*lines;    /* Lines buffer */
    int			lines_nr;  /* Maximum number of buffered lines for this thread */
    int			queue_idx; /* This queue index */
    atomic_int		waiting;   /* True (1) if the thread is waiting for free space ... */
    unsigned int	rd_idx;    /* Read index */
    unsigned long	rd_seq;    /* Read sequence */
    unsigned long	wr_seq;    /* Write sequence */
    long		lost;	   /* Number of lost records */
    struct logger_t	*logger;   /* Logger queue this write queue belongs to */
    pthread_t		thread;    /* Write thread owning this queue */
} logger_write_queue_t;

typedef struct logger_t {
    logger_write_queue_t **queues;	/* Write queues, 1 per thread */
    int			 queues_nr;	/* Number of queues */
    logger_opts_t	 options;	/* Queue options */
    bool		 terminate;     /* Set to true when the reader thread has to finish */
    pthread_t		 reader_thread; /* TID of the reader thread */
    bool		 empty;		/* Set to true when all the queues are empty */
    atomic_int		 waiting;	/* Futex to old the number of waiting loggers (0 or 1) */
} logger_t;

logger_t *		logger_init(			/* Initialize the logger manager */
                            unsigned int queues_max,	/* Number of writer threads. If 0 => Print log lines directly */
                            unsigned int lines_max,	/* Maximum number of log lines to buffer */
                            logger_opts_t options);	/* See options above. */

void			logger_deinit(logger_t *logger); /* Empty the queues and free all the ressources */

logger_write_queue_t *	logger_get_write_queue(logger_t *logger);

int			logger_printf(logger_t *logger,			/* Print a message */
                            logger_write_queue_t *wrq,			/* Write queue to print to */
                            logger_line_level_t level,			/* Importance level of this print */
                            const char *src,				/* Source/Func/Line this print was issued */
                            const char *func,
                            unsigned int line,
                            const char *format, ...);			/* printf() like format & arguments ... */

extern logger_t *	stdlogger; /* Default logger context to use when using the below LOG_* macros */

extern const char *	logger_level_label[LOGGER_LEVEL_COUNT];

#define logger_std_init			stdlogger = logger_init
#define logger_std_deinit()		logger_deinit(stdlogger)
#define logger_std_get_write_queue()	logger_get_write_queue(stdlogger)
#define logger_std_printf(fmt, ...)	logger_printf(stdlogger, NULL, fmt, ## __VA_ARGS__)

#define timespec_to_ns(a)	((STON((a).tv_sec) + (a).tv_nsec))
#define elapsed_ns(b,a) 	(timespec_to_ns(a) - timespec_to_ns(b))

#define LOGGER_COLORS_ENABLED
//#define LOGGER_DISABLED

#define LOGGER_LEVEL_TRUNCATE	LOGGER_LEVEL_INFO	/* Higher levels than INFO are not handled */

#ifndef LOGGER_DISABLED
#define LOG_LEVEL(lvl, fmt, ...) logger_std_printf( \
    (lvl), \
    __FILE__, \
    __FUNCTION__, \
    __LINE__, \
    fmt, ## __VA_ARGS__)
#define LOG_EMERGENCY(fmt, ...)	logger_std_printf( \
    LOGGER_LEVEL_EMERGENCY, \
    __FILE__, \
    __FUNCTION__, \
    __LINE__, \
    fmt, ## __VA_ARGS__)
#define LOG_ALERT(fmt, ...)	logger_std_printf( \
    LOGGER_LEVEL_ALERT, \
    __FILE__, \
    __FUNCTION__, \
    __LINE__, \
    fmt, ## __VA_ARGS__)
#define LOG_CRITICAL(fmt, ...)	logger_std_printf( \
    LOGGER_LEVEL_CRITICAL, \
    __FILE__, \
    __FUNCTION__, \
    __LINE__, \
    fmt, ## __VA_ARGS__)
#define LOG_ERROR(fmt, ...)	logger_std_printf( \
    LOGGER_LEVEL_ERROR, \
    __FILE__, \
    __FUNCTION__, \
    __LINE__, \
    fmt, ## __VA_ARGS__)
#define LOG_WARNING(fmt, ...)	logger_std_printf( \
    LOGGER_LEVEL_WARNING, \
    __FILE__, \
    __FUNCTION__, \
    __LINE__, \
    fmt, ## __VA_ARGS__)
#define LOG_NOTICE(fmt, ...)	logger_std_printf( \
    LOGGER_LEVEL_NOTICE, \
    __FILE__, \
    __FUNCTION__, \
    __LINE__, \
    fmt, ## __VA_ARGS__)
#define LOG_INFO(fmt, ...)	logger_std_printf( \
    LOGGER_LEVEL_INFO, \
    __FILE__, \
    __FUNCTION__, \
    __LINE__, \
    fmt, ## __VA_ARGS__)
#define LOG_DEBUG(fmt, ...)	logger_std_printf( \
    LOGGER_LEVEL_DEBUG, \
    __FILE__, \
    __FUNCTION__, \
    __LINE__, \
    fmt, ## __VA_ARGS__)
#define LOG_OKAY(fmt, ...)	logger_std_printf( \
    LOGGER_LEVEL_OKAY, \
    __FILE__, \
    __FUNCTION__, \
    __LINE__, \
    fmt, ## __VA_ARGS__)
#define LOG_OOPS(fmt, ...)	logger_std_printf( \
    LOGGER_LEVEL_OOPS, \
    __FILE__, \
    __FUNCTION__, \
    __LINE__, \
    fmt, ## __VA_ARGS__)
#define LOG_TRACE(fmt, ...)	logger_std_printf( \
    LOGGER_LEVEL_TRACE, \
    __FILE__, \
    __FUNCTION__, \
    __LINE__, \
    fmt, ## __VA_ARGS__)
#else
#define LOG_LEVEL
#define LOG_EMERGENCY
#define LOG_ALERT
#define LOG_CRITICAL
#define LOG_ERROR
#define LOG_WARNING
#define LOG_NOTICE
#define LOG_INFO
#define LOG_DEBUG

#define LOG_OKAY
#define LOG_OOPS
#define LOG_TRACE
#endif /* LOGGER_DISABLED */

#endif /* _LOGGER_H */
