# OpenGL Engine API Reference

## Core Classes

### OpenGLContext

Singleton class for OpenGL context management.

```cpp
class OpenGLContext {
public:
    static OpenGLContext& getInstance();
    void initialize(GLFWwindow* window, const OpenGLContextConfig& config = {});
    void cleanup();
    bool isInitialized() const;
    const OpenGLVersion& getVersion() const;
    bool supportsComputeShaders() const;
    bool supportsDebugContext() const;
    void checkError(const std::string& operation) const;
    GLenum getLastError() const;
    bool hasError() const;
    void makeCurrent();
    void swapBuffers();
    void setVSync(bool enabled);
    bool getVSync() const;
};
```

### OpenGLWindowManager

Singleton class for window and input management.

```cpp
class OpenGLWindowManager {
public:
    struct WindowConfig {
        int width = 800;
        int height = 600;
        std::string title = "OpenGL Window";
        bool resizable = true;
        bool fullscreen = false;
        // ... more options
    };
    
    static OpenGLWindowManager& getInstance();
    void init(const WindowConfig& config);
    void cleanup();
    bool shouldClose() const;
    void pollEvents() const;
    bool getKey(int key) const;
    bool getMouseButton(int button) const;
    void getCursorPos(double* xpos, double* ypos) const;
    GLFWwindow* getWindow() const;
    // ... more methods
};
```

### OpenGLMemoryManager

Singleton factory for creating OpenGL resources.

```cpp
class OpenGLMemoryManager {
public:
    static OpenGLMemoryManager& getInstance();
    void initialize();
    void shutdown();
    
    std::unique_ptr<OpenGLBuffer> createBuffer(
        GLenum target, size_t size, const void* data = nullptr,
        GLenum usage = GL_DYNAMIC_DRAW);
    
    std::unique_ptr<OpenGLTexture> createTexture(
        GLenum target, uint32_t width, uint32_t height = 1,
        uint32_t depth = 1, GLenum internalFormat = GL_RGBA8,
        GLenum format = GL_RGBA, GLenum type = GL_UNSIGNED_BYTE,
        const void* data = nullptr);
    
    std::unique_ptr<OpenGLVertexArray> createVertexArray();
    
    // Statistics
    size_t getBufferCount() const;
    size_t getTextureCount() const;
    size_t getVertexArrayCount() const;
    uint64_t getTotalMemoryUsed() const;
};
```

### OpenGLCompute

Compute shader pipeline management.

```cpp
class OpenGLCompute {
public:
    OpenGLCompute();
    ~OpenGLCompute();
    
    // Shader management
    bool loadShader(const std::string& shaderPath);
    bool compileShader(const std::string& shaderSource);
    bool linkProgram();
    
    // Buffer management
    void createStateBuffers(uint32_t width, uint32_t height, uint32_t depth);
    void destroyStateBuffers();
    void updateStateBuffer(const std::vector<uint32_t>& state, bool isCurrent);
    void readStateBuffer(std::vector<uint32_t>& state, bool isCurrent);
    
    // Push constants (uniform buffer)
    void updatePushConstants(const ComputePushConstants& constants);
    void createUniformBuffer();
    void destroyUniformBuffer();
    
    // Dispatch
    void dispatch(uint32_t width, uint32_t height, uint32_t depth);
    void waitForCompletion();
    
    // Getters
    GLuint getProgram() const;
    bool isInitialized() const;
};
```

### Grid3D (OpenGL)

3D Game of Life simulation grid.

```cpp
class Grid3D {
public:
    Grid3D(uint32_t width, uint32_t height, uint32_t depth);
    ~Grid3D();
    
    void initialize();
    void cleanup();
    void update();
    void resize(uint32_t width, uint32_t height, uint32_t depth);
    
    // Cell operations
    void setCell(uint32_t x, uint32_t y, uint32_t z, bool alive);
    bool getCell(uint32_t x, uint32_t y, uint32_t z) const;
    void clear();
    void randomize(float density = 0.5f);
    
    // Rule set management
    void setRuleSet(const GameRules::RuleSet& ruleSet);
    void setRules(const GameRules::RuleSet& rules);
    void setBoundaryType(GameRules::BoundaryType type);
    
    // Pattern management
    bool loadPattern(const std::string& filename);
    bool savePattern(const std::string& filename) const;
    
    // Rendering integration
    void updateVoxelRenderer(OpenGLVoxelRenderer& renderer) const;
    
    // Statistics
    uint32_t getWidth() const;
    uint32_t getHeight() const;
    uint32_t getDepth() const;
    uint64_t getGeneration() const;
    uint64_t getPopulation() const;
};
```

## Diagnostic Classes

### OpenGLDebugCallback

```cpp
class OpenGLDebugCallback {
public:
    static OpenGLDebugCallback& getInstance();
    void initialize();
    void shutdown();
    void setLogFile(const std::string& filename);
    void setSeverityFilter(GLDebugSeverity minSeverity);
    void enableAutoDiagnostics(bool enable);
};
```

### OpenGLProfiler

```cpp
class OpenGLProfiler {
public:
    static OpenGLProfiler& getInstance();
    void initialize();
    void shutdown();
    
    void beginFrame();
    void endFrame();
    void beginCompute();
    void endCompute();
    void beginRender();
    void endRender();
    
    void recordDrawCall(uint32_t vertexCount = 0);
    void recordMemoryUsage(uint64_t bytes);
    
    PerformanceStats getStats() const;
    FrameMetrics getLastFrameMetrics() const;
    
    void exportToJSON(const std::string& filename) const;
    void exportToCSV(const std::string& filename) const;
};
```

### HealthMonitor

```cpp
class HealthMonitor {
public:
    static HealthMonitor& getInstance();
    void initialize();
    void shutdown();
    
    void performHealthCheck();
    HealthStatus getOverallHealth() const;
    std::vector<HealthCheck> getHealthChecks() const;
    
    void recordResourceCreation(const std::string& type);
    void recordResourceDeletion(const std::string& type);
    ResourceCounts getResourceCounts() const;
    
    void exportHealthReport(const std::string& filename) const;
};
```

## Usage Examples

### Basic Initialization

```cpp
// Initialize window
auto& windowManager = OpenGLWindowManager::getInstance();
OpenGLWindowManager::WindowConfig windowConfig;
windowConfig.width = 1280;
windowConfig.height = 720;
windowConfig.title = "My Application";
windowManager.init(windowConfig);

// Initialize OpenGL context
auto& context = OpenGLContext::getInstance();
OpenGLContextConfig contextConfig;
contextConfig.majorVersion = 4;
contextConfig.minorVersion = 3;
contextConfig.debugContext = true;
context.initialize(windowManager.getWindow(), contextConfig);

// Initialize memory manager
OpenGLMemoryManager::getInstance().initialize();
```

### Creating and Using Buffers

```cpp
auto& memoryManager = OpenGLMemoryManager::getInstance();

// Create buffer
auto buffer = memoryManager.createBuffer(
    GL_ARRAY_BUFFER, 
    1024 * sizeof(float), 
    nullptr, 
    GL_DYNAMIC_DRAW
);

// Update buffer
std::vector<float> data(1024, 1.0f);
buffer->update(data.data());

// Map buffer for direct access
void* mapped = buffer->map(GL_WRITE_ONLY);
float* floatData = static_cast<float*>(mapped);
floatData[0] = 42.0f;
buffer->unmap();
```

### Using Compute Shaders

```cpp
auto compute = std::make_unique<OpenGLCompute>();

// Load and compile shader
compute->loadShader("shaders/compute/my_shader.comp");
// ... or compile from source
compute->compileShader(shaderSource);
compute->linkProgram();

// Create state buffers
compute->createStateBuffers(32, 32, 32);
compute->createUniformBuffer();

// Update push constants
ComputePushConstants constants{};
constants.width = 32;
constants.height = 32;
constants.depth = 32;
compute->updatePushConstants(constants);

// Dispatch compute shader
compute->dispatch(32, 32, 32);
compute->waitForCompletion();

// Read results
std::vector<uint32_t> result;
compute->readStateBuffer(result, false);
```

### Rendering

```cpp
auto renderer = std::make_unique<OpenGLRenderer>();
renderer->initialize();
renderer->loadShaders("shaders/vertex.vert", "shaders/fragment.frag");
renderer->linkProgram();

auto voxelRenderer = std::make_unique<OpenGLVoxelRenderer>();
voxelRenderer->initialize();

// Render loop
while (!windowManager.shouldClose()) {
    renderer->beginFrame();
    renderer->clear(0.1f, 0.1f, 0.1f, 1.0f);
    
    // Update uniform buffer
    RenderPushConstants constants{};
    constants.viewProj = viewMatrix * projMatrix;
    constants.cameraPos = cameraPosition;
    renderer->updateUniformBuffer(constants);
    
    // Render voxels
    voxelRenderer->render(viewMatrix, projMatrix, cameraPosition, 1.0f, 0, time);
    
    renderer->endFrame();
    context.swapBuffers();
    windowManager.pollEvents();
}
```
