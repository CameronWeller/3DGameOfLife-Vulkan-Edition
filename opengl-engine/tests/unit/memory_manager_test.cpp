#include <gtest/gtest.h>
#include "opengl/memory/OpenGLMemoryManager.h"
#include "opengl/OpenGLContext.h"
#include "opengl/OpenGLWindowManager.h"
#include "../../../include/Logger.h"

using namespace OpenGLHIP;
using namespace VulkanHIP;

class MemoryManagerUnitTest : public ::testing::Test {
protected:
    void SetUp() override {
        OpenGLWindowManager::WindowConfig windowConfig;
        windowConfig.width = 800;
        windowConfig.height = 600;
        windowConfig.title = "Memory Test";
        windowConfig.visible = false;
        
        windowManager_.init(windowConfig);
        
        OpenGLContextConfig contextConfig;
        contextConfig.majorVersion = 4;
        contextConfig.minorVersion = 3;
        
        context_.initialize(windowManager_.getWindow(), contextConfig);
        
        memoryManager_.initialize();
    }
    
    void TearDown() override {
        memoryManager_.shutdown();
        context_.cleanup();
        windowManager_.cleanup();
    }
    
    OpenGLWindowManager& windowManager_ = OpenGLWindowManager::getInstance();
    OpenGLContext& context_ = OpenGLContext::getInstance();
    OpenGLMemoryManager& memoryManager_ = OpenGLMemoryManager::getInstance();
};

TEST_F(MemoryManagerUnitTest, BufferCreation) {
    auto buffer = memoryManager_.createBuffer(GL_ARRAY_BUFFER, 1024, nullptr, GL_STATIC_DRAW);
    ASSERT_NE(buffer, nullptr);
    EXPECT_TRUE(buffer->isValid());
    EXPECT_EQ(buffer->getSize(), 1024);
    EXPECT_EQ(buffer->getTarget(), GL_ARRAY_BUFFER);
}

TEST_F(MemoryManagerUnitTest, BufferUpdate) {
    auto buffer = memoryManager_.createBuffer(GL_ARRAY_BUFFER, 1024, nullptr, GL_DYNAMIC_DRAW);
    ASSERT_NE(buffer, nullptr);
    
    std::vector<float> data(256, 1.0f);
    ASSERT_NO_THROW(buffer->update(data.data(), 0, data.size() * sizeof(float)));
}

TEST_F(MemoryManagerUnitTest, BufferMapping) {
    auto buffer = memoryManager_.createBuffer(GL_ARRAY_BUFFER, 1024, nullptr, GL_DYNAMIC_DRAW);
    ASSERT_NE(buffer, nullptr);
    
    void* mapped = buffer->map(GL_WRITE_ONLY);
    ASSERT_NE(mapped, nullptr);
    
    float* floatData = static_cast<float*>(mapped);
    floatData[0] = 42.0f;
    
    buffer->unmap();
    
    EXPECT_TRUE(buffer->isValid());
}

TEST_F(MemoryManagerUnitTest, TextureCreation) {
    auto texture = memoryManager_.createTexture(GL_TEXTURE_2D, 256, 256, 1, 
                                                GL_RGBA8, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
    ASSERT_NE(texture, nullptr);
    EXPECT_TRUE(texture->isValid());
    EXPECT_EQ(texture->getWidth(), 256);
    EXPECT_EQ(texture->getHeight(), 256);
}

TEST_F(MemoryManagerUnitTest, VertexArrayCreation) {
    auto vao = memoryManager_.createVertexArray();
    ASSERT_NE(vao, nullptr);
    EXPECT_TRUE(vao->isValid());
}

TEST_F(MemoryManagerUnitTest, ResourceCounts) {
    size_t initialBuffers = memoryManager_.getBufferCount();
    size_t initialTextures = memoryManager_.getTextureCount();
    size_t initialVAOs = memoryManager_.getVertexArrayCount();
    
    auto buffer = memoryManager_.createBuffer(GL_ARRAY_BUFFER, 512, nullptr, GL_STATIC_DRAW);
    auto texture = memoryManager_.createTexture(GL_TEXTURE_2D, 128, 128, 1);
    auto vao = memoryManager_.createVertexArray();
    
    EXPECT_EQ(memoryManager_.getBufferCount(), initialBuffers + 1);
    EXPECT_EQ(memoryManager_.getTextureCount(), initialTextures + 1);
    EXPECT_EQ(memoryManager_.getVertexArrayCount(), initialVAOs + 1);
    
    // Cleanup should reduce counts
    buffer.reset();
    texture.reset();
    vao.reset();
}
