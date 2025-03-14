add_subdirectory(imGui)
add_subdirectory(scene)
add_subdirectory(text)
add_subdirectory(texture)
add_subdirectory(vertex)

target_sources(
    ${PROJECT_NAME}
    PRIVATE
    setupRenderer.h
)