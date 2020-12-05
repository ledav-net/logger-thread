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

#        <threads> <min> <max> <print max / thd> <us wait> <wait chance> [delay sec]
default=( 10       50    200   10000             100       10            5          )

[ $# -gt 0 ] && params=(${*}) || params=(${default[*]})

/usr/bin/time -v ./logger ${params[*]} > out.log 2>&1 &
less -RS +F < out.log
