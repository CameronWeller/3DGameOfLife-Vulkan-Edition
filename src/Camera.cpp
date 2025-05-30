#include "Camera.h"
#include "Grid3D.h"
#include <iostream>
#include <algorithm>

Camera::Camera(GLFWwindow* window, float fov, float near, float far)
    : window(window), fov(fov), near(near), far(far),
      position(glm::vec3(125.0f, 125.0f, 125.0f)), // Start at center of 250^3 grid
      worldUp(glm::vec3(0.0f, 1.0f, 0.0f)),
      target(glm::vec3(125.0f, 125.0f, 125.0f)),
      yaw(-90.0f), pitch(0.0f),
      movementSpeed(50.0f),
      mouseSensitivity(0.1f),
      zoom(45.0f),
      minZoom(1.0f),
      maxZoom(45.0f),
      orbitDistance(100.0f),
      mode(CameraMode::Fly),
      grid_(nullptr) {
    updateCameraVectors();
}

void Camera::update(float deltaTime) {
    switch (mode) {
        case CameraMode::Fly:
            // Keyboard input for flying
            if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS)
                moveForward(movementSpeed * deltaTime);
            if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS)
                moveForward(-movementSpeed * deltaTime);
            if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS)
                moveRight(-movementSpeed * deltaTime);
            if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS)
                moveRight(movementSpeed * deltaTime);
            if (glfwGetKey(window, GLFW_KEY_SPACE) == GLFW_PRESS)
                moveUp(movementSpeed * deltaTime);
            if (glfwGetKey(window, GLFW_KEY_LEFT_CONTROL) == GLFW_PRESS)
                moveUp(-movementSpeed * deltaTime);
            break;
            
        case CameraMode::Orbit:
            // Mouse right button for orbiting
            if (glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_RIGHT) == GLFW_PRESS) {
                double xpos, ypos;
                glfwGetCursorPos(window, &xpos, &ypos);
                static double lastX = xpos, lastY = ypos;
                
                float xoffset = xpos - lastX;
                float yoffset = lastY - ypos;
                
                orbit(xoffset * mouseSensitivity, yoffset * mouseSensitivity);
                
                lastX = xpos;
                lastY = ypos;
            }
            break;
            
        case CameraMode::Pan:
            // Mouse middle button for panning
            if (glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_MIDDLE) == GLFW_PRESS) {
                double xpos, ypos;
                glfwGetCursorPos(window, &xpos, &ypos);
                static double lastX = xpos, lastY = ypos;
                
                float xoffset = xpos - lastX;
                float yoffset = lastY - ypos;
                
                pan(xoffset * mouseSensitivity, yoffset * mouseSensitivity);
                
                lastX = xpos;
                lastY = ypos;
            }
            break;
            
        case CameraMode::FirstPerson:
            // Similar to Fly but with collision detection
            if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS) {
                glm::vec3 newPos = position + front * movementSpeed * deltaTime;
                if (!checkCollision(newPos)) {
                    moveForward(movementSpeed * deltaTime);
                } else {
                    position = resolveCollision(position, newPos);
                }
            }
            if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS) {
                glm::vec3 newPos = position - front * movementSpeed * deltaTime;
                if (!checkCollision(newPos)) {
                    moveForward(-movementSpeed * deltaTime);
                } else {
                    position = resolveCollision(position, newPos);
                }
            }
            if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS) {
                glm::vec3 newPos = position - right * movementSpeed * deltaTime;
                if (!checkCollision(newPos)) {
                    moveRight(-movementSpeed * deltaTime);
                } else {
                    position = resolveCollision(position, newPos);
                }
            }
            if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS) {
                glm::vec3 newPos = position + right * movementSpeed * deltaTime;
                if (!checkCollision(newPos)) {
                    moveRight(movementSpeed * deltaTime);
                } else {
                    position = resolveCollision(position, newPos);
                }
            }
            break;
    }
}

glm::mat4 Camera::getViewMatrix() const {
    switch (mode) {
        case CameraMode::Orbit:
            return glm::lookAt(position, target, up);
        default:
            return glm::lookAt(position, position + front, up);
    }
}

glm::mat4 Camera::getProjectionMatrix() const {
    int width, height;
    glfwGetFramebufferSize(window, &width, &height);
    return glm::perspective(glm::radians(zoom), (float)width / (float)height, near, far);
}

void Camera::moveForward(float distance) {
    position += front * distance;
}

void Camera::moveRight(float distance) {
    position += right * distance;
}

void Camera::moveUp(float distance) {
    position += up * distance;
}

void Camera::rotate(float yaw, float pitch) {
    this->yaw += yaw;
    this->pitch += pitch;
    
    // Constrain pitch
    if (this->pitch > 89.0f)
        this->pitch = 89.0f;
    if (this->pitch < -89.0f)
        this->pitch = -89.0f;
    
    updateCameraVectors();
}

void Camera::orbit(float yaw, float pitch) {
    this->yaw += yaw;
    this->pitch += pitch;
    
    // Constrain pitch
    if (this->pitch > 89.0f)
        this->pitch = 89.0f;
    if (this->pitch < -89.0f)
        this->pitch = -89.0f;
    
    updateCameraVectors();
    updateOrbitPosition();
}

void Camera::pan(float x, float y) {
    position += right * x * movementSpeed;
    position += up * y * movementSpeed;
    target += right * x * movementSpeed;
    target += up * y * movementSpeed;
}

void Camera::processMouseMovement(float xoffset, float yoffset, bool constrainPitch) {
    if (mode == CameraMode::Orbit) {
        orbit(xoffset, yoffset);
    } else {
        xoffset *= mouseSensitivity;
        yoffset *= mouseSensitivity;
        
        yaw += xoffset;
        pitch += yoffset;
        
        if (constrainPitch) {
            if (pitch > 89.0f)
                pitch = 89.0f;
            if (pitch < -89.0f)
                pitch = -89.0f;
        }
        
        updateCameraVectors();
    }
}

void Camera::processMouseScroll(float yoffset) {
    zoom -= yoffset;
    if (zoom < minZoom)
        zoom = minZoom;
    if (zoom > maxZoom)
        zoom = maxZoom;
        
    if (mode == CameraMode::Orbit) {
        orbitDistance -= yoffset * 5.0f;
        if (orbitDistance < 1.0f)
            orbitDistance = 1.0f;
        updateOrbitPosition();
    }
}

void Camera::setMode(CameraMode newMode) {
    mode = newMode;
    if (mode == CameraMode::Orbit) {
        updateOrbitPosition();
    }
}

bool Camera::checkCollision(const glm::vec3& newPosition) const {
    if (!grid_) {
        return false; // No grid reference, no collision
    }

    // Define camera collision radius (half a voxel)
    const float collisionRadius = 0.5f;
    
    // Check grid boundaries
    if (newPosition.x < collisionRadius || newPosition.x >= grid_->getWidth() - collisionRadius ||
        newPosition.y < collisionRadius || newPosition.y >= grid_->getHeight() - collisionRadius ||
        newPosition.z < collisionRadius || newPosition.z >= grid_->getDepth() - collisionRadius) {
        return true; // Collision with grid boundaries
    }

    // Check collision with alive cells in a small radius around the camera
    int minX = std::max(0, static_cast<int>(newPosition.x - collisionRadius));
    int maxX = std::min(static_cast<int>(grid_->getWidth() - 1), static_cast<int>(newPosition.x + collisionRadius));
    int minY = std::max(0, static_cast<int>(newPosition.y - collisionRadius));
    int maxY = std::min(static_cast<int>(grid_->getHeight() - 1), static_cast<int>(newPosition.y + collisionRadius));
    int minZ = std::max(0, static_cast<int>(newPosition.z - collisionRadius));
    int maxZ = std::min(static_cast<int>(grid_->getDepth() - 1), static_cast<int>(newPosition.z + collisionRadius));

    for (int x = minX; x <= maxX; ++x) {
        for (int y = minY; y <= maxY; ++y) {
            for (int z = minZ; z <= maxZ; ++z) {
                if (grid_->getCell(x, y, z)) {
                    // Calculate distance from camera center to cell center
                    glm::vec3 cellCenter(x + 0.5f, y + 0.5f, z + 0.5f);
                    float distance = glm::length(newPosition - cellCenter);
                    if (distance < collisionRadius + 0.5f) { // Cell half-size + camera radius
                        return true; // Collision detected
                    }
                }
            }
        }
    }

    return false; // No collision
}

glm::vec3 Camera::resolveCollision(const glm::vec3& currentPos, const glm::vec3& targetPos) const {
    if (!grid_) {
        return targetPos; // No grid reference, allow movement
    }

    // Try sliding along each axis separately
    glm::vec3 movement = targetPos - currentPos;
    
    // Try X-axis movement only
    glm::vec3 xOnlyPos = currentPos + glm::vec3(movement.x, 0.0f, 0.0f);
    if (!checkCollision(xOnlyPos)) {
        return xOnlyPos;
    }
    
    // Try Y-axis movement only
    glm::vec3 yOnlyPos = currentPos + glm::vec3(0.0f, movement.y, 0.0f);
    if (!checkCollision(yOnlyPos)) {
        return yOnlyPos;
    }
    
    // Try Z-axis movement only
    glm::vec3 zOnlyPos = currentPos + glm::vec3(0.0f, 0.0f, movement.z);
    if (!checkCollision(zOnlyPos)) {
        return zOnlyPos;
    }
    
    // Try XY movement
    glm::vec3 xyPos = currentPos + glm::vec3(movement.x, movement.y, 0.0f);
    if (!checkCollision(xyPos)) {
        return xyPos;
    }
    
    // Try XZ movement
    glm::vec3 xzPos = currentPos + glm::vec3(movement.x, 0.0f, movement.z);
    if (!checkCollision(xzPos)) {
        return xzPos;
    }
    
    // Try YZ movement
    glm::vec3 yzPos = currentPos + glm::vec3(0.0f, movement.y, movement.z);
    if (!checkCollision(yzPos)) {
        return yzPos;
    }
    
    // No valid movement, stay at current position
    return currentPos;
}

void Camera::updateCameraVectors() {
    glm::vec3 newFront;
    newFront.x = cos(glm::radians(yaw)) * cos(glm::radians(pitch));
    newFront.y = sin(glm::radians(pitch));
    newFront.z = sin(glm::radians(yaw)) * cos(glm::radians(pitch));
    front = glm::normalize(newFront);
    
    right = glm::normalize(glm::cross(front, worldUp));
    up = glm::normalize(glm::cross(right, front));
}

void Camera::updateOrbitPosition() {
    // Calculate new position based on target, orbit distance, and angles
    position.x = target.x + orbitDistance * cos(glm::radians(yaw)) * cos(glm::radians(pitch));
    position.y = target.y + orbitDistance * sin(glm::radians(pitch));
    position.z = target.z + orbitDistance * sin(glm::radians(yaw)) * cos(glm::radians(pitch));
    
    // Update camera vectors
    front = glm::normalize(target - position);
    right = glm::normalize(glm::cross(front, worldUp));
    up = glm::normalize(glm::cross(right, front));
}