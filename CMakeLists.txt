cmake_minimum_required(VERSION 3.15)
project(raylib_web_manual C)

set(CMAKE_C_STANDARD 99)
set(BUILD_SHARED_LIBS OFF)

# ──────────────────────────────────────────────────────────────────────────────
# Configure these from the command line:
#   -DRAYLIB_SRC=/abs/path/to/raylib/src
#   -DRAYLIB_LIB=/abs/path/to/raylib/build_web/raylib/libraylib.a
# ──────────────────────────────────────────────────────────────────────────────
set(RAYLIB_SRC "" CACHE PATH "Path to raylib/src")
set(RAYLIB_LIB "" CACHE FILEPATH "Path to built libraylib.a")

if(NOT RAYLIB_SRC OR NOT EXISTS "${RAYLIB_SRC}/raylib.h")
  message(FATAL_ERROR "RAYLIB_SRC must point to raylib/src (where raylib.h lives).")
endif()
if(NOT RAYLIB_LIB OR NOT EXISTS "${RAYLIB_LIB}")
  message(FATAL_ERROR "RAYLIB_LIB must point to the built static lib (libraylib.a).")
endif()

# Detect if we're using the Emscripten toolchain (i.e., targeting Web)
if(CMAKE_SYSTEM_NAME STREQUAL "Emscripten")
  set(TARGET_WEB TRUE)
else()
  set(TARGET_WEB FALSE)
endif()

# Your sources
add_executable(game
  main.c
  # add more .c files here
)

# Point directly to raylib headers and static library
target_include_directories(game PRIVATE "${RAYLIB_SRC}")
target_link_libraries(game PRIVATE "${RAYLIB_LIB}")

if(TARGET_WEB)
  # Web build: emit index.html and apply Emscripten flags
  target_compile_definitions(game PRIVATE PLATFORM_WEB)
  set_target_properties(game PROPERTIES OUTPUT_NAME "index" SUFFIX ".html")

  # Core flags that work well with raylib on the web
  target_link_options(game PRIVATE
    "SHELL:-sASYNCIFY"
    "SHELL:-sALLOW_MEMORY_GROWTH=1"
    "SHELL:-sMIN_WEBGL_VERSION=2"
    "SHELL:-sMAX_WEBGL_VERSION=2"
    "SHELL:-sWASM=1"
    "SHELL:-sUSE_GLFW=0"
  )

  # If you have an assets/ folder, preload it automatically
  if(EXISTS "${CMAKE_SOURCE_DIR}/assets")
    message(STATUS "Preloading assets/ into virtual FS")
    target_link_options(game PRIVATE "SHELL:--preload-file assets@/assets")
  endif()

else()
  # Optional: desktop Linux linking (ignored on macOS/Windows)
  if(UNIX AND NOT APPLE)
    find_package(Threads REQUIRED)
    target_link_libraries(game PRIVATE m dl pthread rt X11)
  endif()
endif()


# Tell the app it's a web build and prevent GLFW usage
if(CMAKE_SYSTEM_NAME STREQUAL "Emscripten")
  target_compile_definitions(game PRIVATE PLATFORM_WEB)
  target_link_options(game PRIVATE
    "SHELL:-sUSE_GLFW=0"
    "SHELL:-sMIN_WEBGL_VERSION=2"
    "SHELL:-sMAX_WEBGL_VERSION=2"
    "SHELL:-sALLOW_MEMORY_GROWTH=1"
    "SHELL:-sASYNCIFY"
    "SHELL:-sWASM=1"
  )
endif()
