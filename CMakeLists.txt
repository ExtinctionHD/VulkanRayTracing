cmake_minimum_required(VERSION 3.7.0)

set(TARGET_NAME VulkanRayTracing)
project(${TARGET_NAME})

find_package(Vulkan REQUIRED)

include_directories(
    ${Vulkan_INCLUDE_DIRS}
    external/glfw/include/
    external/glm/
    external/gli/
    external/imgui/
    source/
)

set(SOURCE_DIR ${PROJECT_SOURCE_DIR}/source)

file(GLOB_RECURSE SOURCE_FILES LIST_DIRECTORIES false
    "${SOURCE_DIR}/*.cpp"
    "${SOURCE_DIR}/*.hpp"
)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${PROJECT_SOURCE_DIR}/build/")

add_executable(${TARGET_NAME} ${SOURCE_FILES})

foreach(source IN ITEMS ${SOURCE_FILES})
    get_filename_component(source_path "${source}" PATH)
    file(RELATIVE_PATH source_path_rel "${PROJECT_SOURCE_DIR}" "${source_path}")
    string(REPLACE "/" "\\" group_path "${source_path_rel}")
    source_group("${group_path}" FILES "${source}")
endforeach()

set_property(GLOBAL PROPERTY USE_FOLDERS ON)
set_property(TARGET ${TARGET_NAME} PROPERTY CXX_STANDARD 17)

if(MSVC)
  target_compile_options(${TARGET_NAME} PRIVATE /W4 /WX)
else()
  target_compile_options(${TARGET_NAME} PRIVATE -Wall -Wextra -pedantic -Werror)
endif()

target_compile_definitions(${TARGET_NAME}
    PRIVATE VK_USE_PLATFORM_WIN32_KHR=1
    GLM_FORCE_RADIANS=1
)

target_link_libraries(${TARGET_NAME}
    general ${Vulkan_LIBRARIES}
    debug ${PROJECT_SOURCE_DIR}/external/glfw/src/Debug/glfw3.lib
    optimized ${PROJECT_SOURCE_DIR}/external/glfw/src/Release/glfw3.lib
)
