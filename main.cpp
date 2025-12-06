#include <SDL3/SDL.h>
#include <SDL3/SDL_blendmode.h>
#include <SDL3/SDL_error.h>
#include <SDL3/SDL_events.h>
#include <SDL3/SDL_filesystem.h>
#include <SDL3/SDL_hints.h>
#include <SDL3/SDL_init.h>
#include <SDL3/SDL_keyboard.h>
#include <SDL3/SDL_keycode.h>
#include <SDL3/SDL_log.h>
#include <SDL3/SDL_oldnames.h>

#include <SDL3/SDL_pixels.h>
#include <SDL3/SDL_properties.h>
#include <SDL3/SDL_render.h>
#include <SDL3/SDL_stdinc.h>
#include <SDL3/SDL_surface.h>
#include <SDL3/SDL_version.h>
#include <SDL3/SDL_video.h>

// #include <SDL3_image/SDL_image.h>


#include <iostream>
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

constexpr int WINDOW_WIDTH { 800 };
constexpr int WINDOW_HEIGHT { 600 };

constexpr int DEFAULT_PLAYER_SIZE { 128 };

void set_app_metadata();
void get_error();
void sdl_init();

SDL_Window*   m_window {};
SDL_Renderer* m_renderer {};

int m_current_window_width  = WINDOW_WIDTH;
int m_current_window_height = WINDOW_HEIGHT;

struct Player {
    SDL_Texture* texture {};
    int texture_width {};
    int texture_height {};
};


int main(int argc, char* argv[])
{
    sdl_init();

    Player player;
    SDL_FRect topRect { .x = 0.0f, .y = 0.0f, .w = static_cast<float>(m_current_window_width), .h = 100.0f };
    SDL_Event event;
    bool      isAppRunning = true;

    char* player_texture_path {};
    SDL_asprintf(&player_texture_path, "%s../textures/hey.png", SDL_GetBasePath());

    
    int channels;
    unsigned char* data = stbi_load(player_texture_path, &player.texture_width, &player.texture_height, &channels, 4);
    if (!data) {
        
        std::cout << "Image path: " << player_texture_path << "\n";
        std::cout << "Channels: " << channels << "\n";
        std::cerr << "Failed to load image: " << stbi_failure_reason() << "\n";
        return -1;
    }
    
    SDL_Surface* surface = SDL_CreateSurfaceFrom(player.texture_width, player.texture_height, SDL_PIXELFORMAT_ABGR8888, data, player.texture_width*4);
    if (!surface) {
        get_error();
    }
    
    
    
    player.texture = SDL_CreateTextureFromSurface(m_renderer, surface);
    // player.texture = IMG_LoadTexture(m_renderer, player_texture_path);
    if (!player.texture) {
        get_error();
        std::cout << player_texture_path << "\n";
    }
    SDL_free(player_texture_path);
    
    SDL_DestroySurface(surface);
    stbi_image_free(data);


    SDL_FRect center { .x = (WINDOW_WIDTH)/2.0f,  .y = (WINDOW_HEIGHT)/2.0f,  .w = static_cast<float>(player.texture_width), .h = static_cast<float>(player.texture_height) };

    if (!SDL_SetTextureBlendMode(player.texture, SDL_BLENDMODE_NONE)) {
        get_error();
    }

 
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
                if (event.key.key == SDLK_F1) {
                    Uint32 flags = SDL_GetWindowFlags(m_window);
                    if (flags & SDL_WINDOW_FULLSCREEN) {
                        if(!SDL_SetWindowFullscreen(m_window, false)) {
                            get_error();
                        }
                    } else {
                        if(!SDL_SetWindowFullscreen(m_window, true)) {
                            get_error();
                        }
                    }
                }

                break;
            case SDL_EVENT_WINDOW_RESIZED:
                SDL_GetWindowSize(m_window, &m_current_window_width, &m_current_window_height);
                topRect = { .x = 0.0f, .y = 0.0f, .w = static_cast<float>(m_current_window_width), .h = 100.0f };
                break;
            default:
                break;
            }
        }

        // SDL_SetRenderDrawColor(m_renderer, 100, 100, 150, 255);
        // SDL_RenderClear(m_renderer);

        // SDL_SetRenderDrawColor(m_renderer, 100, 20, 20, 255);
        // SDL_RenderFillRect(m_renderer, &topRect);


        // SDL_SetRenderDrawColor(m_renderer, 20, 100, 20, 255);
        
        
        
        
        // SDL_SetRenderTarget(m_renderer, player.texture);
        // SDL_SetRenderTarget(m_renderer, nullptr);
        // SDL_RenderTexture(m_renderer, m_texture, nullptr, nullptr);
        
        SDL_SetRenderTarget(m_renderer, nullptr);
        SDL_RenderTexture(m_renderer, player.texture, nullptr, &center); //&center
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
