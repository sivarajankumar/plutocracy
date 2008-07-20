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

/* The currently selected tile and ship index. Negative values indicate an
   invalid selection. */
int g_selected_tile, g_selected_ship;

/* Array of connected clients */
g_client_t g_clients[N_CLIENTS_MAX];

/* Array of game nations */
g_nation_t g_nations[G_NATION_NAMES];

/* Array of cargo names */
const char *g_cargo_names[G_CARGO_TYPES];

static bool ring_valid;

/******************************************************************************\
 Initialize game structures before play.
\******************************************************************************/
void G_init(void)
{
        C_status("Initializing game elements");
        G_init_ships();

        /* Setup nations */
        g_nations[G_NN_RED].short_name = "red";
        g_nations[G_NN_RED].long_name = C_str("g-nation-red", "Ruby");
        C_var_update_data(g_nation_colors + G_NN_RED, C_color_update,
                          &g_nations[G_NN_RED].color);
        g_nations[G_NN_GREEN].short_name = "green";
        g_nations[G_NN_GREEN].long_name = C_str("g-nation-green", "Emerald");
        C_var_update_data(g_nation_colors + G_NN_GREEN, C_color_update,
                          &g_nations[G_NN_GREEN].color);
        g_nations[G_NN_BLUE].short_name = "blue";
        g_nations[G_NN_BLUE].long_name = C_str("g-nation-blue", "Sapphire");
        C_var_update_data(g_nation_colors + G_NN_BLUE, C_color_update,
                          &g_nations[G_NN_BLUE].color);
        g_nations[G_NN_PIRATE].short_name = "pirate";
        g_nations[G_NN_PIRATE].long_name = C_str("g-nation-pirate", "Pirate");
        C_var_update_data(g_nation_colors + G_NN_PIRATE, C_color_update,
                          &g_nations[G_NN_PIRATE].color);

        /* Special cargo */
        g_cargo_names[G_CT_GOLD] = C_str("g-cargo-gold", "Gold");
        g_cargo_names[G_CT_CREW] = C_str("g-cargo-crew", "Crew");

        /* Food cargo */
        g_cargo_names[G_CT_RATIONS] = C_str("g-cargo-rations", "Rations");
        g_cargo_names[G_CT_BREAD] = C_str("g-cargo-bread", "Bread");
        g_cargo_names[G_CT_FISH] = C_str("g-cargo-fish", "Fish");
        g_cargo_names[G_CT_FRUIT] = C_str("g-cargo-fruit", "Fruit");

        /* Raw material cargo */
        g_cargo_names[G_CT_GRAIN] = C_str("g-cargo-grain", "Grain");
        g_cargo_names[G_CT_COTTON] = C_str("g-cargo-cotton", "Cotton");
        g_cargo_names[G_CT_LUMBER] = C_str("g-cargo-lumber", "Lumber");
        g_cargo_names[G_CT_IRON] = C_str("g-cargo-iron", "Iron");

        /* Luxury cargo */
        g_cargo_names[G_CT_RUM] = C_str("g-cargo-rum", "Rum");
        g_cargo_names[G_CT_FURNITURE] = C_str("g-cargo-furniture", "Furniture");
        g_cargo_names[G_CT_CLOTH] = C_str("g-cargo-cloth", "Cloth");
        g_cargo_names[G_CT_SUGAR] = C_str("g-cargo-sugar", "Sugar");

        /* Equipment cargo */
        g_cargo_names[G_CT_CANNON] = C_str("g-cargo-cannon", "Cannon");
        g_cargo_names[G_CT_ROUNDSHOT] = C_str("g-cargo-roundshot", "Roundshot");
        g_cargo_names[G_CT_PLATING] = C_str("g-cargo-plating", "Plating");
        g_cargo_names[G_CT_GILLNET] = C_str("g-cargo-gillner", "Gillnet");

        /* Prepare initial state */
        G_init_globe();
}

/******************************************************************************\
 Cleanup game structures.
\******************************************************************************/
void G_cleanup(void)
{
        G_cleanup_globe();
        G_cleanup_ships();
}

/******************************************************************************\
 Returns the amount of space the given cargo manifest contains.
\******************************************************************************/
int G_cargo_space(const g_cargo_t *cargo)
{
        int i, space;

        space = 0;
        for (i = 0; i < G_CARGO_TYPES; i++)
                space += cargo->amounts[i];

        /* Gold is special */
        space += cargo->amounts[G_CT_GOLD] / 100 - cargo->amounts[G_CT_GOLD];

        return space;
}

/******************************************************************************\
 Disconnect if the server has sent corrupted data.
\******************************************************************************/
static void corrupt_disconnect(void)
{
        I_popup(NULL, "Server sent invalid data, disconnected.");
        N_disconnect();
}

/******************************************************************************\
 Client is receving a popup message from the server.
\******************************************************************************/
static void client_popup(void)
{
        c_vec3_t *goto_pos;
        int focus_tile;
        char message[256];

        focus_tile = N_receive_short();
        if (focus_tile >= r_tiles) {
                corrupt_disconnect();
                return;
        }
        goto_pos = focus_tile >= 0 ? &g_tiles[focus_tile].origin : NULL;
        N_receive_string(message, sizeof (message));
        I_popup(goto_pos, message);
}

/******************************************************************************\
 Some client changed their national affiliation.
\******************************************************************************/
static void client_affiliate(void)
{
        c_vec3_t *goto_pos;
        const char *fmt;
        int client, nation, tile;

        client = N_receive_char();
        nation = N_receive_char();
        tile = N_receive_short();
        if (nation < 0 || nation >= G_NATION_NAMES || client < 0 ||
            client >= N_CLIENTS_MAX || !n_clients[client].connected) {
                corrupt_disconnect();
                return;
        }
        g_clients[client].nation = nation;
        if (client == n_client_id)
                I_select_nation(nation);

        /* Affiliation message might have a goto tile attached */
        goto_pos = NULL;
        if (tile >= 0 && tile < r_tiles)
                goto_pos = &g_tiles[tile].origin;

        /* Special case for pirates */
        if (nation == G_NN_PIRATE) {
                if (client == n_client_id) {
                        I_popup(goto_pos, C_str("g-join-pirate",
                                                "You became a pirate."));
                        return;
                }
                fmt = C_str("g-join-pirate-client", "%s became a pirate.");
                I_popup(goto_pos, C_va(fmt, g_clients[client].name));
                return;
        }

        /* Message for nations */
        if (client == n_client_id) {
                fmt = C_str("g-join-nation-you", "You joined the %s nation.");
                I_popup(goto_pos, C_va(fmt, g_nations[nation].long_name));
        } else {
                fmt = C_str("g-join-nation-client", "%s joined the %s nation.");
                I_popup(goto_pos, C_va(fmt, g_clients[client].name,
                                       g_nations[nation].long_name));
        }
}

/******************************************************************************\
 Client just connected to the server or the server has re-hosted.
\******************************************************************************/
static void client_init(void)
{
        int protocol, seed;
        const char *msg;

        /* Check the server's protocol */
        protocol = N_receive_char();
        if (protocol != G_PROTOCOL) {
                msg = C_va("Server protocol (%d) does not match "
                           "client protocol (%d), disconnecting.",
                           protocol, G_PROTOCOL);
                I_popup(NULL, C_str("g-incompatible", msg));
                N_disconnect();
                return;
        }

        /* Generate a new globe to match the server's */
        seed = N_receive_int();
        if (seed != g_globe_seed.value.n) {
                g_globe_seed.value.n = seed;
                G_generate_globe();
        }

        /* Start off nation-less */
        I_select_nation(G_NN_NONE);
}

/******************************************************************************\
 Client network event callback function.
\******************************************************************************/
void G_client_callback(int client, n_event_t event)
{
        g_server_msg_t token;

        C_assert(client == N_SERVER_ID);

        /* Disconnected */
        if (event == N_EV_DISCONNECTED) {
                I_enter_limbo();
                return;
        }

        /* Process messages */
        if (event != N_EV_MESSAGE)
                return;
        token = N_receive_char();
        switch (token) {
        case G_SM_POPUP:
                client_popup();
                break;
        case G_SM_AFFILIATE:
                client_affiliate();
                break;
        case G_SM_INIT:
                client_init();
                break;
        default:
                break;
        }
}

/******************************************************************************\
 Leave the current game.
\******************************************************************************/
void G_leave_game(void)
{
        N_disconnect();
        N_stop_server();
        I_popup(NULL, "Left the current game.");
}

/******************************************************************************\
 Change the player's nation.
\******************************************************************************/
void G_change_nation(int index)
{
        N_send(N_SERVER_ID, "11", G_CM_AFFILIATE, index);
}

/******************************************************************************\
 Returns TRUE if there are possible ring interactions with the [tile].
\******************************************************************************/
static int can_interact(int tile)
{
        if (g_test_globe.value.n)
                return TRUE;
        return FALSE;
}

/******************************************************************************\
 Selects a tile during mouse hover. Pass -1 to deselect.
\******************************************************************************/
void G_select_tile(int tile)
{
        C_assert(tile < r_tiles);

        /* Deselect the old tile */
        if (g_selected_tile >= 0)
                g_tiles[g_selected_tile].model.selected = FALSE;
        R_select_tile(g_selected_tile = -1, R_ST_NONE);

        /* See if there is actually any interaction possible with this tile */
        ring_valid = FALSE;
        if (tile < 0)
                return;
        ring_valid = can_interact(tile);

        /* Selecting a ship */
        if (g_tiles[tile].ship >= 0) {
        }

        /* If we are controlling a ship, we might want to move here */
        else if (g_selected_ship >= 0 && G_open_tile(tile, -1) &&
                 g_ships[g_selected_ship].client == n_client_id) {
                R_select_tile(tile, R_ST_GOTO);
                g_selected_tile = tile;
                return;
        }

        /* Select a tile */
        else if (ring_valid)
                R_select_tile(tile, R_ST_TILE);

        /* Can't select this */
        else {
                R_select_tile(-1, R_ST_NONE);
                g_selected_tile = -1;
                return;
        }

        g_tiles[tile].model.selected = TRUE;
        g_selected_tile = tile;
}

/******************************************************************************\
 Test ring callback function.
\******************************************************************************/
static void test_ring_callback(i_ring_icon_t icon)
{
        if (icon == I_RI_TEST_MILL)
                G_set_tile_model(g_selected_tile, "models/test/mill.plum");
        else if (icon == I_RI_TEST_TREE)
                G_set_tile_model(g_selected_tile,
                               "models/tree/deciduous.plum");
        else if (icon == I_RI_TEST_SHIP)
                G_set_tile_model(g_selected_tile,
                               "models/ship/sloop.plum");
        else
                G_set_tile_model(g_selected_tile, "");
}

/******************************************************************************\
 Select a ship. Pass a negative [index] to deselect.
\******************************************************************************/
static void select_ship(int index)
{
        bool own;

        if (g_selected_ship == index)
                return;
        g_selected_ship = index;
        own = FALSE;
        if (index >= 0) {
                R_select_path(g_ships[index].tile, g_ships[index].path);
                own = g_ships[index].client == n_client_id;
        } else
                R_select_path(-1, NULL);
        I_select_ship(index, own);
}

/******************************************************************************\
 Called when the interface root window receives a click.
\******************************************************************************/
void G_process_click(int button)
{
        /* The game only handles left and middle clicks */
        if (button != SDL_BUTTON_LEFT && button != SDL_BUTTON_MIDDLE)
                return;

        /* Clicking on an unusable space deselects */
        if (g_selected_tile < 0 || button != SDL_BUTTON_LEFT) {
                select_ship(-1);
                return;
        }

        /* Left-clicked on a ship */
        if (g_tiles[g_selected_tile].ship >= 0) {
                if (g_selected_ship != g_tiles[g_selected_tile].ship)
                        select_ship(g_tiles[g_selected_tile].ship);
                else
                        select_ship(-1);
                return;
        }

        /* Controlling a ship */
        if (g_selected_ship >= 0 &&
            g_ships[g_selected_ship].client == n_client_id) {

                /* Ordered an ocean move */
                if (g_selected_tile >= 0 && G_open_tile(g_selected_tile, -1)) {
                        N_send(N_SERVER_ID, "112", G_CM_SHIP_MOVE,
                               g_selected_ship, g_selected_tile);
                        return;
                }
        }

        /* Left-clicked on a tile */
        select_ship(-1);
        if (!ring_valid)
                return;

        /* When globe testing is on, always show the test ring */
        if (g_test_globe.value.n) {
                I_reset_ring();
                I_add_to_ring(I_RI_TEST_BLANK, TRUE);
                I_add_to_ring(I_RI_TEST_MILL, TRUE);
                I_add_to_ring(I_RI_TEST_TREE, TRUE);
                I_add_to_ring(I_RI_TEST_SHIP, TRUE);
                I_add_to_ring(I_RI_TEST_DISABLED, FALSE);
                I_show_ring(test_ring_callback);
        }
}

/******************************************************************************\
 Called to update client-side structures.
\******************************************************************************/
void G_update_client(void)
{
        if (i_limbo)
                return;
        G_update_ships();
}
