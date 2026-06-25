cmake_minimum_required(VERSION 3.20)

project(ngine)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_subdirectory(vendors/glfw)

add_library(glad
    vendors/glad/src/glad.c
)

target_include_directories(glad
    PUBLIC
        vendors/glad/include
)

file(GLOB_RECURSE ENGINE_SOURCES
    src/*.cpp
)

add_executable(${PROJECT_NAME}
    App.cpp
    ${ENGINE_SOURCES}
)

target_include_directories(${PROJECT_NAME}
    PRIVATE
        includes
        vendors/glm
        vendors/stb       
)

target_link_libraries(${PROJECT_NAME}
    PRIVATE
        glfw
        glad
        opengl32
)

add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${CMAKE_SOURCE_DIR}/shaders
        $<TARGET_FILE_DIR:${PROJECT_NAME}>/shaders
)