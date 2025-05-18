# Custom C++ Engine: Vulkan + HIP

## Project Overview

This project is a custom C++ engine targeting AMD hardware, using:
- **Vulkan** for graphics rendering (cross-platform, low-level, AMD-optimized)
- **HIP** (Heterogeneous-compute Interface for Portability) for GPU compute (AMD/CUDA portability)

## User Requirements & Inputs
- Set up a graphics and GPU programming pipeline, AMD-focused
- Use a custom C++ engine
- Use Vulkan for graphics
- Import and set up HIP for compute after Vulkan
- Do not forget HIP setup
- Maintain a backup summary of all user inputs in the home folder of the Docker instance
- Add X11 forwarding for graphics output
- Enable GPU passthrough for Docker
- Add further HIP/Vulkan examples

## Planned Setup Steps
1. Initialize a C++ project with CMake
2. Add Vulkan SDK dependencies
3. Add a minimal Vulkan 'hello triangle' example
4. Add HIP dependencies and a minimal HIP compute example
5. Document build and run instructions
6. Add X11 forwarding to Dockerfile and run commands
7. Add GPU passthrough instructions for Docker

---

## Docker Instructions

### Build the Docker image
```sh
docker build -t cpp-vulkan-hip-engine .
```

### Run the engine in a container

#### For X11 Forwarding (Linux Host):
```sh
docker run --rm \
    -e DISPLAY=$DISPLAY \
    -v /tmp/.X11-unix:/tmp/.X11-unix \
    --device=/dev/dri:/dev/dri \
    cpp-vulkan-hip-engine
```

#### For GPU Passthrough (AMD GPU):
Ensure your system and Docker are configured for GPU passthrough. This usually involves ROCm drivers on the host and `--device=/dev/kfd --device=/dev/dri` flags for Docker.
```sh
docker run --rm \
    --device=/dev/kfd \
    --device=/dev/dri \
    -e DISPLAY=$DISPLAY \
    -v /tmp/.X11-unix:/tmp/.X11-unix \
    cpp-vulkan-hip-engine
```

---

This file is a backup summary of user requirements and project plan. 