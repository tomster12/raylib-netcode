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
    uint32_t client_index;
    PlayerInput input;
} __attribute__((packed)) PlayerEventsPayload;

typedef struct
{
    PlayerInput player_inputs[MAX_CLIENTS];
} __attribute__((packed)) FrameEventsPayload;

typedef struct
{
    uint32_t assigned_client_index;
} __attribute__((packed)) AssignIdPayload;

typedef struct
{
    uint32_t client_index;
} __attribute__((packed)) PlayerJoinedLeftPayload;

size_t serialize_game_events(uint8_t *buffer, uint32_t frame, uint32_t client_index, const GameEvents *events);
void deserialize_game_events(const uint8_t *buffer, size_t size, uint32_t *out_frame, uint32_t *out_client_index, PlayerInput *out_input);

size_t serialize_frame_events(uint8_t *buffer, uint32_t frame, const GameEvents *events);
void deserialize_frame_events(const uint8_t *buffer, size_t size, uint32_t *out_frame, GameEvents *out_events);

size_t serialize_init_player(uint8_t *buffer, uint32_t client_index);
void deserialize_init_player(const uint8_t *buffer, size_t size, uint32_t *out_client_index);

size_t serialize_player_joined(uint8_t *buffer, uint32_t client_index);
size_t serialize_player_left(uint8_t *buffer, uint32_t client_index);
void deserialize_player_joined_left(const uint8_t *buffer, size_t size, MessageType expected_type, uint32_t *out_client_index);