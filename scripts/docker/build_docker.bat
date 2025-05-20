@echo off
echo Building Docker container...

REM Clean up any existing containers and images
docker-compose down
docker rmi cpp-vulkan-hip-engine_vulkan-engine

REM Build the container
docker-compose build --no-cache

echo.
echo Build complete! You can now run the container with:
echo docker-compose run --rm vulkan-engine 