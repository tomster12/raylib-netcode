#pragma once

#include "implcore.h"

void game_handle_events(GameState *game_state, GameEvents *game_events, int local_player_id);

void game_render(const GameState *game_state, int local_player_id);
