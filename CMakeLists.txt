cmake_minimum_required(VERSION 3.24)
project(shadow-mapping-lab VERSION 4.0 LANGUAGES C CXX)
include(FetchContent)
set(FETCHCONTENT_UPDATES_DISCONNECTED ON)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(Vulkan REQUIRED)
find_program(LAB_GLSLC NAMES glslc HINTS "$ENV{VULKAN_SDK}/Bin" "$ENV{VULKAN_SDK}/bin")
if(NOT LAB_GLSLC)
    message(FATAL_ERROR "glslc not found. Install Vulkan SDK and restart the terminal, or set LAB_GLSLC.")
endif()

set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_DOCS OFF CACHE BOOL "" FORCE)
set(GLFW_INSTALL OFF CACHE BOOL "" FORCE)
set(GLFW_LIBRARY_TYPE STATIC)
set(GLM_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(GLM_BUILD_LIBRARY OFF CACHE BOOL "" FORCE)
FetchContent_Declare(glfw GIT_REPOSITORY https://github.com/glfw/glfw.git GIT_TAG 3.5.1 GIT_SHALLOW TRUE)
FetchContent_Declare(vk-bootstrap GIT_REPOSITORY https://github.com/charles-lunarg/vk-bootstrap.git GIT_TAG v1.4.321 GIT_SHALLOW TRUE)
FetchContent_Declare(VulkanMemoryAllocator GIT_REPOSITORY https://github.com/GPUOpen-LibrariesAndSDKs/VulkanMemoryAllocator.git GIT_TAG v3.4.0 GIT_SHALLOW TRUE)
FetchContent_Declare(imgui GIT_REPOSITORY https://github.com/ocornut/imgui.git GIT_TAG v1.92.9 GIT_SHALLOW TRUE)
FetchContent_Declare(glm GIT_REPOSITORY https://github.com/g-truc/glm.git GIT_TAG 1.0.1 GIT_SHALLOW TRUE)
FetchContent_MakeAvailable(glfw vk-bootstrap VulkanMemoryAllocator imgui glm)

add_executable(shadow-mapping-lab source/main.cpp source/graphics_internal.cpp source/graphics.cpp source/texture.cpp source/lighting_scene.cpp source/textured_renderer.cpp source/lab3_application.cpp)
# All new Lab 4 implementation files live in source/lab4.
target_sources(shadow-mapping-lab PRIVATE
    source/lab4/floor.cpp
    source/lab4/shadow_mapping.cpp
    source/lab4/interface.cpp
    source/lab4/vulkan_requirements.cpp)
target_link_libraries(shadow-mapping-lab PRIVATE glfw Vulkan::Vulkan vk-bootstrap::vk-bootstrap GPUOpen::VulkanMemoryAllocator glm::glm)
target_include_directories(shadow-mapping-lab PRIVATE ${imgui_SOURCE_DIR} ${imgui_SOURCE_DIR}/backends third_party)
target_compile_definitions(shadow-mapping-lab PRIVATE GLFW_INCLUDE_NONE GLM_FORCE_DEPTH_ZERO_TO_ONE)
target_sources(shadow-mapping-lab PRIVATE
    ${imgui_SOURCE_DIR}/backends/imgui_impl_glfw.cpp
    ${imgui_SOURCE_DIR}/backends/imgui_impl_vulkan.cpp
    ${imgui_SOURCE_DIR}/imgui.cpp ${imgui_SOURCE_DIR}/imgui_draw.cpp
    ${imgui_SOURCE_DIR}/imgui_tables.cpp ${imgui_SOURCE_DIR}/imgui_widgets.cpp)
if(MSVC)
    target_compile_options(shadow-mapping-lab PRIVATE /W4 /wd4201 /utf-8)
else()
    target_compile_options(shadow-mapping-lab PRIVATE -Wall -Wextra -Wno-missing-field-initializers -Wno-unused-parameter)
endif()
if(MINGW)
    target_link_options(shadow-mapping-lab PRIVATE -static-libgcc -static-libstdc++)
endif()
set_target_properties(shadow-mapping-lab PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
    VS_DEBUGGER_WORKING_DIRECTORY "$<TARGET_FILE_DIR:shadow-mapping-lab>")

foreach(shader textured.vert textured.frag textured_base.frag light_marker.vert marker.frag lab4/shadow.vert)
    get_filename_component(shader_name "${shader}" NAME)
    set(binary "${CMAKE_CURRENT_BINARY_DIR}/shaders/${shader_name}.spv")
    add_custom_command(OUTPUT "${binary}"
        COMMAND ${CMAKE_COMMAND} -E make_directory "${CMAKE_CURRENT_BINARY_DIR}/shaders"
        COMMAND "${LAB_GLSLC}" --target-env=vulkan1.3 "${CMAKE_CURRENT_SOURCE_DIR}/shaders/${shader}" -o "${binary}"
        DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/shaders/${shader}" "${CMAKE_CURRENT_SOURCE_DIR}/shaders/textured_common.glsl" "${CMAKE_CURRENT_SOURCE_DIR}/shaders/lab4/shadow_sampling.glsl" VERBATIM)
    list(APPEND SHADER_BINARIES "${binary}")
endforeach()
add_custom_target(lab-shaders DEPENDS ${SHADER_BINARIES})
add_dependencies(shadow-mapping-lab lab-shaders)
# Run on every build so editing only a shader also updates the runnable directory.
add_custom_target(lab-resources ALL
    COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_CURRENT_SOURCE_DIR}/textures" "$<TARGET_FILE_DIR:shadow-mapping-lab>/textures"
    COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:shadow-mapping-lab>/shaders"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different ${SHADER_BINARIES} "$<TARGET_FILE_DIR:shadow-mapping-lab>/shaders"
    DEPENDS shadow-mapping-lab lab-shaders VERBATIM)
if(MINGW)
    get_filename_component(COMPILER_BIN "${CMAKE_CXX_COMPILER}" DIRECTORY)
    find_file(WINPTHREAD_DLL libwinpthread-1.dll HINTS "${COMPILER_BIN}")
    if(WINPTHREAD_DLL)
        add_custom_command(TARGET lab-resources POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different "${WINPTHREAD_DLL}" "$<TARGET_FILE_DIR:shadow-mapping-lab>"
            VERBATIM)
        install(FILES "${WINPTHREAD_DLL}" DESTINATION .)
    endif()
endif()

install(TARGETS shadow-mapping-lab RUNTIME DESTINATION .)
install(FILES ${SHADER_BINARIES} DESTINATION shaders)
install(DIRECTORY textures DESTINATION .)
install(FILES LICENSE NOTICE DESTINATION .)
