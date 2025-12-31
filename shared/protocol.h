#include "gameimpl.h"

typedef enum
{
    MSG_P2S_FRAME_INPUTS = 1,
    MSG_S2P_FRAME_GAME_EVENTS,
    MSG_S2P_INIT_PLAYER,
} MessageType;

typedef struct
{
    uint8_t type;
    int frame;
    uint16_t payload_size;
} __attribute__((packed)) MessageHeader;

typedef struct
{
    GameState state;
    GameEvents events;
    int client_index;
} __attribute__((packed)) InitPlayerPayload;

size_t serialize_init_player(uint8_t *buffer, int frame, const GameState *state, const GameEvents *events, int client_index);
void deserialize_init_player(const uint8_t *buffer, size_t message_size, int *out_frame, GameState *out_state, GameEvents *out_events, int *out_client_index);

typedef struct
{
    int client_index;
    PlayerInput input;
} __attribute__((packed)) P2SGameEventsPayload;

size_t serialize_p2s_frame_inputs(uint8_t *buffer, int frame, int client_index, const PlayerInput *input);
void deserialize_p2s_frame_inputs(const uint8_t *buffer, size_t message_size, int *out_frame, int *out_client_index, PlayerInput *out_input);

typedef struct
{
    GameEvents events;
} __attribute__((packed)) S2PGameEventsPayload;

size_t serialize_s2p_frame_game_events(uint8_t *buffer, int frame, const GameEvents *events);
void deserialize_s2p_frame_game_events(const uint8_t *buffer, size_t message_size, int *out_frame, GameEvents *out_events);
