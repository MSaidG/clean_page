cmake_minimum_required(VERSION 3.10)
project(clean_page)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_BUILD_TYPE Debug)
set(CMAKE_CXX_EXTENSIONS Off)

find_package(SDL3 CONFIG REQUIRED)
find_package(SDL3_ttf CONFIG REQUIRED)
find_package(flecs CONFIG REQUIRED)
find_package(GameNetworkingSockets CONFIG REQUIRED)



# add_compile_options(-fsanitize=address -fno-omit-frame-pointer)
# add_link_options(-fsanitize=address)



add_executable(page main.cpp game_client.cpp network_utils.cpp)

add_executable(server game_server.cpp network_utils.cpp)
# add_executable(client game_client.cpp network_utils.cpp)

if(TARGET flecs::flecs)
    target_link_libraries(
        page PRIVATE
        SDL3::SDL3
        SDL3_ttf::SDL3_ttf
        flecs::flecs
        GameNetworkingSockets::static
    )
else()
    target_link_libraries(
        page PRIVATE
        SDL3::SDL3
        SDL3_ttf::SDL3_ttf
        flecs::flecs_static
        GameNetworkingSockets::static
    )
endif()


target_link_libraries(
    server PRIVATE
    GameNetworkingSockets::static
)

# target_link_libraries(
#     client PRIVATE
#     GameNetworkingSockets::static
# )
