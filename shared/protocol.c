#include "protocol.h"
#include <arpa/inet.h>
#include <assert.h>
#include <stdio.h>
#include <string.h>

// MSG_P2S_FRAME_INPUTS

size_t serialize_p2s_frame_inputs(uint8_t *buffer, int frame, int client_index, const PlayerInput *input)
{
    MessageHeader header;
    header.type = MSG_P2S_FRAME_INPUTS;
    header.frame = htonl(frame);
    header.payload_size = htons(sizeof(P2SGameEventsPayload));

    P2SGameEventsPayload payload;
    payload.client_index = htonl(client_index);
    payload.input = *input;

    size_t offset = 0;
    memcpy(buffer + offset, &header, sizeof(header));
    offset += sizeof(header);

    memcpy(buffer + offset, &payload, sizeof(payload));
    offset += sizeof(payload);

    return offset;
}

void deserialize_p2s_frame_inputs(const uint8_t *buffer, size_t message_size, int *out_frame, int *out_client_index, PlayerInput *out_input)
{
    size_t offset = 0;

    assert(message_size >= sizeof(MessageHeader));

    MessageHeader header;
    memcpy(&header, buffer + offset, sizeof(header));
    offset += sizeof(header);

    assert(header.type == MSG_P2S_FRAME_INPUTS);

    uint16_t payload_size = ntohs(header.payload_size);
    assert(payload_size == sizeof(P2SGameEventsPayload));
    assert(message_size >= sizeof(MessageHeader) + payload_size);

    P2SGameEventsPayload payload;
    memcpy(&payload, buffer + offset, sizeof(payload));

    *out_frame = ntohl(header.frame);
    *out_client_index = ntohl(payload.client_index);
    *out_input = payload.input;
}

// MSG_S2P_FRAME_GAME_EVENTS

size_t serialize_s2p_frame_game_events(uint8_t *buffer, int frame, const GameEvents *events)
{
    MessageHeader header;
    header.type = MSG_S2P_FRAME_GAME_EVENTS;
    header.frame = htonl(frame);
    header.payload_size = htons(sizeof(S2PGameEventsPayload));

    S2PGameEventsPayload payload;
    payload.events = *events;

    size_t offset = 0;
    memcpy(buffer + offset, &header, sizeof(header));
    offset += sizeof(header);

    memcpy(buffer + offset, &payload, sizeof(payload));
    offset += sizeof(payload);

    return offset;
}

void deserialize_s2p_frame_game_events(const uint8_t *buffer, size_t message_size, int *out_frame, GameEvents *out_events)
{
    size_t offset = 0;

    assert(message_size >= sizeof(MessageHeader));

    MessageHeader header;
    memcpy(&header, buffer + offset, sizeof(header));
    offset += sizeof(header);

    assert(header.type == MSG_S2P_FRAME_GAME_EVENTS);

    uint16_t payload_size = ntohs(header.payload_size);
    assert(payload_size == sizeof(S2PGameEventsPayload));
    assert(message_size >= sizeof(MessageHeader) + payload_size);

    S2PGameEventsPayload payload;
    memcpy(&payload, buffer + offset, sizeof(payload));

    *out_frame = ntohl(header.frame);
    *out_events = payload.events;
}

// MSG_S2P_INIT_PLAYER

size_t serialize_init_player(uint8_t *buffer, int frame, const GameState *state, const GameEvents *events, int client_index)
{
    MessageHeader header;
    header.type = MSG_S2P_INIT_PLAYER;
    header.frame = htonl(frame);
    header.payload_size = htons(sizeof(InitPlayerPayload));

    InitPlayerPayload payload;
    payload.state = *state;
    payload.events = *events;
    payload.client_index = htonl(client_index);

    size_t offset = 0;
    memcpy(buffer + offset, &header, sizeof(header));
    offset += sizeof(header);

    memcpy(buffer + offset, &payload, sizeof(payload));
    offset += sizeof(payload);

    return offset;
}

void deserialize_init_player(const uint8_t *buffer, size_t message_size, int *out_frame, GameState *out_state, GameEvents *out_events, int *out_client_index)
{
    size_t offset = 0;

    assert(message_size >= sizeof(MessageHeader));

    MessageHeader header;
    memcpy(&header, buffer + offset, sizeof(header));
    offset += sizeof(header);

    assert(header.type == MSG_S2P_INIT_PLAYER);

    uint16_t payload_size = ntohs(header.payload_size);
    assert(payload_size == sizeof(InitPlayerPayload));
    assert(message_size >= sizeof(MessageHeader) + payload_size);

    InitPlayerPayload payload;
    memcpy(&payload, buffer + offset, sizeof(payload));

    *out_frame = ntohl(header.frame);
    *out_state = payload.state;
    *out_events = payload.events;
    *out_client_index = ntohl(payload.client_index);
}
