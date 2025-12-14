#pragma once
#include <SDL3/SDL_rect.h>
#include <SDL3/SDL_render.h>
#include <cstdint>

template <typename T>
struct MsgTraits;

enum class MsgType : uint8_t {
    Direction                = 1,
    PlayerInput              = 2,
    ChatMessage              = 3,
    Position                 = 4,
    MsgPlayerJoined          = 5,
    MsgPlayerLeft            = 6,
    MsgPlayerIdAssign        = 7,
    MsgPlayerPositionChanged = 8,
    MsgInitialState          = 9,
    MsgSpawnBullet           = 10,
    // Add more types here
};

#pragma pack(push, 1)
struct MsgHeader {
    MsgType  type;
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
struct Health {
    float max {};
    float current { max };
};
#pragma pack(pop)

#pragma pack(push, 1)
struct Damage {
    float value {};
    float crit_value {};
};
#pragma pack(pop)

#pragma pack(push, 1)
struct Range {
    float value {};
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
    uint32_t playerId {};
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

#pragma pack(push, 1)
struct Client {
    char     nick[32];
    uint32_t id;
    Position pos;
};
#pragma pack(pop)

#pragma pack(push, 1)
struct MsgPlayerJoined {
    uint32_t id;
    Position position;
};
#pragma pack(pop)

#pragma pack(push, 1)
struct MsgPlayerLeft {
    uint32_t id;
};
#pragma pack(pop)

#pragma pack(push, 1)
struct MsgPlayerIdAssign {
    uint32_t id;
};
#pragma pack(pop)

#pragma pack(push, 1)
struct MsgPlayerPositionChanged {
    uint32_t id;
    Position position;
};
#pragma pack(pop)

#pragma pack(push, 1)
struct MsgInitialState {
    uint32_t count;
    Client   clients[8];
};
#pragma pack(pop)

#pragma pack(push, 1)
struct MsgSpawnBullet {
    Position pos;
    Direction direction;
    Speed speed;
    Range range;
    Damage damage;
};
#pragma pack(pop)

struct LocalPlayer { };
struct LocalBullet { };
struct PlayerTag { };
struct BulletTag { };
struct PhysicsSystem { };

template <>
struct MsgTraits<Direction> {
    static constexpr MsgType type = MsgType::Direction;
};

template <>
struct MsgTraits<Position> {
    static constexpr MsgType type = MsgType::Position;
};

template <>
struct MsgTraits<MsgPlayerJoined> {
    static constexpr MsgType type = MsgType::MsgPlayerJoined;
};

template <>
struct MsgTraits<MsgPlayerLeft> {
    static constexpr MsgType type = MsgType::MsgPlayerLeft;
};

template <>
struct MsgTraits<MsgPlayerPositionChanged> {
    static constexpr MsgType type = MsgType::MsgPlayerPositionChanged;
};

template <>
struct MsgTraits<MsgPlayerIdAssign> {
    static constexpr MsgType type = MsgType::MsgPlayerIdAssign;
};

template <>
struct MsgTraits<MsgInitialState> {
    static constexpr MsgType type = MsgType::MsgInitialState;
};

template <>
struct MsgTraits<MsgSpawnBullet> {
    static constexpr MsgType type = MsgType::MsgSpawnBullet;
};
