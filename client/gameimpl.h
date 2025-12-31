#pragma once

#include "../shared/gameimpl.h"

void game_handle_events(GameState *game_state, GameEvents *game_events, uint32_t client_index);

void game_render(const GameState *game_state, uint32_t client_index);
