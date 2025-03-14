cmake_minimum_required(VERSION 3.10)
project(
    OpenGLRendering
    VERSION 1.0.0
    DESCRIPTION "OpenGL Text Rendering with ImGui"
    LANGUAGES CXX C
)

# Set C++ standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Enable LTO (Link Time Optimization) in Release builds
set(CMAKE_INTERPROCEDURAL_OPTIMIZATION_RELEASE TRUE)

# Default to static libraries, but allow override
option(BUILD_SHARED_LIBS "Build shared libraries" OFF)

# Enable FetchContent
include(FetchContent)

# Find Python3 for resource generation
find_package(Python3 REQUIRED COMPONENTS Interpreter)

# GLFW
find_package(glfw3 QUIET)
if (NOT glfw3_FOUND)
    message(STATUS "GLFW3 not found, fetching from GitHub...")
    FetchContent_Declare(
        glfw
        GIT_REPOSITORY https://github.com/glfw/glfw.git
        GIT_TAG 3.3.8
    )
    set(GLFW_BUILD_DOCS OFF CACHE BOOL "" FORCE)
    set(GLFW_BUILD_TESTS OFF CACHE BOOL "" FORCE)
    set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
    FetchContent_MakeAvailable(glfw)
else()
    message(STATUS "GLFW3 found on system.")
endif()

# GLM
find_package(glm QUIET)
if (NOT glm_FOUND)
    message(STATUS "GLM not found, fetching from GitHub...")
    FetchContent_Declare(
        glm
        GIT_REPOSITORY https://github.com/g-truc/glm.git
        GIT_TAG 0.9.9.8
    )
    FetchContent_MakeAvailable(glm)
else()
    message(STATUS "GLM found on system.")
endif()

# spdlog
find_package(spdlog QUIET)
if (NOT spdlog_FOUND)
    message(STATUS "spdlog not found, fetching from GitHub...")
    FetchContent_Declare(
        spdlog
        GIT_REPOSITORY https://github.com/gabime/spdlog.git
        GIT_TAG v1.8.5
    )
    FetchContent_MakeAvailable(spdlog)
else()
    message(STATUS "spdlog found on system.")
endif()

# FreeType
find_package(Freetype QUIET)
if (NOT Freetype_FOUND)
    message(STATUS "FreeType not found, fetching from GitHub...")
    FetchContent_Declare(
        freetype
        GIT_REPOSITORY https://github.com/freetype/freetype.git
        GIT_TAG VER-2-10-4
    )
    set(FT_DISABLE_ZLIB ON CACHE BOOL "" FORCE)
    set(FT_DISABLE_BZIP2 ON CACHE BOOL "" FORCE)
    set(FT_DISABLE_PNG ON CACHE BOOL "" FORCE)
    set(FT_DISABLE_HARFBUZZ ON CACHE BOOL "" FORCE)
    set(FT_DISABLE_BROTLI ON CACHE BOOL "" FORCE)
    FetchContent_MakeAvailable(freetype)
else()
    message(STATUS "FreeType found on system.")
endif()

# cpp-base64 (header-only library)
FetchContent_Declare(
    cpp-base64
    GIT_REPOSITORY https://github.com/ReneNyffenegger/cpp-base64.git
    GIT_TAG master
)
FetchContent_GetProperties(cpp-base64)
if(NOT cpp-base64_POPULATED)
    FetchContent_Populate(cpp-base64)
    add_library(cpp-base64 INTERFACE)
    target_include_directories(cpp-base64 INTERFACE ${cpp-base64_SOURCE_DIR})
endif()

# ImGui (header-only library with backends)
FetchContent_Declare(
    imgui
    GIT_REPOSITORY https://github.com/ocornut/imgui.git
    GIT_TAG v1.91.4  # Use the latest stable version as of March 2025
)
FetchContent_MakeAvailable(imgui)

# Add ImGui as a library with necessary source files
set(IMGUI_DIR ${imgui_SOURCE_DIR})
add_library(imgui STATIC
    ${IMGUI_DIR}/imgui.cpp
    ${IMGUI_DIR}/imgui_draw.cpp
    ${IMGUI_DIR}/imgui_tables.cpp
    ${IMGUI_DIR}/imgui_widgets.cpp
    ${IMGUI_DIR}/backends/imgui_impl_glfw.cpp
    ${IMGUI_DIR}/backends/imgui_impl_opengl3.cpp
)
target_include_directories(imgui PUBLIC
    ${IMGUI_DIR}
    ${IMGUI_DIR}/backends
)
target_link_libraries(imgui PUBLIC
    glfw
    glad  # For OpenGL functions
)

# Add GLAD source files
set(GLAD_DIR ${CMAKE_SOURCE_DIR}/include/glad)
add_library(glad STATIC
    include/src/glad.c
)
target_include_directories(glad PUBLIC ${GLAD_DIR})

# Ensure the include directory exists
file(MAKE_DIRECTORY ${CMAKE_BINARY_DIR}/include)

# Add your source files
add_executable(${PROJECT_NAME}
    src/main.cpp
)

# Include directories
target_include_directories(${PROJECT_NAME} PRIVATE
    include
    ${CMAKE_BINARY_DIR}/include
    ${GLAD_DIR}
    ${cpp-base64_SOURCE_DIR}
    ${IMGUI_DIR}  # Include ImGui headers
    ${IMGUI_DIR}/backends
)

# Link libraries
target_link_libraries(${PROJECT_NAME} PRIVATE
    glad
    glfw
    glm::glm
    spdlog::spdlog
    freetype
    imgui  # Link ImGui
    ${CMAKE_DL_LIBS}
)

# Specify output directory
set_target_properties(${PROJECT_NAME} PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
)

# Copy the src/resources/ folder to the output directory after building (optional fallback)
if(CMAKE_BUILD_TYPE STREQUAL "Release")
    # delete the old resources folder
    add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E remove_directory
        "${CMAKE_BINARY_DIR}/bin/resources"
    )
    # copy the new resources folder
    add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${CMAKE_SOURCE_DIR}/resources"
        "${CMAKE_BINARY_DIR}/bin/Release/resources"
        # display status message in col
        COMMAND ${CMAKE_COMMAND} -E echo "Copying resources -> ${CMAKE_BINARY_DIR}/bin/Release/resources"
    )
endif()