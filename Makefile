
HDR := logger.h logger-thread.h
SRC := logger.c logger-thread.c main.c logger-colors.c

#ARGC := -Og -ggdb
ARGC := -O3

logger: $(HDR) $(SRC)
	gcc $(ARGC) -std=c17 -Wall -pthread -D_GNU_SOURCE -DLOGGER_USE_THREAD -o logger $(SRC)

clean:
	rm -f logger out*.log
