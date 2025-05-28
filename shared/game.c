#include "game.h"

void simulate(const GameState *current, const GameEvents *events, GameState *out)
{
    *out = *current;

    for (size_t i = 0; i < events->player_event_count; ++i)
    {
        const PlayerEvent *event = &events->player_event[i];
        switch (event->type)
        {
        case PLAYER_EVENT_PLAYER_JOINED:
            if (event->player_id < MAX_PLAYERS)
            {
                out->player_data[event->player_id].active = true;
                out->player_data[event->player_id].x = 400;
                out->player_data[event->player_id].y = 400;
            }
            break;
        case PLAYER_EVENT_PLAYER_LEFT:
            if (event->player_id < MAX_PLAYERS)
            {
                out->player_data[event->player_id].active = false;
            }
            break;
        }
    }

    for (size_t i = 0; i < MAX_PLAYERS; ++i)
    {
        if (!out->player_data[i].active)
        {
            continue;
        }

        if (events->player_controls[i].movements_held[0])
        {
            out->player_data[i].x -= 1.0f;
        }
        else if (events->player_controls[i].movements_held[1])
        {
            out->player_data[i].x += 1.0f;
        }
        if (events->player_controls[i].movements_held[2])
        {
            out->player_data[i].y -= 1.0f;
        }
        else if (events->player_controls[i].movements_held[3])
        {
            out->player_data[i].y += 1.0f;
        }
    }
}
