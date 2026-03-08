# Variables
set("target_name" "opengl_triangle")

# Copy shaders to build directory
add_custom_target(copy_shaders ALL
    COMMAND ${CMAKE_COMMAND} -E copy_directory
            ${CMAKE_CURRENT_SOURCE_DIR}/shaders
            ${CMAKE_CURRENT_BINARY_DIR}/shaders
)

# Executable
add_executable("${target_name}" "opengl_triangle.cpp")
target_link_libraries("${target_name}" PRIVATE "simulation_foundry_render" "glfw" "GLEW::GLEW" "glm::glm")
