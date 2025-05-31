#include "Camera.h"
#include <iostream>

namespace VulkanHIP {

Camera::Camera(GLFWwindow* window, float fov, float near, float far)
    : window(window), fov(fov), near(near), far(far) {
    position = glm::vec3(0.0f, 0.0f, 3.0f);
    target = glm::vec3(0.0f, 0.0f, 0.0f);
    updateCameraVectors();
}

void Camera::update(float deltaTime) {
    // Minimal update - just ensure vectors are current
    updateCameraVectors();
}

glm::mat4 Camera::getViewMatrix() const {
    return glm::lookAt(position, position + front, up);
}

glm::mat4 Camera::getProjectionMatrix() const {
    if (window) {
        int width, height;
        glfwGetFramebufferSize(window, &width, &height);
        if (width > 0 && height > 0) {
            return glm::perspective(glm::radians(fov), 
                                  static_cast<float>(width) / static_cast<float>(height), 
                                  near, far);
        }
    }
    return glm::perspective(glm::radians(fov), 16.0f / 9.0f, near, far);
}

void Camera::setMode(CameraMode newMode) {
    mode = newMode;
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

void Camera::rotate(float yawOffset, float pitchOffset) {
    yaw += yawOffset;
    pitch += pitchOffset;
    
    // Constrain pitch
    if (pitch > 89.0f) pitch = 89.0f;
    if (pitch < -89.0f) pitch = -89.0f;
    
    updateCameraVectors();
}

void Camera::processMouseMovement(float xoffset, float yoffset, bool constrainPitch) {
    xoffset *= mouseSensitivity;
    yoffset *= mouseSensitivity;
    
    yaw += xoffset;
    pitch += yoffset;
    
    if (constrainPitch) {
        if (pitch > 89.0f) pitch = 89.0f;
        if (pitch < -89.0f) pitch = -89.0f;
    }
    
    updateCameraVectors();
}

void Camera::processMouseScroll(float yoffset) {
    zoom -= yoffset;
    if (zoom < minZoom) zoom = minZoom;
    if (zoom > maxZoom) zoom = maxZoom;
}

void Camera::updateCameraVectors() {
    // Calculate the new front vector
    glm::vec3 newFront;
    newFront.x = cos(glm::radians(yaw)) * cos(glm::radians(pitch));
    newFront.y = sin(glm::radians(pitch));
    newFront.z = sin(glm::radians(yaw)) * cos(glm::radians(pitch));
    front = glm::normalize(newFront);
    
    // Re-calculate the right and up vector
    right = glm::normalize(glm::cross(front, worldUp));
    up = glm::normalize(glm::cross(right, front));
}

} // namespace VulkanHIP 