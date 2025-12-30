#include "engine/opengl/OpenGLRenderer.h"
#include <iostream>

namespace ogl {

OpenGLRenderer::OpenGLRenderer(OpenGLContext& context) : context_(context) {}

OpenGLRenderer::~OpenGLRenderer() = default;

void OpenGLRenderer::init() {
    std::cout << "OpenGLRenderer::init() called" << std::endl;
    // TODO: Initialization logic (shaders, buffers, etc.)
}

void OpenGLRenderer::render(const glm::mat4& view, const glm::mat4& projection) {
    (void)view;
    (void)projection;
    // TODO: Rendering logic
}

} // namespace ogl