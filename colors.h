/*
 * Copyright (C) 2020 David De Grave <david@ledav.net>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, version 3.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
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

#endif
