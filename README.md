
## Logger thread ##

### The goals of this project ###

* Let the writer threads as free as possible (no mutexes, limited atomics)
* Send output lines per lines in an atomic manner (human monitoring)
* Guarantee the chronological order and accuracy of the time stamps
* The reader thread consume as less ressources as possible
* Memory allocated one time for all at thread creation time
* Limit the amount of memory consumed by reusing queues
* Log levels are compatible with syslog

### Main features ###

* Easy pthread_create wrapper to handle internal house keeping transparently
* The logger can be completely disabled using defines at compile time
* Also using defines, switch to a simple "printf" logging without logger thread
* You can drop the code above the minimal level at compile time
* And also filter the lines at runtime by changing the min. level to print
* Switch between the classical color themes or b/w
* ...

### Main logic / idea ###

               +---------------+       _______       +----------+
    stdout <-- | logger thread | <--- | lines | <--- | thread 1 |
               +---------------+   |   -------       +----------+
                      ^ |          |   _______       +----------+
                      | v          -- | lines | <--- | thread 2 |
                 ____________      |   -------       +----------+
                | line thr 2 |     |   _______       +----------+
                 ------------      -- | lines | <--- | thread n |
                | line thr 1 |         -------       +----------+
                 ------------
                | line thr n |
                 ------------

Each threads have it's own queue allocated when it is forked, with a fine
tunable number of lines, used to buffer the lines to output.  The logger
thread takes care of reading these lines from all the queues in a
chronological order throught an internal sorted 'fuse table', before beeing
formatted and sent to the standard output.

As there is only one reader and one writer per queue, there is no need to
use the classical locking mechanism between the threads.  This let them free
for more parallelism in multi core environments.

The queues can be finetuned when the thread is forked.  More buffer the
thread have, more burst loggings can be handled before forcing the writer
thread to wait (blocking mode).

A non-blocking mode can also be set to return an error when the queue is
full.  Also, for convenience / easy tracing, another option is there to log
the number of lost lines soon as some more space is free.  This allows you
to know the queue was too small at a certain point in time and maybe fine
tune the buffer size for that thread or debug why this is happening at these
times ...

To optimize the (re)use of the queues, threads can also be started with no
queue assigned and assigned it at the time of the 1st print.  This could be
useful in case you know that a short lived thread can eventually log
something, etc.

Another option let you preallocate the memory used by the queues (so
basically bypassing the copy-on-write feature of the kernel) to don't have
the thread loosing time when it have to allocate a page to the process.

The test script can be used with various senarios to see how it react and
which option to choose in some contexts.

As stated, the main goal of this logger is to minimize as much as possible
the time spent by the threads to log something on the terminal or on slow
block devices.  And also to don't serialize them when logging, who is the
main problem of many logger libraries.

Any comments/pull requests/issue reports/ideas/pokes/... are welcome :-)

David.
