#include "gameimpl.h"

typedef enum
{
    MSG_P2S_GAME_EVENTS = 1,
    MSG_S2P_GAME_EVENTS,
    MSG_S2P_INIT_PLAYER,
} MessageType;

typedef struct
{
    uint8_t type;
    uint32_t frame;
    uint16_t payload_size;
} __attribute__((packed)) MessageHeader;

typedef struct
{
    GameState state;
    GameEvents events;
    uint32_t client_index;
} __attribute__((packed)) InitPlayerPayload;

size_t serialize_init_player(uint8_t *buffer, const GameState *state, const GameEvents *events, uint32_t client_index);
void deserialize_init_player(const uint8_t *buffer, size_t message_size, uint32_t *out_frame, GameState *out_state, GameEvents *out_events, uint32_t *out_client_index);

typedef struct
{
    uint32_t client_index;
    PlayerInput input;
} __attribute__((packed)) P2SGameEventsPayload;

size_t serialize_p2s_game_events(uint8_t *buffer, uint32_t frame, uint32_t client_index, const GameEvents *events);
void deserialize_p2s_game_events(const uint8_t *buffer, size_t message_size, uint32_t *out_frame, uint32_t *out_client_index, PlayerInput *out_input);

typedef struct
{
    PlayerInput player_inputs[MAX_CLIENTS];
} __attribute__((packed)) S2PGameEventsPayload;

size_t serialize_s2p_game_events(uint8_t *buffer, uint32_t frame, const GameEvents *events);
void deserialize_s2p_game_events(const uint8_t *buffer, size_t message_size, uint32_t *out_frame, GameEvents *out_events);
