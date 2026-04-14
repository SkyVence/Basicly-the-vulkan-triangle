cmake_minimum_required(VERSION 4.2)

if(NOT DEFINED CMAKE_CXX_COMPILER)
  set(CMAKE_CXX_COMPILER clang++)
endif()

project(VKLearn VERSION 1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

find_package(Vulkan REQUIRED)

# --- SDL3 Configuration (Static/Built-in) ---
set(SDL_TESTS OFF CACHE BOOL "Disable SDL tests" FORCE)
set(SDL_SHARED OFF CACHE BOOL "Disable SDL shared library" FORCE)
set(SDL_STATIC ON CACHE BOOL "Enable SDL static library" FORCE)
set(SDL_VULKAN ON CACHE BOOL "Enable Vulkan support in SDL" FORCE)

add_subdirectory(vendors/SDL3)

file(GLOB_RECURSE SOURCES CONFIGURE_DEPENDS
    "src/*.cpp"
)

file(GLOB_RECURSE HEADERS CONFIGURE_DEPENDS
    "includes/*.h"
    "includes/*.hpp"
)

add_executable(VKLearn
                  ${SOURCES}
                  ${HEADERS})

target_include_directories(VKLearn PRIVATE includes)

target_link_libraries(VKLearn PRIVATE
    Vulkan::Vulkan
    SDL3::SDL3-static
)

# Ensure console output is visible on Windows
if(WIN32)
    if(MSVC)
        set_target_properties(VKLearn PROPERTIES
            LINK_FLAGS "/SUBSYSTEM:CONSOLE"
        )
    else()
        # MinGW/Clang with GNU linker
        set_target_properties(VKLearn PROPERTIES
            LINK_FLAGS "-Wl,--subsystem,console"
        )
    endif()
endif()

if(CMAKE_EXPORT_COMPILE_COMMANDS)
  execute_process(
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
            ${CMAKE_BINARY_DIR}/compile_commands.json
            ${CMAKE_SOURCE_DIR}/compile_commands.json
    RESULT_VARIABLE copy_result
    ERROR_QUIET
  )
endif()
