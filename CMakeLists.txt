cmake_minimum_required(VERSION 3.10)
project(clean_page)

find_package(SDL3 CONFIG REQUIRED)
# find_package(SDL3_image CONFIG REQUIRED)


add_executable(page main.cpp)

target_link_libraries(page PRIVATE
    SDL3::SDL3
    # SDL3_image::SDL3_image
    )
