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
#include <flecs.h>

#include <flecs/addons/cpp/c_types.hpp>
#include <flecs/addons/cpp/entity.hpp>
#include <flecs/addons/cpp/mixins/alerts/impl.hpp>
#include <flecs/addons/cpp/mixins/pipeline/decl.hpp>
#include <flecs/addons/cpp/world.hpp>

#include <cmath>
#include <cstdint>
#include <iostream>
#include <string>

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

#define FLECS_CPP

constexpr int WINDOW_WIDTH { 1360 };
constexpr int WINDOW_HEIGHT { 960 };

constexpr int DEFAULT_PLAYER_SIZE { 128 };

void set_app_metadata();
void get_error();
void sdl_init();
void createTexture(SDL_Texture** texture, int& width, int& height, const char* textureFileName);
void poll_keyboard_state_entity(flecs::entity player);

SDL_Window*   m_window {};
SDL_Renderer* m_renderer {};
TTF_Font*     m_font = nullptr;

int m_current_window_width  = WINDOW_WIDTH;
int m_current_window_height = WINDOW_HEIGHT;

struct Vec2 {
    float x {};
    float y {};
};

struct Position {
    float x {};
    float y {};
};

struct Velocity {
    float x {};
    float y {};
};

struct Direction {
    float x {};
    float y {};
};

struct Speed {
    float speed {};
};

struct Texture {
    SDL_Texture* texture {};

    // ~Texture()
    // {
    //     if (texture)
    //         SDL_DestroyTexture(texture);
    // }
};

struct RectF {
    SDL_FRect rect {};
};

struct PlayerTag { };
struct LocalPlayer { };
struct PlayerId {
    int playerId {};
};

struct Camera {
    float x {};
    float y {};
    float w {};
    float h {};
};

template <typename T>
T normalize_vector(T vec);

template <typename T>
float get_vector_length(T vec);

struct Player {
    SDL_Texture* texture {};
    Direction    move_direction {};
    SDL_FRect    rect { .x {}, .y {}, .w {}, .h {} };
    Position     position {};
    float        speed { 400.0f };
};

void         createPlayer(Player& entity, const char* textureFileName, Position position);
void         poll_keyboard_state(Player& player);
bool         is_in_camera(const SDL_FRect& cam, const Player& obj);
void         create_player_entity(flecs::world ecs, int id, const char* texture_file_name, Position position, float speed, bool isLocal);
bool         is_in_camera_view(const Camera& cam, const Position objPosition, const float objWidth, const float objHeight);
bool         load_font();
SDL_Texture* get_font_texture(SDL_Renderer* renderer, const char* message, SDL_Color color);
void         render_font(const char* message, float rect_x, float rect_y);

int main(int argc, char* argv[])
{
    sdl_init();
    SDL_FRect camera { 0.0f, 0.0f, static_cast<float>(m_current_window_width), static_cast<float>(m_current_window_height) };

    flecs::world ecs;
    auto         camera_entity = ecs.entity("Camera").set<Camera>({ 0.0f, 0.0f, static_cast<float>(m_current_window_width), static_cast<float>(m_current_window_height) });

    // Camera cam = camera_entity.get_mut<Camera>();
    Camera cam { 0.0f, 0.0f, static_cast<float>(m_current_window_width), static_cast<float>(m_current_window_height) };

    ecs.system<Position, LocalPlayer>()
        .kind(flecs::PreUpdate)
        .each([&, camera_entity](Position& player_pos, LocalPlayer) {
            Camera cam = camera_entity.get_mut<Camera>();
            // cam.x = player_pos.x - cam.w * 0.5f;
            // cam.y = player_pos.y - cam.h * 0.5f;
            cam = { player_pos.x - m_current_window_width * 0.5f, player_pos.y - m_current_window_height * 0.5f,
                static_cast<float>(m_current_window_width), static_cast<float>(m_current_window_height) };
            camera_entity.set<Camera>(cam);
            SDL_Log("Cam X, Cam Y: %f, %f", player_pos.x - m_current_window_width / 2.0f, player_pos.y - m_current_window_height / 2.0f);
        });

    ecs.system<Position, Direction, Speed, RectF, Texture>()
        .each([camera_entity](flecs::iter& it, size_t row, Position& p, Direction& d, Speed s, RectF& r, Texture& t) {
            p.x += d.x * s.speed * it.delta_time();
            p.y += d.y * s.speed * it.delta_time();

            // SDL_Log("Px: %f DirX: %f RectW: %f RectX: %f", p.x, d.x, r.rect.w, r.rect.x);
            // SDL_Log("Py: %f DirY: %f RectH: %f RectY: %f", p.y, d.y, r.rect.h, r.rect.y);
            // SDL_Log("CamX, CamY: %f, %f", cam.x, cam.y);

            Camera cam = camera_entity.get_mut<Camera>();

            std::string message_to_render = "Px: " + std::to_string(cam.x) + "Py: " + std::to_string(p.y);
            render_font(message_to_render.c_str(), 100.0f, 100.0f);

            if (is_in_camera_view(cam, p, r.rect.w, r.rect.h)) {
                // Convert world => screen space

                float scaleX = (float)m_current_window_width / cam.w;
                float scaleY = (float)m_current_window_height / cam.h;
                SDL_FRect screenRect {
                    (r.rect.x - cam.x) * scaleX,
                    (r.rect.y - cam.y) * scaleY,
                    r.rect.w * scaleX,
                    r.rect.h * scaleY
                };

                flecs::entity e = it.entity(row);
                if (!e.has<LocalPlayer>()) {
                    SDL_RenderTexture(m_renderer, t.texture, nullptr, &screenRect);
                }
            }

            SDL_RenderTexture(m_renderer, t.texture, nullptr, &r.rect); //&player.rect

            r.rect.x = m_current_window_width / 2.0f - r.rect.w / 2.0f;
            r.rect.y = m_current_window_height / 2.0f - r.rect.h / 2.0f;
        });

    // ecs.system<RectF, Texture>()
    //     .each([](RectF& r, Texture& t) {

    //         SDL_RenderTexture(m_renderer, t.texture, nullptr, &r.rect);
    //         SDL_Log("Py: %f DirY: %f", r.rect.x, r.rect.y);

    //     });

    auto e = ecs.entity()
                 .insert([](Position& p, Direction& d, Speed& s) {
                     p = { 0, 0 };
                     d = { 1, 0 };
                     s = { 1.0f };
                 });

    create_player_entity(ecs, 1, "circle.png", Position { 0.0f, 0.0f }, 1000.0f, true);

    create_player_entity(ecs, 2, "hey.png", Position { 200.0f, 0.0f }, 0.0f, false);

    SDL_FRect topRect { .x = 0.0f, .y = 0.0f, .w = static_cast<float>(m_current_window_width), .h = 100.0f };

    Uint64   last         = SDL_GetPerformanceCounter();
    uint64_t freq         = SDL_GetPerformanceFrequency();
    bool     isAppRunning = true;

    auto          players = ecs.query<PlayerId>();
    flecs::entity player_entity {};
    players.each([&](flecs::entity e, PlayerId& pid) {
        if (pid.playerId == 1) {
            player_entity = e;
        }
    });
    // auto move_direction = player_entity.get_mut<Direction>();

    while (isAppRunning) {

        Uint64 now = SDL_GetPerformanceCounter();
        float  dt  = static_cast<float>(now - last) / SDL_GetPerformanceFrequency();
        last       = now;

        SDL_Event event {};
        poll_keyboard_state_entity(player_entity);

        // SDL_Log("MOVE DIR X: %f", player_entity.get<Direction>().x);
        // SDL_Log("MOVE DIR Y: %f", player_entity.get<Direction>().y);

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

                default: {
                    break;
                }
                }
                break;
            }

            case SDL_EVENT_WINDOW_RESIZED:
                SDL_GetWindowSizeInPixels(m_window, &m_current_window_width, &m_current_window_height);
                topRect = { .x = 0.0f, .y = 0.0f, .w = static_cast<float>(m_current_window_width), .h = 100.0f };
                break;

            default:
                break;
            }
        }

        SDL_RenderClear(m_renderer);
        ecs.progress(dt);

        // RENDER TOP
        SDL_SetRenderDrawColor(m_renderer, 100, 20, 20, 255);
        SDL_RenderFillRect(m_renderer, &topRect);
        SDL_SetRenderDrawColor(m_renderer, 50, 20, 20, 255);

        SDL_RenderPresent(m_renderer);

        SDL_GetWindowSizeInPixels(m_window, &m_current_window_width, &m_current_window_height);
    }

    SDL_DestroyTexture(player_entity.get_mut<Texture>().texture);

    ecs.query<Texture>().each([](flecs::entity e, Texture& t) {
        if (t.texture) {
            SDL_DestroyTexture(t.texture);
            t.texture = nullptr; // good practice to avoid dangling pointer
        }
    });

    SDL_DestroyRenderer(m_renderer);
    SDL_DestroyWindow(m_window);
    TTF_CloseFont(m_font);

    TTF_Quit();

    if (SDL_WasInit(SDL_INIT_VIDEO)) {
        SDL_GL_UnloadLibrary();
    }
    SDL_Quit();
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

void create_player_entity(flecs::world ecs, int id, const char* texture_file_name, Position position, float speed, bool isLocal)
{

    int          texture_width {};
    int          texture_height {};
    SDL_Texture* texture {};
    createTexture(&texture, texture_width, texture_height, texture_file_name);

    if (isLocal)
        auto player_as_entity = ecs.entity()
                                    .add<PlayerTag>()
                                    .set<PlayerId>({ id })
                                    .add<Direction>()
                                    .add<LocalPlayer>()
                                    .set<Speed>({ speed })
                                    .set<Texture>({ texture })
                                    .set<Position>(position)
                                    .set<RectF>({ position.x - texture_width / 2.0f,
                                        position.y - texture_height / 2.0f,
                                        static_cast<float>(texture_width),
                                        static_cast<float>(texture_height) });
    else
        auto player_as_entity = ecs.entity()
                                    .add<PlayerTag>()
                                    .set<PlayerId>({ id })
                                    .add<Direction>()
                                    .set<Speed>({ speed })
                                    .set<Texture>({ texture })
                                    .set<Position>(position)
                                    .set<RectF>({ position.x - texture_width / 2.0f,
                                        position.y - texture_height / 2.0f,
                                        static_cast<float>(texture_width),
                                        static_cast<float>(texture_height) });
}

void createPlayer(Player& entity, const char* textureFileName, Position position)
{
    int texture_width {};
    int texture_height {};
    createTexture(&entity.texture, texture_width, texture_height, textureFileName);
    entity.rect.x   = position.x - texture_width / 2.0f;
    entity.rect.y   = position.y - texture_height / 2.0f;
    entity.rect.h   = static_cast<float>(texture_height);
    entity.rect.w   = static_cast<float>(texture_width);
    entity.position = position;
}

bool is_in_camera(const SDL_FRect& cam, const Player& obj)
{
    return !(
        obj.position.x + obj.rect.w < cam.x || // object is left of camera
        obj.position.x - obj.rect.h > cam.x + cam.w || // object is right of camera
        obj.position.y + obj.rect.h < cam.y || // object is above camera
        obj.position.y - obj.rect.h > cam.y + cam.h // object is below camera
    );
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
    // player.texture = IMG_LoadTexture(m_renderer, player_texture_path);
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
void poll_keyboard_state_entity(flecs::entity player)
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

    // SDL_Log("BEFORE MOVE DIR X: %f", dir.x);
    // SDL_Log("BEFORE MOVE DIR Y: %f", dir.y);
    dir = normalize_vector(dir);
    // SDL_Log("MOVE DIR X: %f", dir.x);
    // SDL_Log("MOVE DIR Y: %f", dir.y);

    player.set<Direction>(dir);
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
