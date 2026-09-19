cmake_minimum_required(VERSION 3.20)
project("ngine")

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_subdirectory(vendors/glfw)

# TINYXML2
set(tinyxml2_BUILD_TESTING OFF CACHE BOOL "" FORCE)
add_subdirectory(vendors/tinyxml2 EXCLUDE_FROM_ALL)

# ASSIMP
set(ASSIMP_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(ASSIMP_BUILD_SAMPLES OFF CACHE BOOL "" FORCE)
set(ASSIMP_BUILD_TOOLS OFF CACHE BOOL "" FORCE)
set(ASSIMP_INSTALL OFF CACHE BOOL "" FORCE)
set(ASSIMP_BUILD_ASSIMP_VIEW OFF CACHE BOOL "" FORCE)

add_subdirectory(vendors/assimp EXCLUDE_FROM_ALL)


# GLAD
add_library(glad STATIC vendors/glad/src/glad.c)

target_include_directories(glad PUBLIC vendors/glad/include)

# STB
add_library(stb INTERFACE)

target_include_directories(stb INTERFACE vendors/stb)

# IMGUI
add_library(imgui STATIC
    vendors/imgui/imgui.cpp
    vendors/imgui/imgui_draw.cpp
    vendors/imgui/imgui_tables.cpp
    vendors/imgui/imgui_widgets.cpp
    vendors/imgui/backends/imgui_impl_glfw.cpp
    vendors/imgui/backends/imgui_impl_opengl3.cpp
)

target_include_directories(imgui PUBLIC vendors/imgui vendors/imgui/backends)

target_compile_definitions(imgui PRIVATE IMGUI_IMPL_OPENGL_LOADER_GLAD)

# Linking libraries
target_link_libraries(imgui PUBLIC glfw glad)

file(GLOB_RECURSE ENGINE_SOURCES src/*.cpp)

add_executable(${PROJECT_NAME} src/App.cpp main.cpp ${ENGINE_SOURCES} )

target_include_directories(${PROJECT_NAME} PUBLIC includes vendors/glm)

target_link_libraries(${PROJECT_NAME} PRIVATE glfw glad stb imgui assimp tinyxml2::tinyxml2 opengl32)

set(RUNTIME_SHADER_FILES
    ${CMAKE_SOURCE_DIR}/shaders/scene.vert
    ${CMAKE_SOURCE_DIR}/shaders/scene.frag
)

add_custom_target(copy_runtime_assets ALL
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${CMAKE_SOURCE_DIR}/shaders
        $<TARGET_FILE_DIR:${PROJECT_NAME}>/shaders
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${CMAKE_SOURCE_DIR}/resources
        $<TARGET_FILE_DIR:${PROJECT_NAME}>/resources
    DEPENDS ${RUNTIME_SHADER_FILES}
)

add_dependencies(${PROJECT_NAME} copy_runtime_assets)