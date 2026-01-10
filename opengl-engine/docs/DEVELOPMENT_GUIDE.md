# OpenGL Engine Development Guide

## Building the OpenGL Engine

### Prerequisites

- **CMake 3.20+**
- **C++17 compiler** (MSVC 2019+, GCC 8+, Clang 10+)
- **vcpkg** (dependency manager)
- **OpenGL 4.3+** compatible GPU and drivers
- **Vulkan SDK** (for GLSL compiler, optional)

### Build Steps

```bash
# Configure build
cmake -B build -S . \
  -DCMAKE_TOOLCHAIN_FILE=vcpkg/scripts/buildsystems/vcpkg.cmake \
  -DCMAKE_BUILD_TYPE=Release \
  -DBUILD_OPENGL_ENGINE=ON \
  -DBUILD_TESTING=ON

# Build
cmake --build build --config Release --parallel

# Run OpenGL executable
./build/Release/3DGameOfLife-Vulkan-Edition-OpenGL.exe  # Windows
./build/3DGameOfLife-Vulkan-Edition-OpenGL  # Linux/macOS
```

### CMake Configuration

The OpenGL engine is conditionally built based on `BUILD_OPENGL_ENGINE`:

```cmake
option(BUILD_OPENGL_ENGINE "Build OpenGL engine version" ON)

if(BUILD_OPENGL_ENGINE)
    add_subdirectory(opengl-engine)
endif()
```

## Project Structure

```
opengl-engine/
├── include/              # Header files
│   └── opengl/
│       ├── OpenGLContext.h
│       ├── OpenGLWindowManager.h
│       ├── compute/
│       ├── rendering/
│       ├── memory/
│       ├── diagnostics/
│       └── ui/
├── src/                  # Implementation files
│   └── opengl/
│       ├── OpenGLContext.cpp
│       ├── OpenGLWindowManager.cpp
│       └── [component implementations]
├── shaders/              # GLSL shaders
│   ├── compute/
│   └── rendering/
├── tests/                # Test suites
│   ├── smoke/
│   ├── unit/
│   ├── integration/
│   ├── e2e/
│   ├── performance/
│   └── visual_regression/
├── diagnostics/          # Diagnostic tools
├── scripts/              # Build and CI scripts
└── docs/                 # Documentation
```

## Adding New Features

### Adding a New Component

1. **Create header file** in `opengl-engine/include/opengl/`
2. **Create implementation file** in `opengl-engine/src/opengl/`
3. **Add to CMakeLists.txt**:
   ```cmake
   list(APPEND OPENGL_ENGINE_SOURCES
       src/opengl/YourComponent.cpp
   )
   ```
4. **Add tests** in appropriate test directory
5. **Update documentation**

### Adding a New Shader

1. **Create shader file** in `opengl-engine/shaders/` (compute or rendering)
2. **Load in code**:
   ```cpp
   renderer->loadShaders("shaders/your_shader.vert", "shaders/your_shader.frag");
   ```
3. **Test shader compilation** in smoke tests
4. **Document shader** in shader directory README

## Debugging

### Enable Debug Context

```cpp
OpenGLContextConfig config;
config.debugContext = true;
config.requestDebugContext = true;
context.initialize(window, config);
```

### View Debug Output

Debug messages are logged to:
- Console (for high/critical severity)
- File: `logs/opengl_debug.log`
- JSON format for automated analysis

### Common Debug Scenarios

1. **Shader compilation errors**: Check shader source, version directives
2. **Binding errors**: Verify SSBO/uniform buffer binding indices match shader
3. **Memory errors**: Check buffer sizes, alignment (std140)
4. **Synchronization errors**: Ensure proper memory barriers and fences

## Testing

### Running Tests

```bash
# All tests
ctest --build-config Release

# Smoke tests only
ctest --build-config Release -L smoke

# Unit tests
ctest --build-config Release -L unit

# Integration tests
ctest --build-config Release -L integration

# E2E tests
ctest --build-config Release -L e2e

# Performance benchmarks
ctest --build-config Release -L performance
```

### Writing Tests

**Smoke Test** (quick validation):
```cpp
TEST_F(MyComponentSmokeTest, BasicFunctionality) {
    auto component = std::make_unique<MyComponent>();
    component->initialize();
    EXPECT_TRUE(component->isInitialized());
}
```

**Unit Test** (component testing):
```cpp
TEST_F(MyComponentUnitTest, FeatureX) {
    auto component = createComponent();
    component->doFeatureX();
    EXPECT_EQ(component->getResult(), expectedResult);
}
```

**Integration Test** (multi-component):
```cpp
TEST_F(IntegrationTest, ComponentAWithComponentB) {
    auto componentA = createComponentA();
    auto componentB = createComponentB();
    componentA->integrateWith(componentB);
    EXPECT_TRUE(componentA->isIntegrated());
}
```

## Performance Profiling

### Using Built-in Profiler

```cpp
auto& profiler = OpenGLProfiler::getInstance();
profiler.initialize();

// In render loop
profiler.beginFrame();
// ... rendering code ...
profiler.endFrame();

// Export results
profiler.exportToJSON("performance_report.json");
profiler.exportToCSV("performance_report.csv");
```

### External Tools

- **RenderDoc**: Capture and analyze frames
- **NVIDIA Nsight Graphics**: Frame debugging and profiling
- **AMD Radeon GPU Profiler**: Performance analysis
- **OpenGL Profiler** (built-in): Real-time metrics

## Code Style Guidelines

### Naming Conventions

- **Classes**: `PascalCase` (e.g., `OpenGLContext`)
- **Functions**: `camelCase` (e.g., `initialize()`)
- **Variables**: `camelCase` (e.g., `bufferSize`)
- **Constants**: `UPPER_SNAKE_CASE` (e.g., `MAX_BUFFER_SIZE`)
- **Private members**: `trailingUnderscore_` (e.g., `id_`)

### RAII Pattern

All OpenGL resources should use RAII wrappers:

```cpp
// Good
auto buffer = memoryManager.createBuffer(...);
// Automatic cleanup on scope exit

// Bad
GLuint buffer;
glGenBuffers(1, &buffer);
// Manual cleanup required
```

### Error Handling

- Use exceptions for fatal errors
- Return `bool` for recoverable errors
- Log all errors via `Logger`
- Use debug callback for OpenGL errors

## Contributing

### Branch Strategy

- `feature/opengl-{component}`: New features
- `fix/opengl-{issue}`: Bug fixes
- `refactor/opengl-{component}`: Refactoring
- `test/opengl-{test-type}`: Test additions

### Commit Messages

Follow Conventional Commits:

```
feat(opengl): add shader hot-reload support
fix(opengl/compute): correct SSBO binding indices
test(opengl): add smoke tests for renderer
docs(opengl): update API documentation
```

### PR Requirements

- All tests must pass
- No linter errors
- Documentation updated
- Performance benchmarks (if applicable)
- Diagnostic reports generated

## Resources

### Documentation

- [OpenGL 4.3 Specification](https://www.khronos.org/registry/OpenGL/specs/gl/glspec43.core.pdf)
- [GLSL 4.30 Specification](https://www.khronos.org/registry/OpenGL/specs/gl/GLSLangSpec.4.30.pdf)
- [GLAD Documentation](https://glad.dav1d.de/)
- [GLFW Documentation](https://www.glfw.org/docs/latest/)

### Tools

- **RenderDoc**: https://renderdoc.org/
- **NVIDIA Nsight**: https://developer.nvidia.com/nsight-graphics
- **AMD Radeon GPU Profiler**: https://gpuopen.com/rgp/

### Community

- OpenGL Forums
- Stack Overflow (opengl tag)
- Khronos Forums
