#!/bin/bash
# SPDX-License-Identifier: MIT
#
# Copyright 2022 David De Grave <david@ledav.net>
#
# Permission is hereby granted, free of charge, to any person obtaining a copy of
# this software and associated documentation files (the "Software"), to deal in
# the Software without restriction, including without limitation the rights to
# use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of
# the Software, and to permit persons to whom the Software is furnished to do so,
# subject to the following conditions:
#
# The above copyright notice and this permission notice shall be included in all
# copies or substantial portions of the Software.
#
# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
# IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS
# FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR
# COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER
# IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
# CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

# NOTE! Don't change the order of the definitions below.

default+=(5)		# <threads>	Number of concurrent threads to keep running to print <total> lines.
default+=(1000)		# <qmin>	Minimum size (in lines) of the queue to allocate per thread.
default+=(1000)		# <qmax>	Maximum size (in lines) of the queue to allocate per thread. The size is allocated between <qmin> and <qmax>.
default+=(100000)	# <total>	Total number of lines to print, using <threads> producers
default+=(1000)		# <max/thd>	Maximum of lines to process per thread. A new one is allocated using a new queue, with a size betwen <qmin> and <qmax>.
default+=(100)		# <wait>	Penality (time in micro seconds) the thread will have to wait when it has no <chance>.
default+=(1000)		# <chance>	1 chance on <chance> to have to wait <wait> us (try to simulate a more "realistic" logging)

# The following options are bolleans. Enabled = 1, Disabled = 0

default+=(0)	# [non-blocking] Non-blocking mode. Return with an error instead of waiting for free space (blocking).
default+=(0)	# [print lost]   In non-blocking mode, print the number of lines lost so far soon as it can.
default+=(0)	# [noqueue]	 Start of the threads with no queue assignment. Done at the first logger_printlog() call with the default queue size.
default+=(1)	# [prealloc]	 Fills the allocated queues with garbage to force Linux to reserve the page (copy-on-write workaround).
default+=(3) 	# [delay sec]    Start time delay ...

# You can specify your own parameters on the command line, the have precedence.
# The ones between <> below are mandatory.
[ $# -gt 0 ] && params=(${*}) || params=(${default[*]})

# Params = <threads> <qmin> <qmax> <total> <max/thd> <us wait> <chance 1/n> [non-blocking (0)] [print lost (0)] [noqueue (0)] [prealloc (0)] [delay sec]
/usr/bin/time -v ./logger ${params[*]} > out.log 2>&1 &
less -RS +F < out.log
wait
