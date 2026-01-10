#include "opengl/OpenGLContext.h"
#include "opengl/OpenGLWindowManager.h"
#include "opengl/Grid3D.h"
#include "opengl/rendering/OpenGLRenderer.h"
#include "opengl/rendering/OpenGLVoxelRenderer.h"
#include "opengl/ui/OpenGLImGui.h"
#include "opengl/memory/OpenGLMemoryManager.h"
#include "../../include/Logger.h"
#include "../../include/Camera.h"
#include "../../include/GameRules.h"
#include <iostream>
#include <stdexcept>
#include <chrono>
#include <thread>
#include <filesystem>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

using namespace OpenGLHIP;
using namespace VulkanHIP;

// Global control flags
bool g_shouldClose = false;
bool g_simulationRunning = false;
float g_simulationSpeed = 1.0f;
float g_voxelSize = 1.0f;
int g_renderMode = 0;  // 0: Solid, 1: Wireframe, 2: Points

int main() {
    try {
        // Initialize logger
        Logger logger;
        logger.log(LogLevel::Info, "Starting OpenGL engine application");

        // Initialize memory manager
        OpenGLMemoryManager::getInstance().initialize();

        // Initialize window manager
        auto& windowManager = OpenGLWindowManager::getInstance();
        OpenGLWindowManager::WindowConfig windowConfig;
        windowConfig.width = 1280;
        windowConfig.height = 720;
        windowConfig.title = "3D Game of Life - OpenGL Edition";
        windowConfig.resizable = true;
        windowConfig.vsync = true;
        
        windowManager.init(windowConfig);
        
        // Set up key callback
        windowManager.setKeyCallback([](int key, int scancode, int action, int mods) {
            if (action != GLFW_PRESS) return;
            
            switch (key) {
                case GLFW_KEY_ESCAPE:
                    g_shouldClose = true;
                    break;
                case GLFW_KEY_SPACE:
                    g_simulationRunning = !g_simulationRunning;
                    break;
                case GLFW_KEY_R:
                    // Reset simulation (will be handled by grid)
                    break;
            }
        });
        
        logger.log(LogLevel::Info, "Controls:\n  ESC - Exit\n  SPACE - Play/Pause\n  R - Reset");
        
        // Initialize OpenGL context
        auto& glContext = OpenGLContext::getInstance();
        OpenGLContextConfig contextConfig;
        contextConfig.majorVersion = 4;
        contextConfig.minorVersion = 3;
        contextConfig.debugContext = true;
        contextConfig.requestDebugContext = true;
        
        glContext.initialize(windowManager.getWindow(), contextConfig);
        
        if (!glContext.supportsComputeShaders()) {
            logger.log(LogLevel::Error, "OpenGL version does not support compute shaders!");
            return 1;
        }
        
        logger.log(LogLevel::Info, "OpenGL context initialized");
        logger.log(LogLevel::Info, "Version: " + glContext.getVersion().versionString);
        logger.log(LogLevel::Info, "Compute shaders: " + std::string(glContext.supportsComputeShaders() ? "Yes" : "No"));
        
        // Initialize renderer
        auto renderer = std::make_unique<OpenGLRenderer>();
        renderer->initialize();
        
        // Load rendering shaders
        std::string vertexShaderPath = "opengl-engine/shaders/rendering/voxel.vert";
        std::string fragmentShaderPath = "opengl-engine/shaders/rendering/voxel.frag";
        
        if (!std::filesystem::exists(vertexShaderPath)) {
            vertexShaderPath = "shaders/rendering/voxel.vert";
            fragmentShaderPath = "shaders/rendering/voxel.frag";
            if (!std::filesystem::exists(vertexShaderPath)) {
                vertexShaderPath = "../opengl-engine/shaders/rendering/voxel.vert";
                fragmentShaderPath = "../opengl-engine/shaders/rendering/voxel.frag";
            }
        }
        
        if (!renderer->loadShaders(vertexShaderPath, fragmentShaderPath)) {
            throw std::runtime_error("Failed to load rendering shaders");
        }
        
        if (!renderer->linkProgram()) {
            throw std::runtime_error("Failed to link rendering program");
        }
        
        // Initialize voxel renderer
        auto voxelRenderer = std::make_unique<OpenGLVoxelRenderer>();
        voxelRenderer->initialize();
        
        // Initialize Grid3D
        auto grid = std::make_unique<Grid3D>(32, 32, 32);
        grid->randomize(0.3f);  // 30% initial density
        
        // Initialize Camera
        auto camera = std::make_unique<VulkanHIP::Camera>(windowManager.getWindow());
        camera->setPosition(glm::vec3(16.0f, 16.0f, 16.0f));
        camera->setTarget(glm::vec3(16.0f, 16.0f, 16.0f));
        
        // Initialize ImGui
        auto& imgui = OpenGLImGui::getInstance();
        imgui.initialize(windowManager.getWindow());
        
        logger.log(LogLevel::Info, "All components initialized successfully");
        logger.log(LogLevel::Info, "Grid initialized: 32x32x32, 30% density");
        
        // Main render loop
        auto lastFrameTime = std::chrono::high_resolution_clock::now();
        auto lastUpdateTime = lastFrameTime;
        int frameCount = 0;
        float simulationTimer = 0.0f;
        
        while (!g_shouldClose && !windowManager.shouldClose()) {
            auto currentTime = std::chrono::high_resolution_clock::now();
            float deltaTime = std::chrono::duration<float>(currentTime - lastFrameTime).count();
            lastFrameTime = currentTime;
            
            // Handle input and window events
            windowManager.pollEvents();
            
            // Update camera
            camera->update(deltaTime);
            
            // Update simulation at fixed intervals (10 FPS by default)
            simulationTimer += deltaTime;
            float updateInterval = 0.1f / g_simulationSpeed;  // 100ms / speed
            if (g_simulationRunning && simulationTimer >= updateInterval) {
                try {
                    grid->update();
                    simulationTimer = 0.0f;
                    lastUpdateTime = currentTime;
                } catch (const std::exception& e) {
                    logger.log(LogLevel::Error, std::string("Simulation update error: ") + e.what());
                }
            }
            
            // Begin frame
            renderer->beginFrame();
            
            // Update viewport
            int width, height;
            windowManager.getFramebufferSize(&width, &height);
            renderer->setViewport(0, 0, width, height);
            
            // Clear screen
            renderer->clear(0.1f, 0.1f, 0.1f, 1.0f);
            
            // Update voxel renderer with grid state
            grid->updateVoxelRenderer(*voxelRenderer);
            
            // Calculate view and projection matrices
            glm::mat4 view = camera->getViewMatrix();
            glm::mat4 proj = camera->getProjectionMatrix();
            
            // Render voxels
            voxelRenderer->render(view, proj, camera->getPosition(), g_voxelSize, 
                                 g_renderMode, static_cast<float>(currentTime.time_since_epoch().count()) / 1e9f);
            
            // Render ImGui
            imgui.beginFrame();
            imgui.renderDebugPanel();
            imgui.renderSettingsPanel();
            imgui.renderPerformancePanel();
            imgui.renderSimulationControls();
            imgui.render();
            
            // Swap buffers
            glContext.swapBuffers();
            
            frameCount++;
            
            // Log every 60 frames
            if (frameCount % 60 == 0) {
                auto& profiler = OpenGLProfiler::getInstance();
                auto lastFrame = profiler.getLastFrameMetrics();
                logger.log(LogLevel::Debug, 
                    "FPS: " + std::to_string(lastFrame.fps) + 
                    ", Generation: " + std::to_string(grid->getGeneration()) +
                    ", Population: " + std::to_string(grid->getPopulation()));
            }
        }
        
        logger.log(LogLevel::Info, "Application shutting down");
        
        // Cleanup
        imgui.shutdown();
        voxelRenderer->cleanup();
        renderer->cleanup();
        grid->cleanup();
        glContext.cleanup();
        windowManager.cleanup();
        OpenGLMemoryManager::getInstance().shutdown();
        
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        VulkanHIP::Logger::getInstance().log(VulkanHIP::Logger::LogLevel::Fatal,
            std::string("Fatal error: ") + e.what());
        return 1;
    }
}

