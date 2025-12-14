#include <SDL3/SDL.h>
#include <SDL3/SDL_blendmode.h>
#include <SDL3/SDL_error.h>
#include <SDL3/SDL_events.h>
#include <SDL3/SDL_filesystem.h>
#include <SDL3/SDL_gpu.h>
#include <SDL3/SDL_hints.h>
#include <SDL3/SDL_init.h>
#include <SDL3/SDL_keyboard.h>
#include <SDL3/SDL_keycode.h>
#include <SDL3/SDL_log.h>
#include <SDL3/SDL_oldnames.h>

#include <SDL3/SDL_pixels.h>
#include <SDL3/SDL_properties.h>
#include <SDL3/SDL_rect.h>
#include <SDL3/SDL_render.h>
#include <SDL3/SDL_scancode.h>
#include <SDL3/SDL_stdinc.h>
#include <SDL3/SDL_surface.h>
#include <SDL3/SDL_timer.h>
#include <SDL3/SDL_version.h>
#include <SDL3/SDL_video.h>

#include <SDL3_ttf/SDL_ttf.h>
#include <cstddef>
#include <cstring>
#include <flecs.h>

#include <flecs/addons/cpp/c_types.hpp>
#include <flecs/addons/cpp/entity.hpp>
#include <flecs/addons/cpp/mixins/alerts/impl.hpp>
#include <flecs/addons/cpp/mixins/pipeline/decl.hpp>
#include <flecs/addons/cpp/world.hpp>

#include <cmath>
#include <cstdint>
#include <iostream>
#include <steam/steamnetworkingtypes.h>

#define STB_IMAGE_IMPLEMENTATION
#include "game_client.h"
#include "net_messages.h"
#include "stb_image.h"

#define FLECS_CPP

constexpr int WINDOW_WIDTH { 1360 };
constexpr int WINDOW_HEIGHT { 960 };

constexpr float WORLD_VIEW_WIDTH  = 20.0f;
constexpr float WORLD_VIEW_HEIGHT = 12.0f;
constexpr float GRID_SIZE         = 100.0f; // 1 world unit per cell

constexpr int DEFAULT_PLAYER_SIZE { 128 };

constexpr float BASE_PLAYER_SPEED { 250 };
constexpr float BASE_PLAYER_HEALTH { 100 };

constexpr float BASE_BULLET_SPEED { 500 };
constexpr float BASE_BULLET_DAMAGE { 5 };
constexpr float BASE_BULLET_RANGE { 500 };

void        sdl_init();
void        set_app_metadata();
void        get_error();
void        createTexture(SDL_Texture** texture, int& width, int& height, const char* textureFileName);
const char* dptf_name = "hey_small.png";
const char* dbtf_name = "bullet_14x14.png";

SDL_Window*   m_window {};
SDL_Renderer* m_renderer {};
TTF_Font*     m_font = nullptr;
GameClient    m_game_client;

int m_window_w = WINDOW_WIDTH;
int m_window_h = WINDOW_HEIGHT;

template <typename T>
T normalize_vector(T vec);

template <typename T>
float get_vector_length(T vec);

template <typename T>
void send_data(T data, const int k_n_flag);

SDL_Texture* get_font_texture(SDL_Renderer* renderer, const char* message, SDL_Color color);

flecs::entity create_player(flecs::world ecs, uint32_t id, const char* texture_file_name, Position position, float speed, Health health, bool is_local);
void          create_bullet(flecs::world ecs, const char* texture_file_name, Position position, Direction direction, float speed, Damage damage, Range range, bool is_local);

bool is_in_camera_view(const Camera& cam, const Position obj_position, const float obj_width, const float obj_height);
void poll_keyboard_state(flecs::entity player);
void update_physics(const float dt);

bool load_font();
void render_font(const char* message, float rect_x, float rect_y);

void send_direction_and_position_data_to_server(Direction dir, Position pos);
void disconnect_from_server(flecs::entity player);

std::unordered_map<uint32, flecs::entity> m_players_by_id;

int main(int argc, char* argv[])
{
    flecs::world ecs;

    sdl_init();
    m_game_client.init();
    m_game_client.on_player_joined = [&](uint32_t id, Position pos) {
        std::cout << "Getting joining ...\n";
        auto player = create_player(ecs,
            id,
            dptf_name,
            pos,
            BASE_PLAYER_SPEED,
            Health { BASE_PLAYER_HEALTH },
            false);
        m_players_by_id.insert_or_assign(id, player);
        std::cout << "Player " << id << " joined.\n";
    };

    m_game_client.on_player_left = [&](uint32_t id) {
        std::cout << "Player " << id << " leaving.\n";
        auto player = m_players_by_id[id];
        SDL_DestroyTexture(player.get_mut<Texture>().texture);
        player.destruct();
        m_players_by_id.erase(id);
        std::cout << "Player " << id << " left.\n";
    };

    m_game_client.on_player_id_assigned = [&](uint32_t id) {
        std::cout << "Player id assigning ...\n";
        auto local_player_entity = ecs.lookup("LocalPlayer");
        std::cout << "Player id: " << local_player_entity.get<PlayerId>().playerId << "\n";
        local_player_entity.assign<PlayerId>({ id });
        std::cout << "New Player id: " << local_player_entity.get<PlayerId>().playerId << "\n";
        m_players_by_id.insert_or_assign(id, local_player_entity);
    };

    m_game_client.on_player_position_changed = [&](uint32_t id, Position pos) {
        auto player = m_players_by_id[id];
        player.assign<Position>({ pos });
        RectF r = player.get<RectF>();
        player.assign<RectF>({ pos.x - r.rect.w * 0.5f,
            pos.y - r.rect.h * 0.5f,
            r.rect.w,
            r.rect.h });
    };

    m_game_client.on_players_initial_state_sent = [&](uint32_t count, Client clients[8]) {
        std::cout << "Getting initial state ...\n";
        for (int i = 0; i < count; ++i) {
            std::cout << "Getting player " << clients[i].id << "\n";
            auto player = create_player(ecs,
                clients[i].id,
                dptf_name,
                clients[i].pos,
                BASE_PLAYER_SPEED,
                Health { BASE_PLAYER_HEALTH },
                false);
            m_players_by_id.insert_or_assign(clients[i].id, player);
            std::cout << "Player " << clients[i].id << " in the server.\n";
        }
    };

    auto player_entity = create_player(ecs,
        1,
        dptf_name,
        Position { 0.0f, 0.0f },
        BASE_PLAYER_SPEED,
        Health { BASE_PLAYER_HEALTH },
        true);

    auto camera_entity = ecs.entity("Camera")
                             .set<Camera>({ 0.0f, 0.0f,
                                 WORLD_VIEW_WIDTH,
                                 WORLD_VIEW_HEIGHT });

    ecs.system<Position, RectF, LocalPlayer>()
        .kind(flecs::PreUpdate)
        .each([&, camera_entity](flecs::iter it, size_t row, Position p, RectF r, LocalPlayer) {
            flecs::entity e = it.entity(row);

            Camera cam = { p.x - m_window_w * 0.5f, p.y - m_window_h * 0.5f,
                static_cast<float>(m_window_w), static_cast<float>(m_window_h) };
            camera_entity.assign<Camera>(cam);
        });

    flecs::system player_physics
        = ecs.system<Position, Direction, Speed, PlayerTag>()
              .kind(0) // flecs::OnValidate, flecs::PostUpdate
              .each([](flecs::iter& it, size_t row, Position& p, Direction& d, Speed s, PlayerTag) {
                  flecs::entity e = it.entity(row);
                  if (e.has<LocalPlayer>()) {
                      p.x += d.x * s.speed * it.delta_time();
                      p.y += d.y * s.speed * it.delta_time();
                  }
              });

    flecs::system bullet_physics
        = ecs.system<Position, Direction, Speed, Range, RectF, BulletTag>()
              .kind(0)
              .each([](flecs::iter it, size_t i, Position& p, Direction d, Speed s, Range& range, RectF& r, BulletTag) {
                  if (range.value > 0) {
                      p.x += d.x * s.speed * it.delta_time();
                      p.y += d.y * s.speed * it.delta_time();

                      range.value -= s.speed * it.delta_time();

                      r.rect.x = p.x - r.rect.w * 0.5f;
                      r.rect.y = p.y - r.rect.h * 0.5f;
                  } else {
                      SDL_DestroyTexture(it.entity(i).get_mut<Texture>().texture);
                      it.entity(i).destruct();
                  }

                  // std::cout << "Bullet Range left: " << range.value << "\n";
                  // std::cout << "Bullet Speed: " << s.speed << "\n";
                  // std::cout << "Bullet Px Py: " << p.x << ", " << p.y << "\n";
                  // std::cout << "Bullet delta time: " << it.delta_time() << "\n";
                  // std::cout << "Bullet Dx Dy: " << d.x << ", " << d.y << "\n";
              });

    ecs.system<Position, RectF, Texture>()
        .kind(flecs::OnStore)
        .each([camera_entity](flecs::iter& it, size_t row, Position p, RectF r, Texture t) {
            flecs::entity e = it.entity(row);
            if (e.has<LocalPlayer>()) {
                r.rect.x = m_window_w * 0.5f - r.rect.w * 0.5f;
                r.rect.y = m_window_h * 0.5f - r.rect.h * 0.5f;
            }

            Camera cam = camera_entity.get<Camera>();

            std::string message_to_render = "Px: " + std::to_string(cam.x) + "   Py: " + std::to_string(cam.y);
            render_font(message_to_render.c_str(), 100.0f, 100.0f);

            float camLeft   = cam.x;
            float camRight  = cam.x + cam.w;
            float camTop    = cam.y;
            float camBottom = cam.y + cam.h;

            int startX = (int)std::floor(camLeft / GRID_SIZE);
            int endX   = (int)std::ceil(camRight / GRID_SIZE);

            int startY = (int)std::floor(camTop / GRID_SIZE);
            int endY   = (int)std::ceil(camBottom / GRID_SIZE);

            float scaleX = (float)m_window_w / cam.w;
            float scaleY = (float)m_window_h / cam.h;

            SDL_SetRenderDrawColor(m_renderer, 0, 0, 0, 100);
            for (int x = startX; x <= endX; ++x) {
                float worldX = x * GRID_SIZE;

                float screenX = (worldX - cam.x) * scaleX;

                SDL_RenderLine(
                    m_renderer,
                    (int)screenX, 0,
                    (int)screenX, m_window_h);
            }

            for (int y = startY; y <= endY; ++y) {
                float worldY = y * GRID_SIZE;

                float screenY = (worldY - cam.y) * scaleY;

                SDL_RenderLine(
                    m_renderer,
                    0, (int)screenY,
                    m_window_w, (int)screenY);
            }

            if (is_in_camera_view(cam, p, r.rect.w, r.rect.h)) {
                float     scaleX = (float)m_window_w / cam.w;
                float     scaleY = (float)m_window_h / cam.h;
                SDL_FRect screenRect {
                    (r.rect.x - cam.x) * scaleX,
                    (r.rect.y - cam.y) * scaleY,
                    r.rect.w * scaleX,
                    r.rect.h * scaleY
                };

                if (!e.has<LocalPlayer>()) {
                    SDL_RenderTexture(m_renderer, t.texture, nullptr, &screenRect);
                }
            }

            if (e.has<LocalPlayer>()) {
                SDL_RenderTexture(m_renderer, t.texture, nullptr, &r.rect); //&player.rect
            }

            SDL_SetRenderDrawColor(m_renderer, 100, 20, 20, 255);
        });

    // auto          players = ecs.query<PlayerId>();
    // flecs::entity player_entity {};
    // players.each([&](flecs::entity e, PlayerId& pid) {
    //     if (pid.playerId == 1) {
    //         player_entity = e;
    //     }
    // });

    bool isAppRunning = true;

    constexpr float fixed_dt    = 1.0f / 60.0f;
    const int       MAX_STEPS   = 5;
    float           accumulator = 0.0f;

    Uint64   last = SDL_GetPerformanceCounter();
    uint64_t freq = SDL_GetPerformanceFrequency();
    while (isAppRunning) {
        Uint64 now = SDL_GetPerformanceCounter();
        float  dt  = static_cast<float>(now - last) / freq;
        last       = now;

        dt = std::min(dt, 0.25f);
        accumulator += dt;

        int steps = 0;
        while (accumulator >= fixed_dt && steps < MAX_STEPS) {
            player_physics.run(fixed_dt);
            bullet_physics.run(fixed_dt);
            accumulator -= fixed_dt;
            ++steps;
        }
        float rendering_alpha = accumulator / fixed_dt;

        SDL_Event event {};
        poll_keyboard_state(player_entity);

        if (m_game_client.m_is_connected) {
            m_game_client.parse_incoming_messages();
        }

        while (SDL_PollEvent(&event)) {

            switch (event.type) {

            case SDL_EVENT_QUIT:
                isAppRunning = false;
                break;

            case SDL_EVENT_KEY_DOWN: {
                switch (event.key.key) {

                case SDLK_ESCAPE:
                    isAppRunning = false;
                    break;

                case SDLK_F1: {
                    Uint32 flags = SDL_GetWindowFlags(m_window);
                    if (flags & SDL_WINDOW_FULLSCREEN) {
                        if (!SDL_SetWindowFullscreen(m_window, false)) {
                            get_error();
                        }
                    } else {
                        if (!SDL_SetWindowFullscreen(m_window, true)) {
                            get_error();
                        }
                    }
                    break;
                }

                case SDLK_F2: {
                    if (!m_game_client.m_is_connected) {
                        m_game_client.connect();

                        MsgPlayerJoined msg;
                        msg.id       = player_entity.get<PlayerId>().playerId;
                        msg.position = player_entity.get<Position>();
                        send_data(msg, k_nSteamNetworkingSend_Reliable);
                    } else {
                        disconnect_from_server(player_entity);
                    }
                }

                default: {
                    break;
                }
                }
                break;
            }

            case SDL_EVENT_WINDOW_RESIZED:
                SDL_GetWindowSizeInPixels(m_window, &m_window_w, &m_window_h);
                break;

            case SDL_EVENT_MOUSE_MOTION:
                // std::cout << "MouseX, MouseY: "
                //   << event.motion.x << ", " << event.motion.y << "\n";
                break;
            case SDL_EVENT_MOUSE_BUTTON_DOWN: {
                Position play_pos = player_entity.get<Position>();

                float world_x = camera_entity.get<Camera>().x + event.motion.x;
                float world_y = camera_entity.get<Camera>().y + event.motion.y;

                Direction normalized_dir = normalize_vector(Direction { world_x - play_pos.x, world_y - play_pos.y });
                normalized_dir.x += player_entity.get<Direction>().x * 0.5f;
                normalized_dir.y += player_entity.get<Direction>().y * 0.5f;

                bool is_local = true;
                create_bullet(ecs,
                    dbtf_name,
                    play_pos,
                    normalized_dir,
                    BASE_BULLET_SPEED,
                    Damage { BASE_BULLET_DAMAGE },
                    Range { BASE_BULLET_RANGE },
                    is_local);

            } break;

            default:
                break;
            }
        }

        // Render(rendering_alpha)
        // renderPos = currPos * rendering_alpha + prevPos * (1 - rendering_alpha) ;

        // alpha = 0.0 → exactly at previous physics state
        // alpha = 0.5 → halfway to next physics state
        // alpha = 0.99 → almost at next physics state

        SDL_RenderClear(m_renderer);
        ecs.progress(dt);
        SDL_RenderPresent(m_renderer);

        // RENDER TOP
        // SDL_RenderFillRect(m_renderer, &topRect);
        // SDL_SetRenderDrawColor(m_renderer, 50, 20, 20, 255);

        SDL_GetWindowSizeInPixels(m_window, &m_window_w, &m_window_h);
    }

    SDL_DestroyTexture(player_entity.get_mut<Texture>().texture);

    ecs.query<Texture>().each([](flecs::entity e, Texture& t) {
        if (t.texture) {
            SDL_DestroyTexture(t.texture);
            t.texture = nullptr; // good practice to avoid dangling pointer
        }
    });

    if (m_game_client.m_is_connected) {
        disconnect_from_server(player_entity);
    }

    SDL_DestroyRenderer(m_renderer);
    SDL_DestroyWindow(m_window);

    TTF_CloseFont(m_font);
    TTF_Quit();

    if (SDL_WasInit(SDL_INIT_VIDEO)) {
        SDL_GL_UnloadLibrary();
    }
    SDL_Quit();

    m_game_client.shutdown();
    return 0;
}

bool load_font()
{
    if (!TTF_Init()) {
        get_error();
        return false;
    }
    m_font = TTF_OpenFont("fonts/roboto.ttf", 32);
    if (!m_font) {
        SDL_Log("%s", "Couldnt load the font!");
        return false;
    }
    return true;
}

void create_bullet(flecs::world ecs, const char* texture_file_name, Position pos, Direction dir, float speed, Damage damage, Range range, bool isLocal)
{
    int          tex_w {};
    int          tex_h {};
    SDL_Texture* texture {};
    createTexture(&texture, tex_w, tex_h, texture_file_name);

    if (isLocal)
        ecs.entity()
            .add<BulletTag>()
            .add<LocalBullet>()
            .set<Speed>({ speed })
            .set<Texture>({ texture })
            .set<Position>(pos)
            .set<Direction>(dir)
            .set<Damage>(damage)
            .set<Range>(range)
            .set<RectF>({ pos.x - tex_w / 2.0f,
                pos.y - tex_h / 2.0f,
                static_cast<float>(tex_w),
                static_cast<float>(tex_h) });
    else
        ecs.entity()
            .add<BulletTag>()
            .add<Direction>()
            .set<Speed>({ speed })
            .set<Texture>({ texture })
            .set<Position>(pos)
            .set<Direction>(dir)
            .set<Damage>(damage)
            .set<Range>(range)
            .set<RectF>({ pos.x - tex_w / 2.0f,
                pos.y - tex_h / 2.0f,
                static_cast<float>(tex_w),
                static_cast<float>(tex_h) });
}

flecs::entity create_player(flecs::world ecs, uint32_t id, const char* texture_file_name, Position position, float speed, Health health, bool isLocal)
{

    int          tex_w {};
    int          tex_h {};
    SDL_Texture* texture {};
    createTexture(&texture, tex_w, tex_h, texture_file_name);
    flecs::entity player;

    if (isLocal)
        player = ecs.entity("LocalPlayer")
                     .add<PlayerTag>()
                     .add<LocalPlayer>()
                     .set<PlayerId>({ id })
                     .add<Direction>()
                     .set<Speed>({ speed })
                     .set<Texture>({ texture })
                     .set<Position>(position)
                     .set<Health>({ health })
                     .set<RectF>({ position.x - tex_w / 2.0f,
                         position.y - tex_h / 2.0f,
                         static_cast<float>(tex_w),
                         static_cast<float>(tex_h) });
    else
        player = ecs.entity()
                     .add<PlayerTag>()
                     .set<PlayerId>({ id })
                     .add<Direction>()
                     .set<Speed>({ speed })
                     .set<Texture>({ texture })
                     .set<Position>(position)
                     .set<Health>({ health })
                     .set<RectF>({ position.x - tex_w / 2.0f,
                         position.y - tex_h / 2.0f,
                         static_cast<float>(tex_w),
                         static_cast<float>(tex_h) });

    return player;
}

bool is_in_camera_view(const Camera& cam, const Position objPosition, const float objWidth, const float objHeight)
{
    // SDL_Log("Camera X, Y: %f, %f ", cam.x, cam.y);
    // SDL_Log("Object X, Y: %f, %f ", obj.position.x, obj.position.y);
    return !(
        objPosition.x + objWidth < cam.x - cam.w || // object is left of camera
        objPosition.x - objWidth > cam.x + cam.w || // object is right of camera
        objPosition.y + objHeight < cam.y - cam.w || // object is above camera
        objPosition.y - objHeight > cam.y + cam.h // object is below camera
    );
}

void render_font(const char* message, float rect_x, float rect_y)
{
    SDL_Color    white = { 255, 255, 255, 255 };
    SDL_Texture* tex   = get_font_texture(m_renderer, message, white);

    SDL_FRect dst = { rect_x, rect_y, 0, 0 };
    SDL_GetTextureSize(tex, &dst.w, &dst.h);
    SDL_RenderTexture(m_renderer, tex, nullptr, &dst);

    SDL_DestroyTexture(tex);
}

void createTexture(SDL_Texture** texture, int& width, int& height, const char* textureFileName)
{
    char* player_texture_path {};
    SDL_asprintf(&player_texture_path, "%s../textures/%s", SDL_GetBasePath(), textureFileName);

    int            channels;
    unsigned char* data = stbi_load(player_texture_path, &width, &height, &channels, 4);
    if (!data) {
        std::cout << "Image path: " << player_texture_path << "\n";
        std::cout << "Channels: " << channels << "\n";
        std::cerr << "Failed to load image: " << stbi_failure_reason() << "\n";
        SDL_LogDebug(1, "%s", "STBI couldnt load the image!");
    }

    SDL_Surface* surface = SDL_CreateSurfaceFrom(width, height, SDL_PIXELFORMAT_ABGR8888, data, width * 4);
    if (!surface) {
        get_error();
    }

    *texture = SDL_CreateTextureFromSurface(m_renderer, surface);
    if (!*texture) {
        get_error();
        std::cout << player_texture_path << "\n";
    }
    SDL_free(player_texture_path);

    SDL_DestroySurface(surface);
    stbi_image_free(data);
}

bool isPressedDown {};
bool isPressedRight {};
void poll_keyboard_state(flecs::entity player)
{
    const bool* keyboard_state = SDL_GetKeyboardState(nullptr);
    bool        isVertical {};
    bool        isHorizontal {};
    bool        isPressedLeft {};
    bool        isPressedUp {};

    Direction dir;
    if ((keyboard_state[SDL_SCANCODE_W] || keyboard_state[SDL_SCANCODE_UP]) && !isPressedDown) {
        dir.y       = -1.0f;
        isVertical  = true;
        isPressedUp = true;
    }
    if ((keyboard_state[SDL_SCANCODE_A] || keyboard_state[SDL_SCANCODE_LEFT]) && !isPressedRight) {
        dir.x         = -1.0f;
        isHorizontal  = true;
        isPressedLeft = true;
    }
    isPressedDown = false;
    if ((keyboard_state[SDL_SCANCODE_S] || keyboard_state[SDL_SCANCODE_DOWN]) && !isPressedUp) {
        isVertical    = true;
        dir.y         = 1.0f;
        isPressedDown = true;
    }
    isPressedRight = false;
    if ((keyboard_state[SDL_SCANCODE_D] || keyboard_state[SDL_SCANCODE_RIGHT]) && !isPressedLeft) {
        dir.x          = 1.0f;
        isHorizontal   = true;
        isPressedRight = true;
    }
    if (!isVertical)
        dir.y = 0.0f;
    if (!isHorizontal)
        dir.x = 0.0f;

    dir = normalize_vector(dir);
    player.assign<Direction>({ dir });

    dir.x *= player.get<Speed>().speed;
    dir.y *= player.get<Speed>().speed;

    if (dir.x || dir.y) {
        // send_data(dir, k_nSteamNetworkingSend_Unreliable);
        send_data(player.get<Position>(), k_nSteamNetworkingSend_Unreliable);
    }
}

template <typename T>
void send_data(T data, const int k_n_flag)
{
    if (m_game_client.m_is_connected) {
        MsgHeader header;
        header.type = MsgTraits<T>::type;
        header.size = sizeof(T);

        uint8_t buffer[sizeof(MsgHeader) + sizeof(T)];
        memcpy(buffer, &header, sizeof(header));
        memcpy(buffer + sizeof(header), &data, sizeof(data));

        m_game_client.send_data(
            &buffer,
            sizeof(buffer),
            k_n_flag);
    }
}

void send_direction_and_position_data_to_server(Direction dir, Position pos)
{
    if ((dir.x || dir.y) && m_game_client.m_is_connected) {
        MsgHeader header;
        header.type = MsgType::Direction;
        header.size = sizeof(Direction);

        uint8_t buffer[sizeof(MsgHeader) + sizeof(Direction)];
        memcpy(buffer, &header, sizeof(header));
        memcpy(buffer + sizeof(header), &dir, sizeof(dir));

        m_game_client.send_data(&buffer, sizeof(buffer),
            k_nSteamNetworkingSend_Unreliable);

        MsgHeader pos_header;
        pos_header.type = MsgType::Position;
        pos_header.size = sizeof(Position);

        uint8_t pos_buffer[sizeof(MsgHeader) + sizeof(Position)];
        memcpy(pos_buffer, &pos_header, sizeof(pos_header));
        memcpy(pos_buffer + sizeof(pos_header), &pos, sizeof(pos));

        m_game_client.send_data(&pos_buffer, sizeof(pos_buffer),
            k_nSteamNetworkingSend_Unreliable);
    }
}

template <typename T>
float get_vector_length(T vec)
{
    return std::sqrt((vec.x * vec.x) + (vec.y * vec.y));
}

template <typename T>
T normalize_vector(T vec)
{
    float length = get_vector_length(vec);
    // SDL_LogDebug(1, "LENGTH: %f", length);
    // // SDL_LogDebug(1, "Vec X: %f", vec.x);
    // // SDL_LogDebug(1, "Vec Y: %f", vec.y);
    return (length != 0.0f) ? T { vec.x / length, vec.y / length } : T { 0.0f, 0.0f };
}

void disconnect_from_server(flecs::entity player)
{
    MsgPlayerLeft msg;
    msg.id = player.get<PlayerId>().playerId;
    send_data(msg, k_nSteamNetworkingSend_Reliable);

    m_game_client.disconnect_from_server();

    for (const auto& [count, entity] : m_players_by_id) {
        if (!entity.has<LocalPlayer>()) {
            SDL_DestroyTexture(entity.get<Texture>().texture);
            entity.destruct();
        }
    }
    m_players_by_id.clear();
}

void sdl_init()
{

    SDL_Log("%s", "Program Started...");

    set_app_metadata();

    if (!SDL_Init(SDL_INIT_VIDEO)) {
        get_error();
    }

    if (!SDL_Init(SDL_INIT_EVENTS)) {
        get_error();
    }

    if (!SDL_CreateWindowAndRenderer("Clear Page", WINDOW_WIDTH, WINDOW_HEIGHT,
            SDL_WINDOW_RESIZABLE, //| SDL_WINDOW_FULLSCREEN,
            &m_window, &m_renderer)) {
        get_error();
    }
    SDL_RenderPresent(m_renderer);

    if (!SDL_SetHint("SDL_RENDER_DRIVER", "vulkan")) {
        get_error();
    }

    int num_of_render = SDL_GetNumRenderDrivers();
    for (int i = 0; i < num_of_render; ++i) {
        SDL_Log("driver %d: %s\n", i, SDL_GetRenderDriver(i));
    }

    SDL_Log("\nSDL_VERSION: %d", SDL_GetVersion());
    SDL_Log("CURRENT RENDERER: %s\n\n", SDL_GetRendererName(m_renderer));

    SDL_SetRenderVSync(m_renderer, 0);

    load_font();
}

SDL_Texture* get_font_texture(SDL_Renderer* renderer,
    const char*                             message,
    SDL_Color                               color)
{
    SDL_Surface* surf = TTF_RenderText_Blended(m_font, message, 0, color);
    if (!surf)
        return nullptr;

    SDL_Texture* font_texture;
    font_texture = SDL_CreateTextureFromSurface(renderer, surf);
    SDL_DestroySurface(surf);

    return font_texture;
}

void get_error()
{
    const char* error = SDL_GetError();
    SDL_LogError(0, "%s", error);
}

void set_app_metadata()
{
    if (!SDL_SetAppMetadataProperty(SDL_PROP_APP_METADATA_NAME_STRING,
            "Clear Page")) {
        get_error();
    }
    if (!SDL_SetAppMetadataProperty(SDL_PROP_APP_METADATA_TYPE_STRING,
            "game")) {
        get_error();
    }
    if (!SDL_SetAppMetadataProperty(SDL_PROP_APP_METADATA_VERSION_STRING,
            "0.0.1 alpha")) {
        get_error();
    }
    if (!SDL_SetAppMetadataProperty(SDL_PROP_APP_METADATA_CREATOR_STRING,
            "MSaidG, LLC")) {
        get_error();
    }
    if (!SDL_SetAppMetadataProperty(SDL_PROP_APP_METADATA_COPYRIGHT_STRING,
            "Copyright (c) 2024 MSaidG, LLC")) {
        get_error();
    }
}
