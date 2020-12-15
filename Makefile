
logger: logger.h logger.c logger-thread.c logger-thread.h main.c logger-colors.c
	gcc -std=c17 -Og -ggdb -pthread logger-thread.c logger-colors.c logger.c main.c -o logger

clean:
	rm -f logger out*.log