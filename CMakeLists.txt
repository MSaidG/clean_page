cmake_minimum_required(VERSION 3.10)
project(clean_page)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

find_package(SDL3 CONFIG REQUIRED)
# find_package(flecs CONFIG REQUIRED)
find_package(EnTT CONFIG REQUIRED)


add_executable(page main.cpp)

target_link_libraries(page PRIVATE
    SDL3::SDL3
    # $<IF:$<TARGET_EXISTS:flecs::flecs>,flecs::flecs,flecs::flecs_static>
    EnTT::EnTT
    )
