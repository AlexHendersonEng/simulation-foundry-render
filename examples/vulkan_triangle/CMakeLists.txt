# Find glslc
find_program(GLSLC_EXECUTABLE glslc REQUIRED)

# Variables
set("target_name" "vulkan_triangle")
set("shader_sources" 
    "${CMAKE_CURRENT_SOURCE_DIR}/shaders/vertex.vert"
    "${CMAKE_CURRENT_SOURCE_DIR}/shaders/fragment.frag"
)
set("shader_binaries" "")

# Executable
add_executable("${target_name}" "vulkan_triangle.cpp")
target_include_directories("${target_name}" PRIVATE "${Vulkan_INCLUDE_DIR}")
target_link_libraries("${target_name}" PRIVATE "Vulkan::Vulkan" "glfw")
set_target_properties("${target_name}" PROPERTIES CXX_STANDARD 20)
set(ENABLE_CPP20_MODULE ON)

# Compile shaders
file(MAKE_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/shaders")

foreach(shader ${shader_sources})
    cmake_path(GET shader STEM shader_name)
    set(shader_output "${CMAKE_CURRENT_BINARY_DIR}/shaders/${shader_name}.spv")

    add_custom_command(
        OUTPUT  "${shader_output}"
        COMMAND "${GLSLC_EXECUTABLE}" "${shader}" -o "${shader_output}"
        DEPENDS "${shader}"
        COMMENT "Compiling shader: ${shader_name}"
    )

    list(APPEND shader_binaries "${shader_output}")
endforeach()

add_custom_target(shaders ALL DEPENDS "${shader_binaries}")
add_dependencies("${target_name}" shaders)
