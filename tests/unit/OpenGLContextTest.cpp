#include <gtest/gtest.h>
#include "opengl/OpenGLContext.h"

TEST(OpenGLContextTest, Initialization) {
    ogl::OpenGLContext ctx;
    ASSERT_TRUE(ctx.initialize(1, 1, "Test"));
} 