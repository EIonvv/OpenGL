cmake_minimum_required(VERSION 3.10)
project(3D)

# Set C++ standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Enable FetchContent
include(FetchContent)

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

# Add GLAD source files
set(GLAD_DIR ${CMAKE_SOURCE_DIR}/include/glad)
add_library(glad STATIC
    include/src/glad.c
)

# Include directories for GLAD
target_include_directories(glad PUBLIC ${GLAD_DIR})

# Add your source files
add_executable(${PROJECT_NAME}
    src/main.cpp
    src/render/text/text_renderer.cpp
)

# Include directories
target_include_directories(${PROJECT_NAME} PRIVATE
    include
    ${GLAD_DIR}
)

# Link libraries
target_link_libraries(${PROJECT_NAME} PRIVATE
    glad
    glfw
    glm::glm
    spdlog::spdlog
    freetype
    ${CMAKE_DL_LIBS}
)

# Specify output directory
set_target_properties(${PROJECT_NAME} PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
)