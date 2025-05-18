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
    libglm-dev && \
    rm -rf /var/lib/apt/lists/*

# Install Vulkan SDK
RUN wget -qO- https://packages.lunarg.com/lunarg-signing-key-pub.asc | apt-key add - && \
    wget -qO /etc/apt/sources.list.d/lunarg-vulkan-jammy.list http://packages.lunarg.com/vulkan/lunarg-vulkan-jammy.list && \
    apt-get update && \
    apt-get install -y vulkan-sdk && \
    rm -rf /var/lib/apt/lists/*

# Set working directory
WORKDIR /workspace

# Copy project files
COPY . /workspace

# Build the project
RUN cmake -S . -B build && \
    cmake --build build

# Default command
CMD ["/workspace/build/engine"] 