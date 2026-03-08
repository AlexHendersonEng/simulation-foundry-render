# Project
cmake_minimum_required(VERSION 4.0)
project(simulation-foundry-render)

# Includes
include("${CMAKE_SOURCE_DIR}/cmake/utils.cmake")

# Variables
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Find packages
find_package(glfw3 CONFIG REQUIRED)
find_package(GLEW CONFIG REQUIRED)
find_package(glm CONFIG REQUIRED)
find_package(Vulkan REQUIRED)

# Source
add_subdirectory("${CMAKE_SOURCE_DIR}/src")

# Examples
add_subdirectory("${CMAKE_SOURCE_DIR}/examples")

# Add a custom target to copy 'compile-commands.json' to parent directory
add_custom_target(copy_compile_commands ALL
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
            ${CMAKE_BINARY_DIR}/compile_commands.json
            ${CMAKE_BINARY_DIR}/../compile_commands.json
    COMMENT "Copying compile_commands.json to parent directory"
    VERBATIM
)
