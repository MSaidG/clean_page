cmake_minimum_required(VERSION 3.10)
project(clean_page)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_BUILD_TYPE Debug)
set(CMAKE_CXX_EXTENSIONS Off)

find_package(SDL3 CONFIG REQUIRED)
find_package(SDL3_ttf CONFIG REQUIRED)
find_package(flecs CONFIG REQUIRED)



# add_compile_options(-fsanitize=address -fno-omit-frame-pointer)
# add_link_options(-fsanitize=address)



add_executable(page main.cpp)

if(TARGET flecs::flecs)
    target_link_libraries(
        page PRIVATE
        SDL3::SDL3
        SDL3_ttf::SDL3_ttf
        flecs::flecs
    )
else()
    target_link_libraries(
        page PRIVATE
        SDL3::SDL3
        SDL3_ttf::SDL3_ttf
        flecs::flecs_static
    )
endif()
