#include "Camera.h"
// #include "Grid3D.h"  // Removed for minimal build - collision detection disabled
#include <iostream>
#include <algorithm>

namespace VulkanHIP {

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
      mode(VulkanHIP::CameraMode::Fly),
      grid_(nullptr) {
    updateCameraVectors();
}

void Camera::update(float deltaTime) {
    switch (mode) {
        case VulkanHIP::CameraMode::Fly:
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
            
        case VulkanHIP::CameraMode::Orbit:
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
            
        case VulkanHIP::CameraMode::Pan:
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
            
        case VulkanHIP::CameraMode::FirstPerson:
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
        case VulkanHIP::CameraMode::Orbit:
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
    if (mode == VulkanHIP::CameraMode::Orbit) {
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
        
    if (mode == VulkanHIP::CameraMode::Orbit) {
        orbitDistance -= yoffset * 5.0f;
        if (orbitDistance < 1.0f)
            orbitDistance = 1.0f;
        updateOrbitPosition();
    }
}

void Camera::setMode(VulkanHIP::CameraMode newMode) {
    mode = newMode;
    if (mode == VulkanHIP::CameraMode::Orbit) {
        updateOrbitPosition();
    }
}

bool Camera::checkCollision(const glm::vec3& newPosition) const {
    // Simplified for minimal build - no collision detection
    return false; // No collision, allow all movement
}

glm::vec3 Camera::resolveCollision(const glm::vec3& currentPos, const glm::vec3& targetPos) const {
    // Simplified for minimal build - no collision resolution needed
    return targetPos; // Allow movement to target position
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
}