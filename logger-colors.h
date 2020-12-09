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

#ifndef _LOGGER_COLORS_H
#define _LOGGER_COLORS_H

#include "colors.h"
#include "logger.h"

typedef struct {
    const char *reset;
    const char *level;
    const char *time;
    const char *date;
    const char *date_lines;
    const char *thread_name;
} _logger_line_colors_t;

extern const char * const _logger_level_color[LOGGER_LEVEL_COUNT];

extern const _logger_line_colors_t _logger_line_colors;
extern const _logger_line_colors_t _logger_line_no_colors;

#endif
