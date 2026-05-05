# OpenGL Beta Isolation Handoff - 2026-05-04

## Summary

This pass established the OpenGL foundation branch as the practical playable-beta path for 3D Game of Life. The Vulkan/main branch still has structural build breakage in shared game/rendering components, so the beta work isolated the OpenGL executable from those parent libraries and made the OpenGL app build, launch, initialize rendering, initialize compute, and draw voxel instances.

## Repository State

- Working branch: `codex/opengl-beta-isolation`
- Base branch: `origin/feature/opengl-foundation`
- Remote: `https://github.com/CameronWeller/3DGameOfLife-Vulkan-Edition.git`
- Local worktree: `C:\Dev\research\graphics\3DGameOfLife-opengl-foundation`

## Work Performed

- Isolated `3DGameOfLife-Vulkan-Edition-OpenGL` from broken parent CMake targets by compiling only the parent sources the OpenGL app actually needs.
- Added the vcpkg `glad::glad` target and aligned the OpenGL executable with the vcpkg debug DLL runtime on MSVC.
- Fixed OpenGL include path drift for local engine headers.
- Fixed logger usage in `main_opengl.cpp` to use the singleton API.
- Corrected namespace drift around `PatternManager` for the OpenGL grid.
- Fixed GLAD loading for the vcpkg GLAD package.
- Guarded OpenGL extension checks so a core-profile OpenGL context does not crash when `glGetString(GL_EXTENSIONS)` is unavailable.
- Fixed the OpenGL debug callback declaration for MSVC.
- Disabled ImGui docking only when the vcpkg ImGui build does not expose the docking flag.
- Copied the OpenGL shader tree beside the executable after build.
- Fixed the voxel renderer draw path by neutralizing default culling planes and explicitly binding the index buffer before instanced draws.
- Added `build_*/` to `.gitignore` so local CMake probe directories do not pollute review diffs.

## Build

Configure:

```powershell
cmake -S . -B build_codex_opengl -G "Visual Studio 17 2022" -A x64 -DBUILD_TESTING=OFF -DCMAKE_TOOLCHAIN_FILE="C:/Dev/research/graphics/3DGameOfLife-Vulkan-Edition/3DGameOfLife-Vulkan-Edition/vcpkg/scripts/buildsystems/vcpkg.cmake"
```

Build:

```powershell
cmake --build build_codex_opengl --config Debug --target 3DGameOfLife-Vulkan-Edition-OpenGL
```

Run:

```powershell
.\build_codex_opengl\opengl-engine\Debug\3DGameOfLife-Vulkan-Edition-OpenGL.exe
```

## Validation

- Configure completed with Visual Studio 17 2022 and the repo vcpkg toolchain.
- Debug build completed for `3DGameOfLife-Vulkan-Edition-OpenGL`.
- Runtime probe launched the executable, let it run for 8 seconds, then stopped the launched process.
- Latest runtime log showed:
  - OpenGL 4.3 initialized on NVIDIA GeForce RTX 5080.
  - Renderer, voxel renderer, compute shader, grid, and ImGui initialized.
  - Randomized `32x32x32` population initialized.
  - Instanced voxel draw calls recorded with nonzero vertex counts.

## Remaining Beta Follow-Ups

- Wire `R` to reset or randomize the grid.
- Add visible in-app controls for play, pause, reset, simulation speed, density, and rule selection.
- Confirm camera position and voxel scale are comfortable on first launch.
- Clean remaining MSVC warnings in `OpenGLContext.cpp` and `Camera.cpp`.
- Decide whether to PR this branch into `feature/opengl-foundation` first or promote it as the new beta branch against `main`.
