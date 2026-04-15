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

# --- Shader Configuration ---
set (SLANGC_EXECUTABLE "slangc")

function (add_slang_shader_target TARGET)
  cmake_parse_arguments ("SHADER" "" "" "SOURCES" ${ARGN})
  set (SHADERS_DIR ${CMAKE_CURRENT_LIST_DIR}/shaders)
  set (ENTRY_POINTS -entry vertMain -entry fragMain)

  # Extract just the filenames from full paths for the command
  foreach(SOURCE_FILE ${SHADER_SOURCES})
    get_filename_component(FILENAME ${SOURCE_FILE} NAME)
    list(APPEND SOURCE_FILENAMES ${FILENAME})
  endforeach()

  add_custom_command (
          OUTPUT ${SHADERS_DIR}
          COMMAND ${CMAKE_COMMAND} -E make_directory ${SHADERS_DIR}
  )
  add_custom_command (
          OUTPUT  ${SHADERS_DIR}/slang.spv
          COMMAND ${SLANGC_EXECUTABLE} ${SOURCE_FILENAMES} -target spirv -profile spirv_1_4 -emit-spirv-directly -fvk-use-entrypoint-name ${ENTRY_POINTS} -o slang.spv
          WORKING_DIRECTORY ${SHADERS_DIR}
          DEPENDS ${SHADERS_DIR} ${SHADER_SOURCES}
          COMMENT "Compiling Slang Shaders"
          VERBATIM
  )
  add_custom_target (${TARGET} DEPENDS ${SHADERS_DIR}/slang.spv)
endfunction()

file(GLOB_RECURSE SOURCES CONFIGURE_DEPENDS
    "src/*.cpp"
)

file(GLOB_RECURSE HEADERS CONFIGURE_DEPENDS
    "includes/*.h"
    "includes/*.hpp"
)

set (SHADER_SLANG_SOURCES "${CMAKE_CURRENT_LIST_DIR}/shaders/triangle.slang")

add_slang_shader_target(shaders SOURCES ${SHADER_SLANG_SOURCES})

add_executable(VKLearn
                  ${SOURCES}
                  ${HEADERS})

target_include_directories(VKLearn PRIVATE includes)

target_link_libraries(VKLearn PRIVATE
    Vulkan::Vulkan
    SDL3::SDL3-static
)

add_dependencies(VKLearn shaders)

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
