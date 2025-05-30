# Use Ubuntu 22.04 as base image
FROM ubuntu:22.04

# Avoid interactive prompts during package installation
ENV DEBIAN_FRONTEND=noninteractive

# Install essential build tools and dependencies
RUN apt-get update && apt-get install -y \
    build-essential \
    cmake \
    ninja-build \
    git \
    wget \
    unzip \
    pkg-config \
    python3 \
    && rm -rf /var/lib/apt/lists/*

# Install Vulkan SDK
RUN wget -qO- https://packages.lunarg.com/lunarg-signing-key-pub.asc | apt-key add - \
    && wget -qO /etc/apt/sources.list.d/lunarg-vulkan-jammy.list http://packages.lunarg.com/vulkan/lunarg-vulkan-jammy.list \
    && apt-get update \
    && apt-get install -y vulkan-sdk \
    && rm -rf /var/lib/apt/lists/*

# Set up working directory
WORKDIR /app

# Copy CMake configuration
COPY CMakeLists.txt .
COPY cmake/ cmake/

# Create build directory
RUN mkdir build

# Set environment variables
ENV VULKAN_SDK=/usr
ENV PATH=$PATH:/usr/bin

# Default command
CMD ["/bin/bash"] 