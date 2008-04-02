/******************************************************************************\
 Plutocracy - Copyright (C) 2008 - Michael Levin

 This program is free software; you can redistribute it and/or modify it under
 the terms of the GNU General Public License as published by the Free Software
 Foundation; either version 2, or (at your option) any later version.

 This program is distributed in the hope that it will be useful, but WITHOUT
 ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details.
\******************************************************************************/

#include "g_common.h"

/******************************************************************************\
 Render the game elements.
\******************************************************************************/
void G_render(void)
{
}

/******************************************************************************\
 Initialize an idle globe.
\******************************************************************************/
void G_init(void)
{
        C_var_unlatch(&g_globe_seed);
        C_var_unlatch(&g_globe_subdiv4);
        R_generate_globe(g_globe_seed.value.n, g_globe_subdiv4.value.n);
}

