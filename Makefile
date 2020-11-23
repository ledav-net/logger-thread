
logger: logger.h logger-thread.c main.c
	gcc -Og -ggdb -pthread logger-thread.c main.c -o logger
