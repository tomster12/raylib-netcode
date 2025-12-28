#include "protocol.h"
#include <arpa/inet.h>
#include <assert.h>
#include <string.h>

// MSG_P2S_GAME_EVENTS

size_t serialize_game_events(uint8_t *buffer, uint32_t frame, uint32_t client_index, const GameEvents *events)
{
    size_t offset = 0;

    MessageHeader header = {
        .type = MSG_P2S_GAME_EVENTS,
        .frame = htonl(frame),
        .payload_size = htons(sizeof(PlayerEventsPayload)),
    };
    memcpy(buffer + offset, &header, sizeof(header));
    offset += sizeof(header);

    PlayerEventsPayload payload = {
        .client_index = htonl(client_index),
        .input = events->player_inputs[client_index],
    };
    memcpy(buffer + offset, &payload, sizeof(payload));
    offset += sizeof(payload);

    return offset;
}

void deserialize_game_events(const uint8_t *buffer, size_t size, uint32_t *out_frame, uint32_t *out_client_index, PlayerInput *out_input)
{
    assert(size >= sizeof(MessageHeader) + sizeof(PlayerEventsPayload));

    size_t offset = 0;

    MessageHeader header;
    memcpy(&header, buffer + offset, sizeof(header));
    offset += sizeof(header);
    assert(header.type == MSG_P2S_GAME_EVENTS);

    PlayerEventsPayload payload;
    memcpy(&payload, buffer + offset, sizeof(payload));

    *out_frame = ntohl(header.frame);
    *out_client_index = ntohl(payload.client_index);
    *out_input = payload.input;
}

// MSG_S2P_GAME_EVENTS

size_t serialize_frame_events(uint8_t *buffer, uint32_t frame, const GameEvents *events)
{
    size_t offset = 0;

    MessageHeader header = {
        .type = MSG_S2P_GAME_EVENTS,
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
    assert(header.type == MSG_S2P_GAME_EVENTS);

    FrameEventsPayload payload;
    memcpy(&payload, buffer + offset, sizeof(payload));

    *out_frame = ntohl(header.frame);
    memcpy(out_events->player_inputs, payload.player_inputs, sizeof(payload.player_inputs));
}

// MSG_S2P_INIT_PLAYER

size_t serialize_init_player(uint8_t *buffer, uint32_t client_index)
{
    size_t offset = 0;

    MessageHeader header = {
        .type = MSG_S2P_INIT_PLAYER,
        .frame = 0,
        .payload_size = htons(sizeof(AssignIdPayload)),
    };
    memcpy(buffer + offset, &header, sizeof(header));
    offset += sizeof(header);

    AssignIdPayload payload = {
        .assigned_client_index = htonl(client_index)};
    memcpy(buffer + offset, &payload, sizeof(payload));
    offset += sizeof(payload);

    return offset;
}

void deserialize_init_player(const uint8_t *buffer, size_t size, uint32_t *out_client_index)
{
    assert(size >= sizeof(MessageHeader) + sizeof(AssignIdPayload));

    size_t offset = 0;

    MessageHeader header;
    memcpy(&header, buffer + offset, sizeof(header));
    offset += sizeof(header);
    assert(header.type == MSG_S2P_INIT_PLAYER);

    AssignIdPayload payload;
    memcpy(&payload, buffer + offset, sizeof(payload));

    *out_client_index = ntohl(payload.assigned_client_index);
}
