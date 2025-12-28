#pragma once

#include "shared/gameimpl.h"

void game_handle_events(GameState *game_state, GameEvents *game_events, int client_index);

void game_render(const GameState *game_state, int client_index);
