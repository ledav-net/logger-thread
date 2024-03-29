
HDR := logger.h logger-thread.h
SRC := logger.c logger-thread.c main.c logger-colors.c

CC ?= gcc

ARGC += -O3
#ARGC += -O0 -ggdb
#ARGC += -save-temps

DEFINES += -DLOGGER_USE_THREAD
#DEFINES += -DLOGGER_USE_PRINTF
#DEFINES += -DLOGGER_LEVEL_MIN_EMERG
#DEFINES += -DLOGGER_LEVEL_MIN_ERROR
#DEFINES += -D_DEBUG_LOGGER

build: logger

logger: $(HDR) $(SRC)
	$(CC) $(ARGC) -std=c17 -Wall -pthread -D_GNU_SOURCE $(DEFINES) -o logger $(SRC)

clean:
	rm -f logger out*.log *.[iso]
