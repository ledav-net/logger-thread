#pragma once
/*
 * SPDX-License-Identifier: MIT
 *
 * Copyright 2022 David De Grave <david@ledav.net>
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy of
 * this software and associated documentation files (the "Software"), to deal in
 * the Software without restriction, including without limitation the rights to
 * use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of
 * the Software, and to permit persons to whom the Software is furnished to do so,
 * subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS
 * FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR
 * COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER
 * IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */
#ifndef _COLORS_H
#define _COLORS_H

#define C_RST	"\033[0m"	/* RESET */
#define C_CLR	"\033[2j"	/* Clear screen */

#define C_BLD	"\033[1m"       /* Only turn bold on */
#define C_UNL	"\033[4m"       /* Only turn Underline on */

#define C_DK	"\033[0;30m"    /* Dark Black */
#define C_DR	"\033[0;31m"    /* Dark Red */
#define C_DG	"\033[0;32m"    /* Dark Green */
#define C_DY	"\033[0;33m"    /* Dark Yellow */
#define C_DB 	"\033[0;34m"    /* Dark Blue */
#define C_DM 	"\033[0;35m"    /* Dark Magenta */
#define C_DC	"\033[0;36m"    /* Dark Cyan */
#define C_DW	"\033[0;37m"    /* Dark White */

#define C_LK	"\033[1;30m"    /* Light Black */
#define C_LR	"\033[1;31m"    /* Light Red */
#define C_LG	"\033[1;32m"    /* Light Green */
#define C_LY	"\033[1;33m"    /* Light Yellow */
#define C_LB	"\033[1;34m"    /* Light Blue */
#define C_LM	"\033[1;35m"    /* Light Magenta */
#define C_LC	"\033[1;36m"    /* Light Cyan */
#define C_LW	"\033[1;37m"    /* Light White */

#define C_UK	"\033[4;30m"    /* Underline Black */
#define C_UR	"\033[4;31m"    /* Underline Red */
#define C_UG	"\033[4;32m"    /* Underline Green */
#define C_UY	"\033[4;33m"    /* Underline Yellow */
#define C_UB	"\033[4;34m"    /* Underline Blue */
#define C_UM	"\033[4;35m"    /* Underline Magenta */
#define C_UC	"\033[4;36m"    /* Underline Cyan */
#define C_UW	"\033[4;37m"    /* Underline White */

#define C_BK	"\033[40m"	/* Background Black */
#define C_BR	"\033[41m"	/* Background Red */
#define C_BG	"\033[42m"	/* Background Green */
#define C_BY	"\033[43m"	/* Background Yellow */
#define C_BB	"\033[44m"	/* Background Blue */
#define C_BM	"\033[45m"	/* Background Magenta */
#define C_BC	"\033[46m"	/* Background Cyan */
#define C_BW	"\033[47m"	/* Background White */

#define C_256(color)   "\033[38;5;" # color "m"      /* 256 colors */
#define C_B256(color)  "\033[38;5;" # color "m"      /* 256 colors (background) */

#endif
