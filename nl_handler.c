/*
 * Copyright (C) 2013 Vadim Kochan <vadim4j@gmail.com>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 * 
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "nl_handler.h"
#include "utils.h"
#include "log.h"

int nl_handlers_init(nl_handler_t **hlist)
{
    int i;

    for (i = 0; hlist[i]; i++)
    {
        if (hlist[i]->do_init)
            hlist[i]->do_init();
    }

    return 0;
}

void nl_handlers_cleanup(nl_handler_t **hlist)
{
    int i;

    for (i = 0; hlist[i]; i++)
    {
        if (hlist[i]->do_cleanup)
            hlist[i]->do_cleanup();
    }
}
