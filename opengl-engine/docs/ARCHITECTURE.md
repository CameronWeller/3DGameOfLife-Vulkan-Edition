# OpenGL Engine Architecture

## Overview

The OpenGL Engine is a complete implementation of the 3D Game of Life simulation using OpenGL 4.3+ compute shaders and modern OpenGL rendering. It provides a simpler, more development-friendly alternative to the Vulkan implementation while maintaining feature parity.

## Architecture Components

### Core Components

#### OpenGLContext (`opengl/OpenGLContext.h`)
- **Purpose**: Manages OpenGL context lifecycle, version detection, and debug context setup
- **Features**:
  - GLAD loader integration
  - OpenGL version detection with fallback
  - Debug context setup with callbacks
  - Extension checking
  - VSync control
  - Error checking utilities
- **Dependencies**: GLAD, GLFW, Logger

#### OpenGLWindowManager (`opengl/OpenGLWindowManager.h`)
- **Purpose**: Window management and input handling (Vulkan-free)
- **Features**:
  - GLFW window creation with OpenGL hints
  - Input handling (keyboard, mouse, scroll)
  - Thread-safe window callbacks
  - Window state queries
- **Dependencies**: GLFW, Logger

#### OpenGLMemoryManager (`opengl/memory/OpenGLMemoryManager.h`)
- **Purpose**: RAII resource management for OpenGL objects
- **Components**:
  - `OpenGLBuffer`: RAII buffer wrapper with automatic cleanup
  - `OpenGLTexture`: RAII texture wrapper (1D/2D/3D support)
  - `OpenGLVertexArray`: RAII VAO wrapper
  - Factory methods with automatic resource tracking
- **Features**:
  - Move-only semantics
  - Automatic cleanup on destruction
  - Health monitor integration
  - Resource statistics
- **Dependencies**: OpenGL, HealthMonitor

#### OpenGLCompute (`opengl/compute/OpenGLCompute.h`)
- **Purpose**: Compute shader pipeline management
- **Features**:
  - Shader compilation from source/file
  - SSBO (Shader Storage Buffer Object) management
  - Uniform buffer management (for push constants)
  - Compute dispatch with synchronization
  - Fence-based synchronization
- **Dependencies**: OpenGLMemoryManager, OpenGLProfiler

#### OpenGLRenderer (`opengl/rendering/OpenGLRenderer.h`)
- **Purpose**: Basic rendering pipeline
- **Features**:
  - Shader program management (vertex/fragment)
  - Render state setup (depth test, face culling, blending)
  - Viewport management
  - Clear operations
  - Uniform buffer for render constants
  - Render mode switching (Solid/Wireframe/Points)
- **Dependencies**: OpenGLMemoryManager, OpenGLProfiler

#### OpenGLVoxelRenderer (`opengl/rendering/OpenGLVoxelRenderer.h`)
- **Purpose**: Voxel instance rendering
- **Features**:
  - Cube geometry generation
  - Instance rendering with VBO
  - Frustum culling
  - LOD system
  - Dynamic instance management
- **Dependencies**: OpenGLRenderer, OpenGLMemoryManager, Camera

#### Grid3D (OpenGL version) (`opengl/Grid3D.h`)
- **Purpose**: 3D Game of Life simulation grid
- **Features**:
  - SSBO-based state management
  - Compute shader integration
  - Pattern loading/saving
  - Rule set management
  - Boundary condition handling (Toroidal/Fixed)
  - Statistics tracking (generation, population)
- **Dependencies**: OpenGLCompute, GameRules, PatternManager

#### OpenGLImGui (`opengl/ui/OpenGLImGui.h`)
- **Purpose**: ImGui integration with OpenGL backend
- **Features**:
  - OpenGL3 backend setup
  - Debug panel
  - Settings panel
  - Performance panel
  - Simulation controls
- **Dependencies**: ImGui (OpenGL3 binding), OpenGLProfiler, HealthMonitor

### Diagnostic Framework

#### OpenGLDebugCallback (`opengl/diagnostics/OpenGLDebugCallback.h`)
- **Purpose**: OpenGL debug context integration
- **Features**:
  - Automatic error detection and logging
  - Severity levels (Low, Medium, High, Critical)
  - Source/timestamp tracking
  - Suggestion system for common issues
  - Message history (up to 1000 messages)
  - Thread-safe file logging

#### OpenGLProfiler (`opengl/diagnostics/OpenGLProfiler.h`)
- **Purpose**: Performance profiling
- **Features**:
  - Frame time tracking (min/max/avg)
  - Compute shader execution time
  - Render time tracking
  - Draw call and vertex count
  - GPU memory usage
  - FPS calculation
  - JSON/CSV export
  - Frame history (up to 1000 frames)

#### HealthMonitor (`opengl/diagnostics/HealthMonitor.h`)
- **Purpose**: System health monitoring
- **Features**:
  - OpenGL context validity checks
  - Memory usage monitoring
  - Frame rate degradation detection
  - Resource leak detection
  - Shader compilation error tracking
  - Automatic issue detection
  - Health report generation (JSON and text)
  - Resource tracking (buffers, textures, shaders, programs, VAOs)

## Data Flow

### Simulation Loop

```
main() → Grid3D::update()
    → OpenGLCompute::dispatch()
        → GPU Compute Shader Execution
        → OpenGLCompute::waitForCompletion()
    → Grid3D::syncStateFromGPU()
    → Grid3D::updateVoxelRenderer()
        → OpenGLVoxelRenderer::updateInstances()
```

### Rendering Loop

```
main() → OpenGLRenderer::beginFrame()
    → OpenGLProfiler::beginFrame()
    → OpenGLRenderer::clear()
    → OpenGLVoxelRenderer::render()
        → Bind shader program
        → Update uniform buffer
        → Bind VAO and instance buffer
        → glDrawElementsInstanced()
        → OpenGLProfiler::recordDrawCall()
    → OpenGLImGui::render()
        → Debug panels
        → Settings panels
        → Performance metrics
    → OpenGLContext::swapBuffers()
    → OpenGLRenderer::endFrame()
        → OpenGLProfiler::endFrame()
```

## Memory Management

### SSBO Management

- **Current State Buffer**: SSBO binding 1, read-only in compute shader
- **Next State Buffer**: SSBO binding 2, write-only in compute shader
- **Uniform Buffer**: Binding 0, for push constants (std140 layout)

### Buffer Lifecycle

1. **Creation**: `OpenGLMemoryManager::createBuffer()` → `glGenBuffers()` → `glBufferData()`
2. **Update**: `OpenGLBuffer::update()` → `glBufferSubData()`
3. **Mapping** (optional): `OpenGLBuffer::map()` → `glMapBuffer()` → modify → `unmap()`
4. **Cleanup**: Automatic on `OpenGLBuffer` destruction → `glDeleteBuffers()`

## Shader Management

### Compute Shader

- **Location**: `opengl-engine/shaders/compute/game_of_life_3d_opengl.comp`
- **Workgroup Size**: 8x8x8 (512 threads per workgroup)
- **Input**: Uniform buffer (push constants), SSBO (current state)
- **Output**: SSBO (next state)
- **Compilation**: Runtime compilation via `OpenGLCompute::compileShader()`

### Rendering Shaders

- **Vertex Shader**: `opengl-engine/shaders/rendering/voxel.vert`
  - Attributes: position, normal, instance position, LOD, color, age
  - Uniforms: viewProj matrix, camera position, voxel size, frustum planes, LOD distances, time, render mode
  - Output: normal, world position, LOD, color, age

- **Fragment Shader**: `opengl-engine/shaders/rendering/voxel.frag`
  - Input: normal, world position, LOD, color, age
  - Uniforms: viewProj, camera position, voxel size, frustum planes, LOD distances, time, render mode
  - Output: final color (RGBA)

## Error Handling

### OpenGL Error Checking

- Automatic via debug callback (`OpenGLDebugCallback`)
- Manual checking via `OpenGLContext::checkError()`
- Error logging to file and console

### Exception Handling

- RAII wrappers throw on creation failure
- Shader compilation errors logged and return false
- Resource allocation failures throw `std::runtime_error`

## Performance Considerations

### Compute Shader Optimization

- Shared memory for neighbor access (10x10x10 workgroup)
- Toroidal boundary conditions in shader
- Workgroup size: 8x8x8 (optimal for most GPUs)

### Rendering Optimization

- Instance rendering for voxels
- Frustum culling (CPU-side)
- LOD system for distant voxels
- Draw call batching

### Memory Optimization

- GPU-only buffers for state (no CPU access needed)
- Dynamic instance buffer updates
- Resource pooling (via RAII wrappers)

## Threading Model

- **Main Thread**: Window, rendering, input handling
- **Diagnostics**: Thread-safe via mutexes
- **OpenGL Context**: Must be accessed from main thread only
- **Resource Creation**: Thread-safe via `OpenGLMemoryManager` mutex

## Extension Requirements

- **OpenGL 4.3+**: Required for compute shaders
- **GL_ARB_compute_shader**: Fallback if 4.3 not available
- **GL_KHR_debug**: For debug context (optional but recommended)
- **GL_ARB_shader_storage_buffer_object**: For SSBOs

## Future Improvements

- Shader hot-reload
- Multi-threaded resource loading
- Advanced LOD system
- Occlusion culling
- Shadow mapping
- Advanced lighting (PBR)
- Post-processing pipeline
