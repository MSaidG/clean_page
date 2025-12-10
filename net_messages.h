#pragma once
#include <SDL3/SDL_rect.h>
#include <SDL3/SDL_render.h>
#include <cstdint>

enum class MsgType : uint8_t {
    Direction = 1,
    PlayerInput = 2,
    ChatMessage = 3,
    // Add more types here
};

#pragma pack(push, 1)
struct MsgHeader {
    MsgType type;
    uint16_t size; // Size of payload (bytes)
};
#pragma pack(pop)

// Example payload struct
#pragma pack(push, 1)
struct Direction {
    float x;
    float y;
}; 
#pragma pack(pop)

#pragma pack(push, 1)
struct Vec2 {
    float x {};
    float y {};
};
#pragma pack(pop)

#pragma pack(push, 1)
struct Position {
    float x {};
    float y {};
};
#pragma pack(pop)

#pragma pack(push, 1)
struct Velocity {
    float x {};
    float y {};
};
#pragma pack(pop)

#pragma pack(push, 1)
struct Speed {
    float speed {};
};
#pragma pack(pop)

#pragma pack(push, 1)
struct Texture {
    SDL_Texture* texture {};
};
#pragma pack(pop)

#pragma pack(push, 1)
struct RectF {
    SDL_FRect rect {};
};
#pragma pack(pop)

#pragma pack(push, 1)
struct PlayerId {
    int playerId {};
};
#pragma pack(pop)

#pragma pack(push, 1)
struct Camera {
    float x {};
    float y {};
    float w {};
    float h {};
};
#pragma pack(pop)

struct LocalPlayer { };
struct PlayerTag { };
