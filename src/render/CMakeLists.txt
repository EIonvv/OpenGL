add_subdirectory(texture)
add_subdirectory(vertex)

target_sources(
    ${PROJECT_NAME}
    PRIVATE
    setupRenderer.h
)