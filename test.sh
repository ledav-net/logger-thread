#!/bin/sh
# Copyright (C) 2009 David De Grave <david@ledav.net>
#
# This program is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation, version 3.
#
# This program is distributed in the hope that it will be useful, but
# WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
# General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program. If not, see <http://www.gnu.org/licenses/>.

for ((i=0 ; i<1000; i++)); do
	# Randmize between 1 & 10 threads with between 51 & 100 buffered lines
	# repeated 1000x10000 times.
	./logger 10 100 1000
done > out.log 2>&1

# So, we expect to have 10 000 unique lines (10 threads x 1000 lines)
# No outputs = Success.
# Full output is in out.log
grep ^LOG < out.log | cut -d" " -f 5 | sort | uniq -c | tail | grep -v 10000

# And we should have the 10M lines
max=10000000
lines=$(grep ^LOG < out.log | wc -l)
if [ $max -ne $lines ]; then
	echo "Error: $lines lines !"
	exit 1
fi
