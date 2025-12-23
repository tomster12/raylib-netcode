#include "protocol.h"
#include <arpa/inet.h>
#include <assert.h>
#include <string.h>

// MSG_P2S_PLAYER_EVENTS

size_t serialize_player_events(uint8_t *buffer, uint32_t frame, uint32_t player_id, const GameEvents *events)
{
    size_t offset = 0;

    MessageHeader header = {
        .type = MSG_P2S_PLAYER_EVENTS,
        .frame = htonl(frame),
        .payload_size = htons(sizeof(PlayerEventsPayload)),
    };
    memcpy(buffer + offset, &header, sizeof(header));
    offset += sizeof(header);

    PlayerEventsPayload payload = {
        .player_id = htonl(player_id),
        .input = events->player_inputs[player_id],
    };
    memcpy(buffer + offset, &payload, sizeof(payload));
    offset += sizeof(payload);

    return offset;
}

void deserialize_player_events(const uint8_t *buffer, size_t size, uint32_t *out_frame, uint32_t *out_player_id, PlayerInput *out_input)
{
    assert(size >= sizeof(MessageHeader) + sizeof(PlayerEventsPayload));

    size_t offset = 0;

    MessageHeader header;
    memcpy(&header, buffer + offset, sizeof(header));
    offset += sizeof(header);
    assert(header.type == MSG_P2S_PLAYER_EVENTS);

    PlayerEventsPayload payload;
    memcpy(&payload, buffer + offset, sizeof(payload));

    *out_frame = ntohl(header.frame);
    *out_player_id = ntohl(payload.player_id);
    *out_input = payload.input;
}

// MSG_S2P_FRAME_EVENTS

size_t serialize_frame_events(uint8_t *buffer, uint32_t frame, const GameEvents *events)
{
    size_t offset = 0;

    MessageHeader header = {
        .type = MSG_S2P_FRAME_EVENTS,
        .frame = htonl(frame),
        .payload_size = htons(sizeof(FrameEventsPayload)),
    };
    memcpy(buffer + offset, &header, sizeof(header));
    offset += sizeof(header);

    FrameEventsPayload payload;
    memcpy(&payload.player_inputs, events->player_inputs, sizeof(events->player_inputs));
    memcpy(buffer + offset, &payload, sizeof(payload));
    offset += sizeof(payload);

    return offset;
}

void deserialize_frame_events(const uint8_t *buffer, size_t size, uint32_t *out_frame, GameEvents *out_events)
{
    assert(size >= sizeof(MessageHeader) + sizeof(FrameEventsPayload));

    size_t offset = 0;

    MessageHeader header;
    memcpy(&header, buffer + offset, sizeof(header));
    offset += sizeof(header);
    assert(header.type == MSG_S2P_FRAME_EVENTS);

    FrameEventsPayload payload;
    memcpy(&payload, buffer + offset, sizeof(payload));

    *out_frame = ntohl(header.frame);
    memcpy(out_events->player_inputs, payload.player_inputs, sizeof(payload.player_inputs));
}

// MSG_S2P_ASSIGN_ID

size_t serialize_assign_id(uint8_t *buffer, uint32_t player_id)
{
    size_t offset = 0;

    MessageHeader header = {
        .type = MSG_S2P_ASSIGN_ID,
        .frame = 0,
        .payload_size = htons(sizeof(AssignIdPayload)),
    };
    memcpy(buffer + offset, &header, sizeof(header));
    offset += sizeof(header);

    AssignIdPayload payload = {
        .assigned_player_id = htonl(player_id)};
    memcpy(buffer + offset, &payload, sizeof(payload));
    offset += sizeof(payload);

    return offset;
}

void deserialize_assign_id(const uint8_t *buffer, size_t size, uint32_t *out_player_id)
{
    assert(size >= sizeof(MessageHeader) + sizeof(AssignIdPayload));

    size_t offset = 0;

    MessageHeader header;
    memcpy(&header, buffer + offset, sizeof(header));
    offset += sizeof(header);
    assert(header.type == MSG_S2P_ASSIGN_ID);

    AssignIdPayload payload;
    memcpy(&payload, buffer + offset, sizeof(payload));

    *out_player_id = ntohl(payload.assigned_player_id);
}

// MSG_SB_PLAYER_JOINED / MSG_SB_PLAYER_LEFT

size_t serialize_player_joined(uint8_t *buffer, uint32_t player_id)
{
    size_t offset = 0;

    MessageHeader header = {
        .type = MSG_SB_PLAYER_JOINED,
        .frame = 0,
        .payload_size = htons(sizeof(PlayerJoinedLeftPayload)),
    };
    memcpy(buffer + offset, &header, sizeof(header));
    offset += sizeof(header);

    PlayerJoinedLeftPayload payload = {
        .player_id = htonl(player_id),
    };
    memcpy(buffer + offset, &payload, sizeof(payload));
    offset += sizeof(payload);

    return offset;
}

size_t serialize_player_left(uint8_t *buffer, uint32_t player_id)
{
    size_t offset = 0;

    MessageHeader header = {
        .type = MSG_SB_PLAYER_LEFT,
        .frame = 0,
        .payload_size = htons(sizeof(PlayerJoinedLeftPayload)),
    };
    memcpy(buffer + offset, &header, sizeof(header));
    offset += sizeof(header);

    PlayerJoinedLeftPayload payload = {
        .player_id = htonl(player_id),
    };
    memcpy(buffer + offset, &payload, sizeof(payload));
    offset += sizeof(payload);

    return offset;
}

void deserialize_player_joined_left(const uint8_t *buffer, size_t size, MessageType expected_type, uint32_t *out_player_id)
{
    assert(size >= sizeof(MessageHeader) + sizeof(PlayerJoinedLeftPayload));

    size_t offset = 0;

    MessageHeader header;
    memcpy(&header, buffer + offset, sizeof(header));
    offset += sizeof(header);
    assert(header.type == expected_type);

    PlayerJoinedLeftPayload payload;
    memcpy(&payload, buffer + offset, sizeof(payload));

    *out_player_id = ntohl(payload.player_id);
}