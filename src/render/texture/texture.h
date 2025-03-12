#ifndef TEXTURE_H
#define TEXTURE_H

#include <glad/glad.h>
#define STB_IMAGE_IMPLEMENTATION
#include <stb_image/stb_image.h>
#include <spdlog/spdlog.h>

// Load texture
static void loadTexture(const char *filepath, GLuint &planeTexture)
{
    glGenTextures(1, &planeTexture);
    glBindTexture(GL_TEXTURE_2D, planeTexture);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    int width, height, nrChannels;
    stbi_set_flip_vertically_on_load(true);
    unsigned char *data = stbi_load(filepath, &width, &height, &nrChannels, 0);
    if (data)
    {
        GLenum format = nrChannels == 3 ? GL_RGB : GL_RGBA;
        glTexImage2D(GL_TEXTURE_2D, 0, format, width, height, 0, format, GL_UNSIGNED_BYTE, data);
        glGenerateMipmap(GL_TEXTURE_2D);
        spdlog::info("Texture: {} loaded successfully: {}x{}", filepath, width, height);
    }
    else
    {
        spdlog::error("Failed to load texture: {}", filepath);
    }
    stbi_image_free(data);
}

#endif /* TEXTURE_H */
