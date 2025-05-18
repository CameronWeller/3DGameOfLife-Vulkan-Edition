#include <gtest/gtest.h>
#include "../src/VulkanEngine.h"
#include <memory>

class SwapchainTest : public ::testing::Test {
protected:
    void SetUp() override {
        engine = std::make_unique<VulkanEngine>();
        engine->initWindow(800, 600, "Swapchain Test Window");
        engine->initVulkan();
    }

    void TearDown() override {
        engine->cleanup();
    }

    std::unique_ptr<VulkanEngine> engine;
};

// Test swapchain creation
TEST_F(SwapchainTest, SwapchainCreationTest) {
    EXPECT_NO_THROW(engine->createSwapChain());
    EXPECT_NE(engine->getSwapChain(), VK_NULL_HANDLE);
}

// Test swapchain image creation
TEST_F(SwapchainTest, SwapchainImageCreationTest) {
    engine->createSwapChain();
    
    uint32_t imageCount;
    vkGetSwapchainImagesKHR(engine->getDevice(), engine->getSwapChain(), &imageCount, nullptr);
    EXPECT_GT(imageCount, 0);
    
    std::vector<VkImage> swapchainImages(imageCount);
    vkGetSwapchainImagesKHR(engine->getDevice(), engine->getSwapChain(), &imageCount, swapchainImages.data());
    
    for (const auto& image : swapchainImages) {
        EXPECT_NE(image, VK_NULL_HANDLE);
    }
}

// Test swapchain image view creation
TEST_F(SwapchainTest, SwapchainImageViewCreationTest) {
    engine->createSwapChain();
    engine->createImageViews();
    
    const auto& imageViews = engine->getSwapChainImageViews();
    EXPECT_FALSE(imageViews.empty());
    
    for (const auto& imageView : imageViews) {
        EXPECT_NE(imageView, VK_NULL_HANDLE);
    }
}

// Test swapchain recreation
TEST_F(SwapchainTest, SwapchainRecreationTest) {
    engine->createSwapChain();
    VkSwapchainKHR oldSwapchain = engine->getSwapChain();
    
    // Simulate window resize
    engine->setFramebufferResized(true);
    engine->recreateSwapChain();
    
    EXPECT_NE(engine->getSwapChain(), VK_NULL_HANDLE);
    EXPECT_NE(engine->getSwapChain(), oldSwapchain);
}

// Test swapchain format
TEST_F(SwapchainTest, SwapchainFormatTest) {
    engine->createSwapChain();
    
    VkSurfaceFormatKHR surfaceFormat = engine->getSwapChainImageFormat();
    EXPECT_NE(surfaceFormat.format, VK_FORMAT_UNDEFINED);
    EXPECT_NE(surfaceFormat.colorSpace, VK_COLOR_SPACE_MAX_ENUM_KHR);
}

// Test swapchain extent
TEST_F(SwapchainTest, SwapchainExtentTest) {
    engine->createSwapChain();
    
    VkExtent2D extent = engine->getSwapChainExtent();
    EXPECT_GT(extent.width, 0);
    EXPECT_GT(extent.height, 0);
}

// Test swapchain capabilities
TEST_F(SwapchainTest, SwapchainCapabilitiesTest) {
    VkSurfaceCapabilitiesKHR capabilities;
    vkGetPhysicalDeviceSurfaceCapabilitiesKHR(
        engine->getPhysicalDevice(),
        engine->getSurface(),
        &capabilities
    );
    
    EXPECT_GT(capabilities.maxImageCount, 0);
    EXPECT_GT(capabilities.minImageCount, 0);
    EXPECT_GT(capabilities.maxImageExtent.width, 0);
    EXPECT_GT(capabilities.maxImageExtent.height, 0);
}

int main(int argc, char **argv) {
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
} 