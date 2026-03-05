# Find packages
find_package(glfw3 CONFIG REQUIRED)
find_package(GLEW CONFIG REQUIRED)
find_package(glm CONFIG REQUIRED)

# Variables
set("target_name" "simulation_foundry_render")

# Library
file(GLOB_RECURSE "source_files" CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/*.cpp")
add_library("${target_name}" STATIC "${source_files}")
target_include_directories("${target_name}" PUBLIC "${CMAKE_SOURCE_DIR}/include")
target_link_libraries("${target_name}" PUBLIC "glfw" "GLEW::GLEW" "glm::glm")
