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
#include <SDL3/SDL_render.h>
#include <SDL3/SDL_scancode.h>
#include <SDL3/SDL_stdinc.h>
#include <SDL3/SDL_surface.h>
#include <SDL3/SDL_timer.h>
#include <SDL3/SDL_version.h>
#include <SDL3/SDL_video.h>

// #include <SDL3_image/SDL_image.h>

#include <cmath>
#include <cstdint>
#include <iostream>
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

constexpr int WINDOW_WIDTH { 800 };
constexpr int WINDOW_HEIGHT { 600 };

constexpr int DEFAULT_PLAYER_SIZE { 128 };

void set_app_metadata();
void get_error();
void sdl_init();
void createTexture(SDL_Texture** texture, int& width, int& height, const char* textureFileName);

SDL_Window*   m_window {};
SDL_Renderer* m_renderer {};

int m_current_window_width  = WINDOW_WIDTH;
int m_current_window_height = WINDOW_HEIGHT;

struct Vec2 {
    float x {};
    float y {};
};

struct Camera {
    Vec2 position {};
    Vec2 size {};
};

Vec2  normalize_vector(Vec2 vec);
float get_vector_length(Vec2 vec);

struct Entity {
    SDL_Texture* texture {};
    Vec2         move_direction {};
    SDL_FRect    rect { .x {}, .y {}, .w {}, .h {} };
    Vec2         position {};
    float        speed { 400.0f };
};

void createEntity(Entity& entity, const char* textureFileName, Vec2 position);
void poll_keyboard_state(Entity& player);
bool is_in_camera(const SDL_FRect& cam, const Entity& obj);

int main(int argc, char* argv[])
{
    // SDL_FRect playerDst { .x = m_current_window_width / 2.0f, .y = m_current_window_height / 2.0f, .w = player.rect.w, .h = player.rect.h };

    sdl_init();

    SDL_FRect camera { 0.0f, 0.0f, static_cast<float>(m_current_window_width), static_cast<float>(m_current_window_height) };
    SDL_FRect topRect { .x = 0.0f, .y = 0.0f, .w = static_cast<float>(m_current_window_width), .h = 100.0f };

    Entity player;
    createEntity(player, "hey.png", Vec2 { 0.0f, 0.0f });

    Entity object;
    createEntity(object, "hey.png", Vec2 { 500.0f, -100.0f });

    Uint64   last         = SDL_GetPerformanceCounter();
    uint64_t freq         = SDL_GetPerformanceFrequency();
    bool     isAppRunning = true;
    while (isAppRunning) {

        Uint64 now = SDL_GetPerformanceCounter();
        float  dt  = static_cast<float>(now - last) / SDL_GetPerformanceFrequency();
        last       = now;

        SDL_Event event {};
        poll_keyboard_state(player);

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
                // SDL_GetRenderOutputSize(m_renderer, &m_current_window_width, &m_current_window_height);
                // SDL_GetWindowSize(m_window, &m_current_window_width, &m_current_window_height);
                topRect = { .x = 0.0f, .y = 0.0f, .w = static_cast<float>(m_current_window_width), .h = 100.0f };
                break;

            default:
                break;
            }
        }

        // MOVE PLAYER
        player.position.x += player.move_direction.x * player.speed * dt;
        player.position.y += player.move_direction.y * player.speed * dt;
        
        SDL_RenderClear(m_renderer);
        if (is_in_camera(camera, object)) {
            // Convert world → screen coordinates
            SDL_FRect screenRect {
                object.rect.x - camera.x,
                object.rect.y - camera.y,
                object.rect.w,
                object.rect.h
            };
            
            SDL_Log("%s", "RENDER OBJECT!");
            SDL_RenderTexture(m_renderer, object.texture, nullptr, &screenRect);
        }

        // RENDER TOP
        SDL_SetRenderDrawColor(m_renderer, 100, 20, 20, 255);
        SDL_RenderFillRect(m_renderer, &topRect);
        SDL_SetRenderDrawColor(m_renderer, 50, 20, 20, 255);

        // RENDER PLAYER
        SDL_RenderTexture(m_renderer, player.texture, nullptr, &player.rect); //&player.rect
        SDL_RenderPresent(m_renderer);

        SDL_GetWindowSizeInPixels(m_window, &m_current_window_width, &m_current_window_height);
        player.rect.x = m_current_window_width / 2.0f - player.rect.w / 2.0f;
        player.rect.y = m_current_window_height / 2.0f - player.rect.h / 2.0f;

        camera = { player.position.x - m_current_window_width / 2.0f, player.position.y - m_current_window_height / 2.0f,
            static_cast<float>(m_current_window_width), static_cast<float>(m_current_window_height) };

        SDL_Log("Player X, Y: %f, %f", player.position.x, player.position.y);
        // SDL_Log("DST X: %f, DST Y: %f", player.rect.x, player.rect.y);
    }


    SDL_DestroyTexture(player.texture);
    SDL_DestroyTexture(object.texture);
    SDL_DestroyWindow(m_window);
    SDL_DestroyRenderer(m_renderer);
    SDL_Quit();
    return 0;
}

bool is_in_camera(const SDL_FRect& cam, const Entity& obj)
{
    SDL_Log("Camera X, Y: %f, %f ", cam.x, cam.y);
    SDL_Log("Object X, Y: %f, %f ", obj.position.x, obj.position.y);

    return !(
        obj.position.x + obj.rect.w  < cam.x || // object is left of camera
        obj.position.x - obj.rect.h > cam.x + cam.w || // object is right of camera
        obj.position.y + obj.rect.h  < cam.y || // object is above camera
        obj.position.y - obj.rect.h > cam.y + cam.h // object is below camera
    );
}

void createEntity(Entity& entity, const char* textureFileName, Vec2 position)
{
    int texture_width {};
    int texture_height {};
    createTexture(&entity.texture, texture_width, texture_height, textureFileName);
    entity.rect.x = position.x - texture_width / 2.0f;
    entity.rect.y = position.y - texture_height / 2.0f;
    entity.rect.h   = static_cast<float>(texture_height);
    entity.rect.w   = static_cast<float>(texture_width);
    entity.position = position;
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
void poll_keyboard_state(Entity& player)
{
    const bool* keyboard_state = SDL_GetKeyboardState(nullptr);
    bool        isVertical {};
    bool        isHorizontal {};
    bool        isPressedLeft {};
    bool        isPressedUp {};
    if ((keyboard_state[SDL_SCANCODE_W] || keyboard_state[SDL_SCANCODE_UP]) && !isPressedDown) {
        player.move_direction.y = -1.0f;
        isVertical              = true;
        isPressedUp             = true;
    }
    if ((keyboard_state[SDL_SCANCODE_A] || keyboard_state[SDL_SCANCODE_LEFT]) && !isPressedRight) {
        player.move_direction.x = -1.0f;
        isHorizontal            = true;
        isPressedLeft           = true;
    }
    isPressedDown = false;
    if ((keyboard_state[SDL_SCANCODE_S] || keyboard_state[SDL_SCANCODE_DOWN]) && !isPressedUp) {
        isVertical              = true;
        player.move_direction.y = 1.0f;
        isPressedDown           = true;
    }
    isPressedRight = false;
    if ((keyboard_state[SDL_SCANCODE_D] || keyboard_state[SDL_SCANCODE_RIGHT]) && !isPressedLeft) {
        player.move_direction.x = 1.0f;
        isHorizontal            = true;
        isPressedRight          = true;
    }
    if (!isVertical)
        player.move_direction.y = 0.0f;
    if (!isHorizontal)
        player.move_direction.x = 0.0f;

    // SDL_Log("BEFORE MOVE DIR X: %f", player.move_direction.x);
    // SDL_Log("BEFORE MOVE DIR Y: %f", player.move_direction.y);
    player.move_direction = normalize_vector(player.move_direction);
    // SDL_LogDebug(1,"MOVE DIR X: %f", player.move_direction.x);
}

float get_vector_length(Vec2 vec)
{
    return std::sqrt((vec.x * vec.x) + (vec.y * vec.y));
}

Vec2 normalize_vector(Vec2 vec)
{
    float length = get_vector_length(vec);
    // SDL_LogDebug(1, "LENGTH: %f", length);
    // // SDL_LogDebug(1, "Vec X: %f", vec.x);
    // // SDL_LogDebug(1, "Vec Y: %f", vec.y);
    return (length != 0.0f) ? Vec2 { vec.x / length, vec.y / length } : Vec2 { 0.0f, 0.0f };
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
            SDL_WINDOW_RESIZABLE | SDL_WINDOW_FULLSCREEN, &m_window, &m_renderer)) {
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
    SDL_Log("CURRENT RENDERER: %s", SDL_GetRendererName(m_renderer));

    SDL_PropertiesID prop { SDL_GetRendererProperties(m_renderer) };
    if (!prop) {
        get_error();
    } else {
        SDL_Log("RENDERER PROPERTIES: %d", prop);
    }


    SDL_SetRenderVSync(m_renderer, 0);
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
    if (!SDL_SetAppMetadataProperty(SDL_PROP_APP_METADATA_TYPE_STRING, "game")) {
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
