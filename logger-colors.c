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

#if defined(LOGGER_USE_THREAD)

#include "colors.h"
#include "logger.h"

const logger_line_colors_t logger_colors_bw = {
    .reset = "",
    .level = {
        [LOGGER_LEVEL_ALL] = "",
    },
    .time  = "",
    .date  = "",
    .date_lines = "",
    .thread_name = "",
};

const logger_line_colors_t logger_colors_default = {
    .level       = {
        [LOGGER_LEVEL_ALL]      = "", /* Default. In case we miss one ... */

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
    },
    .reset       = C_RST,
    .time        = "",
    .date        = C_LG,
    .date_lines  = C_DG,
    .thread_name = C_DW,
};

#endif
