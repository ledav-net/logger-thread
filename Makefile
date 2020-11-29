
logger: logger.h logger-thread.c main.c logger-colors.c
	gcc -Og -ggdb -pthread logger-thread.c logger-colors.c main.c -o logger
