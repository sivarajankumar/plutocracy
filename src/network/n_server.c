/******************************************************************************\
 Plutocracy - Copyright (C) 2008 - Michael Levin

 This program is free software; you can redistribute it and/or modify it under
 the terms of the GNU General Public License as published by the Free Software
 Foundation; either version 2, or (at your option) any later version.

 This program is distributed in the hope that it will be useful, but WITHOUT
 ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details.
\******************************************************************************/

#include "n_common.h"

/* Client slots array */
n_client_t n_clients[N_CLIENTS_MAX];

/******************************************************************************\
 Open server sockets and begin accepting connections.
\******************************************************************************/
void N_start_server(n_callback_f server_func, n_callback_f client_func)
{
        n_client_id = N_HOST_CLIENT_ID;
        n_server_func = server_func;
        n_client_func = client_func;
        C_zero(&n_clients);

        /* Setup the host's client */
        n_clients[N_HOST_CLIENT_ID].connected = TRUE;
        n_server_func(N_HOST_CLIENT_ID, N_EV_CONNECTED);
}

/******************************************************************************\
 Close server sockets and stop accepting connections.
\******************************************************************************/
void N_stop_server(void)
{
        if (n_client_id != N_HOST_CLIENT_ID)
                return;
}

/******************************************************************************\
 Poll connections and dispatch any messages that arrive.
\******************************************************************************/
void N_poll_server(void)
{
        if (n_client_id != N_HOST_CLIENT_ID)
                return;
}

/******************************************************************************\
 Disconnect a client from the server.
\******************************************************************************/
void N_kick_client(int client)
{
        n_clients[client].connected = FALSE;
        if (client == n_client_id) {
                N_disconnect();
                N_stop_server();
        }
}

