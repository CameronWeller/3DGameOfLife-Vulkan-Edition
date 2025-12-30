#pragma once

#include "engine/opengl/OpenGLContext.h"
#include <glm/glm.hpp>
#include <vector>
#include <GL/glew.h>

namespace ogl {

class OpenGLRenderer {
public:
    OpenGLRenderer(OpenGLContext& context);
    ~OpenGLRenderer();

    void init();
    void render(const glm::mat4& view, const glm::mat4& projection);

private:
    OpenGLContext& context_;
    GLuint vao_ = 0;
    GLuint vbo_ = 0;
    GLuint program_ = 0;

    void createCube();
    void createShaderProgram();
};

} // namespace ogl 