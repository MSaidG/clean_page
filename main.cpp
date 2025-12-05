#include <SDL3/SDL.h>
#include <SDL3/SDL_error.h>
#include <SDL3/SDL_events.h>
#include <SDL3/SDL_hints.h>
#include <SDL3/SDL_init.h>
#include <SDL3/SDL_keyboard.h>
#include <SDL3/SDL_keycode.h>
#include <SDL3/SDL_log.h>
#include <SDL3/SDL_oldnames.h>

#include <SDL3/SDL_pixels.h>
#include <SDL3/SDL_properties.h>
#include <SDL3/SDL_render.h>
#include <SDL3/SDL_version.h>

constexpr int WINDOW_WIDTH { 800 };
constexpr int WINDOW_HEIGTH { 600 };

constexpr int DEFAULT_PLAYER_SIZE { 128 };

void set_app_metadata();
void get_error();
void sdl_init();

SDL_Texture*  m_texture {};
SDL_Window*   m_window {};
SDL_Renderer* m_renderer {};

int main(int argc, char* argv[])
{
    sdl_init();

    const SDL_FRect someRect { .w = 100.0f, .h = 50.0f };
    SDL_Event       event;
    bool            isAppRunning = true;

    m_texture = SDL_CreateTexture(m_renderer, SDL_PIXELFORMAT_RGBA8888,
        SDL_TEXTUREACCESS_TARGET, DEFAULT_PLAYER_SIZE, DEFAULT_PLAYER_SIZE);

    while (isAppRunning) {

        while (SDL_PollEvent(&event)) {

            switch (event.type) {
            case SDL_EVENT_QUIT:
                isAppRunning = false;
                break;
            case SDL_EVENT_KEY_DOWN:
                if (event.key.key == SDLK_ESCAPE) {
                    isAppRunning = false;
                }
                break;
            }
        }

        SDL_SetRenderTarget(m_renderer, m_texture);
        SDL_SetRenderDrawColor(m_renderer, 100, 20, 20, 255);
        SDL_RenderClear(m_renderer);

        SDL_RenderRect(m_renderer, &someRect);
        SDL_SetRenderDrawColor(m_renderer, 20, 100, 20, 255);
        SDL_RenderFillRect(m_renderer, &someRect);

        SDL_SetRenderTarget(m_renderer, nullptr);
        SDL_RenderTexture(m_renderer, m_texture, nullptr, nullptr);
        SDL_RenderPresent(m_renderer);
    }

    SDL_Quit();
    return 0;
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

    if (!SDL_CreateWindowAndRenderer("Clear Page", WINDOW_WIDTH, WINDOW_HEIGTH,
            SDL_WINDOW_RESIZABLE, &m_window, &m_renderer)) {
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

    SDL_Log("VSYNC: %s", SDL_GetStringProperty(prop, SDL_PROP_RENDERER_VSYNC_NUMBER, ""));
    SDL_SetStringProperty(prop, SDL_PROP_RENDERER_VSYNC_NUMBER, "1");
    SDL_Log("VSYNC: %s", SDL_GetStringProperty(prop, SDL_PROP_RENDERER_VSYNC_NUMBER, ""));
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
