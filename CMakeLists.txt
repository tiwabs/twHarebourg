cmake_minimum_required(VERSION 3.20)
project(twHarebourg)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# --- ImGui ---
set(IMGUI_DIR ${CMAKE_SOURCE_DIR}/third_party/imgui)
add_library(imgui STATIC
    ${IMGUI_DIR}/imgui.cpp
    ${IMGUI_DIR}/imgui_draw.cpp
    ${IMGUI_DIR}/imgui_tables.cpp
    ${IMGUI_DIR}/imgui_widgets.cpp
    ${IMGUI_DIR}/backends/imgui_impl_glfw.cpp
    ${IMGUI_DIR}/backends/imgui_impl_opengl3.cpp
)
target_include_directories(imgui PUBLIC ${IMGUI_DIR} ${IMGUI_DIR}/backends)

# --- GLFW + OpenGL ---
set(BUILD_SHARED_LIBS OFF)  # forcer les libs statiques
# Chercher explicitement la lib statique GLFW (.a) plutôt que le DLL
find_library(GLFW_STATIC_LIB NAMES libglfw3.a glfw3
    PATHS ${CMAKE_PREFIX_PATH}
    PATH_SUFFIXES lib
    NO_DEFAULT_PATH
    REQUIRED)
find_path(GLFW_INCLUDE_DIR NAMES GLFW/glfw3.h
    PATHS ${CMAKE_PREFIX_PATH}
    PATH_SUFFIXES include
    NO_DEFAULT_PATH
    REQUIRED)
find_package(OpenGL REQUIRED)

# --- Executable ---
add_executable(${PROJECT_NAME}
    src/main.cpp
    src/app.cpp
    src/overlay_window.cpp
    src/control_window.cpp
    src/hotkey_manager.cpp
    src/placement_mode.cpp
    src/draw_mode.cpp
    src/grid_edit_mode.cpp
    src/ui_panel.cpp
    src/grid.cpp
    src/logic.cpp
)

target_include_directories(${PROJECT_NAME} PRIVATE
    ${CMAKE_SOURCE_DIR}/third_party
    ${IMGUI_DIR}
    ${IMGUI_DIR}/backends
    ${GLFW_INCLUDE_DIR}
)

target_link_libraries(${PROJECT_NAME} PRIVATE
    imgui
    ${GLFW_STATIC_LIB}
    OpenGL::GL
    gdi32
    comctl32
    dwmapi
)

# Lier statiquement le runtime MinGW pour la distribution
if(MINGW)
    target_link_options(${PROJECT_NAME} PRIVATE
        -static-libgcc
        -static-libstdc++
        -Wl,-Bstatic,--whole-archive
        -lwinpthread
        -Wl,--no-whole-archive,-Bdynamic
    )
endif()
