FROM ubuntu:22.04

# Install dependencies
RUN apt-get update && \
    DEBIAN_FRONTEND=noninteractive apt-get install -y \
    build-essential \
    cmake \
    git \
    wget \
    pkg-config \
    x11-apps \
    x11-xserver-utils \
    xauth \
    mesa-utils \
    libgl1-mesa-glx \
    libgl1-mesa-dri \
    x11vnc \
    xvfb \
    libx11-dev \
    libxrandr-dev \
    libxinerama-dev \
    libxcursor-dev \
    libxi-dev \
    libglm-dev \
    libgl1-mesa-dev \
    libglu1-mesa-dev && \
    rm -rf /var/lib/apt/lists/*

# Install Vulkan SDK and validation layers
RUN wget -qO- https://packages.lunarg.com/lunarg-signing-key-pub.asc | apt-key add - && \
    wget -qO /etc/apt/sources.list.d/lunarg-vulkan-jammy.list http://packages.lunarg.com/vulkan/lunarg-vulkan-jammy.list && \
    apt-get update && \
    apt-get install -y vulkan-sdk=1.3.239.0-1 vulkan-validationlayers && \
    # Pin other critical packages as needed (e.g., cmake, gcc, etc.) \
    rm -rf /var/lib/apt/lists/*

# Set environment variables for Vulkan
ENV VK_LAYER_PATH=/usr/share/vulkan/explicit_layer.d
ENV VK_INSTANCE_LAYERS=VK_LAYER_KHRONOS_validation

# Set working directory
WORKDIR /workspace

# Copy project files
COPY . /workspace

# Build the project
RUN cmake -S . -B build && \
    cmake --build build --verbose && \
    ls -la build && \
    ls -la build/shaders

# Default command
CMD ["/workspace/build/vulkan-engine"] 