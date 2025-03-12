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
        GIT_TAG 3.3.8  # Specify a stable release
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
        GIT_TAG 0.9.9.8  # Specify a stable release
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
        GIT_TAG v1.8.5  # Specify a stable release
    )
    FetchContent_MakeAvailable(spdlog)
else()
    message(STATUS "spdlog found on system.")
endif()

# nlohmann_json
find_package(nlohmann_json QUIET)
if (NOT nlohmann_json_FOUND)
    message(STATUS "nlohmann_json not found, fetching from GitHub...")
    FetchContent_Declare(
        nlohmann_json
        GIT_REPOSITORY https://github.com/nlohmann/json.git
        GIT_TAG v3.9.1  # Specify a stable release
    )
    FetchContent_MakeAvailable(nlohmann_json)
else()
    message(STATUS "nlohmann_json found on system.")
endif()

# Add GLAD source files
set(GLAD_DIR ${CMAKE_SOURCE_DIR}/include/glad)  # Adjust path if different
add_library(glad STATIC
    include/src/glad.c
)

target_include_directories(glad PUBLIC ${GLAD_DIR})

# Add your source files
add_executable(${PROJECT_NAME}
    src/main.cpp
)

# Include directories
target_include_directories(${PROJECT_NAME} PRIVATE
    include  # Your project headers
    ${GLAD_DIR}  # GLAD headers
)

# Link libraries
target_link_libraries(${PROJECT_NAME} PRIVATE
    glad      # Link the GLAD library
    glfw      # Link the GLFW library (correct target name)
    glm::glm  # GLM as a CMake target
    spdlog::spdlog  # spdlog as a CMake target
    nlohmann_json::nlohmann_json  # nlohmann_json as a CMake target
    ${CMAKE_DL_LIBS}  # For dlopen/dlclose on Unix systems (empty on Windows)
)

# Specify output directory
set_target_properties(${PROJECT_NAME} PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
)