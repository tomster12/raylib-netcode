#include "gameimpl.h"

typedef enum
{
    MSG_P2S_PLAYER_EVENTS = 1,
    MSG_S2P_FRAME_EVENTS,
    MSG_SB_PLAYER_JOINED,
    MSG_SB_PLAYER_LEFT,
    MSG_S2P_ASSIGN_ID,
} MessageType;

typedef struct
{
    uint8_t type;
    uint32_t frame;
    uint16_t payload_size;
} __attribute__((packed)) MessageHeader;

typedef struct
{
    uint32_t player_id;
    PlayerInput input;
} __attribute__((packed)) PlayerEventsPayload;

typedef struct
{
    PlayerInput player_inputs[MAX_CLIENTS];
} __attribute__((packed)) FrameEventsPayload;

typedef struct
{
    uint32_t assigned_player_id;
} __attribute__((packed)) AssignIdPayload;

typedef struct
{
    uint32_t player_id;
} __attribute__((packed)) PlayerJoinedLeftPayload;

size_t serialize_player_events(uint8_t *buffer, uint32_t frame, uint32_t player_id, const PlayerInput *input);
void deserialize_player_events(const uint8_t *buffer, size_t size, uint32_t *out_frame, uint32_t *out_player_id, PlayerInput *out_input);

size_t serialize_frame_events(uint8_t *buffer, uint32_t frame, const GameEvents *events);
void deserialize_frame_events(const uint8_t *buffer, size_t size, uint32_t *out_frame, GameEvents *out_events);

size_t serialize_assign_id(uint8_t *buffer, uint32_t player_id);
void deserialize_assign_id(const uint8_t *buffer, size_t size, uint32_t *out_player_id);

size_t serialize_player_joined(uint8_t *buffer, uint32_t player_id);
size_t serialize_player_left(uint8_t *buffer, uint32_t player_id);
void deserialize_player_joined_left(const uint8_t *buffer, size_t size, MessageType expected_type, uint32_t *out_player_id);