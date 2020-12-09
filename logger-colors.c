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

#include "colors.h"
#include "logger-colors.h"
#include "logger.h"

const char * const _logger_level_color[LOGGER_LEVEL_COUNT] = {
    [LOGGER_LEVEL_EMERG]    = C_BR C_LW,
    [LOGGER_LEVEL_ALERT]    = C_UNL C_LR,
    [LOGGER_LEVEL_CRITICAL] = C_LR,
    [LOGGER_LEVEL_ERROR]    = C_DR,
    [LOGGER_LEVEL_WARNING]  = C_DY,
    [LOGGER_LEVEL_NOTICE]   = C_DW,
    [LOGGER_LEVEL_INFO]     = C_DB,
    [LOGGER_LEVEL_DEBUG]    = C_DM,
    [LOGGER_LEVEL_OKAY]     = C_DG,
    [LOGGER_LEVEL_TRACE]    = C_DC,
    [LOGGER_LEVEL_OOPS]     = C_LW,
};

const _logger_line_colors_t _logger_line_no_colors = {
    .reset = "",
    .level = "",
    .time  = "",
    .date  = "",
    .date_lines = "",
    .thread_name = "",
};

const _logger_line_colors_t _logger_line_colors = {
    .reset       = C_RST,
    .time        = C_RST,
    .date        = C_DG,
    .date_lines  = C_LG,
    .thread_name = C_DW,
};
