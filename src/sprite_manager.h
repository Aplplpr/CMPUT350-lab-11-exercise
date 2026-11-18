#ifndef SPRITE_MANAGER_H
#define SPRITE_MANAGER_H

#include "basic_transform.h"
#include "texture_atlas.h"
#include "vector_util.h"
#include <SFML/Graphics.hpp>
#include <SFML/OpenGL.hpp>

#define GL_SILENCE_DEPRECATION  // silences macOS OpenGL deprecation warnings

#define glCheck(expr)                                                                              \
    expr;                                                                                          \
    if (int e = glGetError()) {                                                                    \
        std::cerr << "OpenGL error: " << e << " in file " __FILE__ << ", line number " << __LINE__ \
                  << '\n';                                                                         \
        abort();                                                                                   \
    }

using SpriteID = size_t;

class SpriteManager {
public:
    SpriteManager(const sf::Vector2u& atlasSize, uint padding)
        : mVertices(sf::PrimitiveType::Triangles),
          mNumSprites{0},
          mAtlas(atlasSize, padding),
          mShader(std::filesystem::path{"src/main.vert"}, std::filesystem::path{"src/main.frag"}) {
        // TODO
        GLuint program = mShader.getNativeHandle();
        // Get attribute locations.
        mPositionAttribLocation = glGetAttribLocation(program, "position");
        mTexCoordsAttribLocation = glGetAttribLocation(program, "texCoords");
        mSpriteNumberAttribLocation = glGetAttribLocation(program, "spriteNumber");

        // Get uniform locations.
        mTextureUniformLocation = glGetUniformLocation(program, "texture");
        mTransformsUniformLocation = glGetUniformLocation(program, "transforms");
        mNumTransformsUniformLocation = glGetUniformLocation(program, "numTransforms");

        // Create OpenGL buffers and texture.
        glGenBuffers(1, &mVerticesVBO);
        glGenBuffers(1, &mSpriteNumberVBO);
        glGenTextures(1, &mTransformsTexture);
    }

    // Destructor
    ~SpriteManager() {
        glDeleteBuffers(1, &mVerticesVBO);
        glDeleteBuffers(1, &mSpriteNumberVBO);
        glDeleteTextures(1, &mTransformsTexture);
    }

    // TODO
    TextureID registerTexture(const std::string& filepath) {
        return mAtlas.registerTexture(filepath);
    };

    // TODO
    SpriteID createSprite(TextureID textureID) {
        sf::Vector2u size = mAtlas.getSizePixels(textureID);
        sf::FloatRect uv = mAtlas.getBoundsUV(textureID);

        float width = static_cast<float>(size.x);
        float height = static_cast<float>(size.y);

        float u0 = uv.position.x;
        float v0 = uv.position.y;
        float u1 = u0 + uv.size.x;
        float v1 = v0 + uv.size.y;

        // First triangle: bottom-left, bottom-right, top-right
        mVertices.append(sf::Vertex({0, 0}, sf::Color::White, {u0, v0}));
        mVertices.append(sf::Vertex({width, 0}, sf::Color::White, {u1, v0}));
        mVertices.append(sf::Vertex({width, height}, sf::Color::White, {u1, v1}));

        // Second triangle: bottom-left, top-right, top-left
        mVertices.append(sf::Vertex({0, 0}, sf::Color::White, {u0, v0}));
        mVertices.append(sf::Vertex({width, height}, sf::Color::White, {u1, v1}));
        mVertices.append(sf::Vertex({0, height}, sf::Color::White, {u0, v1}));

        // Create a default transform for the new sprite.
        mTransforms.push_back(BasicTransform{});

        SpriteID id = mNumSprites;
        mNumSprites++;

        // Store the sprite ID for each of its 6 vertices.
        for (int i = 0; i < 6; i++) {
            mSpriteNumbers.push_back(static_cast<float>(id));
        }

        return id;
    };

    // TODO
    void setTexture(SpriteID sprite, TextureID newTexture) {
        sf::Vector2u size = mAtlas.getSizePixels(newTexture);
        sf::FloatRect uv = mAtlas.getBoundsUV(newTexture);

        float width = static_cast<float>(size.x);
        float height = static_cast<float>(size.y);

        float u0 = uv.position.x;
        float v0 = uv.position.y;
        float u1 = u0 + uv.size.x;
        float v1 = v0 + uv.size.y;

        size_t start = sprite * 6;

        // First triangle
        mVertices[start + 0].position = {0, 0};
        mVertices[start + 1].position = {width, 0};
        mVertices[start + 2].position = {width, height};

        // Second triangle
        mVertices[start + 3].position = {0, 0};
        mVertices[start + 4].position = {width, height};
        mVertices[start + 5].position = {0, height};

        // Update UV coordinates
        mVertices[start + 0].texCoords = {u0, v0};
        mVertices[start + 1].texCoords = {u1, v0};
        mVertices[start + 2].texCoords = {u1, v1};

        mVertices[start + 3].texCoords = {u0, v0};
        mVertices[start + 4].texCoords = {u1, v1};
        mVertices[start + 5].texCoords = {u0, v1};
    };

    // TODO
    void draw(sf::RenderTarget& target, sf::RenderStates states) const {
        // Copy the original vertices.
        sf::VertexArray vertices = mVertices;

        // Apply each sprite's transform to its 6 vertices.
        for (size_t i = 0; i < mNumSprites; i++) {
            for (size_t j = 0; j < 6; j++) {
                size_t index = i * 6 + j;
                vertices[index].position =
                    mTransforms[i].applyPoint(vertices[index].position);
            }
        }

        // Set the atlas texture and shader.
        states.texture = &mAtlas.getTexture();
        states.shader = &mShader;
        mShader.setUniform("texture", sf::Shader::CurrentTexture);

        // Draw all sprites in one call.
        target.draw(vertices, states);
    };

    // TODO
    void drawGL() {
        if (mNumSprites == 0) return;

        glUseProgram(mShader.getNativeHandle());

        // Upload vertex data.
        glBindBuffer(GL_ARRAY_BUFFER, mVerticesVBO);
        glBufferData(GL_ARRAY_BUFFER,
                    mVertices.getVertexCount() * sizeof(sf::Vertex),
                    &mVertices[0], GL_STREAM_DRAW);

        // Position attribute.
        glVertexAttribPointer(mPositionAttribLocation, 2, GL_FLOAT, GL_FALSE,
                            sizeof(sf::Vertex), (const void*)0);
        glEnableVertexAttribArray(mPositionAttribLocation);

        // Texture coordinates attribute.
        glVertexAttribPointer(mTexCoordsAttribLocation, 2, GL_FLOAT, GL_FALSE,
                            sizeof(sf::Vertex),
                            (const void*)(sizeof(sf::Vector2f) + sizeof(sf::Color)));
        glEnableVertexAttribArray(mTexCoordsAttribLocation);

        // Upload sprite numbers.
        glBindBuffer(GL_ARRAY_BUFFER, mSpriteNumberVBO);
        glBufferData(GL_ARRAY_BUFFER,
                    mSpriteNumbers.size() * sizeof(float),
                    mSpriteNumbers.data(), GL_STREAM_DRAW);

        glVertexAttribPointer(mSpriteNumberAttribLocation, 1, GL_FLOAT, GL_FALSE,
                            sizeof(float), (const void*)0);
        glEnableVertexAttribArray(mSpriteNumberAttribLocation);

        // Upload transforms as a 1D texture.
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_1D, mTransformsTexture);

        glTexParameteri(GL_TEXTURE_1D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_1D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_1D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);

        glTexImage1D(GL_TEXTURE_1D, 0, GL_RGBA32F_ARB,
                    static_cast<GLsizei>(mTransforms.size()), 0,
                    GL_RGBA, GL_FLOAT, mTransforms.data());

        // Bind the texture atlas to texture unit 1.
        glActiveTexture(GL_TEXTURE1);
        glBindTexture(GL_TEXTURE_2D, mAtlas.getTexture().getNativeHandle());

        // Set uniforms.
        glUniform1i(mTransformsUniformLocation, 0);
        glUniform1i(mTextureUniformLocation, 1);
        glUniform1i(mNumTransformsUniformLocation,
                    static_cast<GLint>(mTransforms.size()));

        // Draw all sprite triangles.
        glDrawArrays(GL_TRIANGLES, 0,
                    static_cast<GLsizei>(mVertices.getVertexCount()));

        // Disable vertex attributes after drawing.
        glDisableVertexAttribArray(mPositionAttribLocation);
        glDisableVertexAttribArray(mTexCoordsAttribLocation);
        glDisableVertexAttribArray(mSpriteNumberAttribLocation);

        glBindBuffer(GL_ARRAY_BUFFER, 0);
        glUseProgram(0);
    };

    // TODO
    BasicTransform& getTransform(SpriteID id) {
        return mTransforms[id];
    }

    // TODO
    const TextureAtlas& getAtlas() const {
        return mAtlas;
    };

private:
    sf::VertexArray mVertices;
    std::vector<BasicTransform> mTransforms;
    size_t mNumSprites;
    TextureAtlas mAtlas;
    mutable sf::Shader mShader;

    // Use the below only in Batched rendering (part 2):
    std::vector<float> mSpriteNumbers;

    GLint mTextureUniformLocation;
    GLint mTransformsUniformLocation;
    GLint mNumTransformsUniformLocation;

    GLint mPositionAttribLocation;
    GLint mTexCoordsAttribLocation;
    GLint mSpriteNumberAttribLocation;

    GLuint mVerticesVBO;
    GLuint mSpriteNumberVBO;
    GLuint mTransformsTexture;
};

#endif  // SPRITE_MANAGER_H
