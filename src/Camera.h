#pragma once

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <GLFW/glfw3.h>

enum class CameraMode {
    Fly,        // Free-flying camera
    Orbit,      // Orbit around target point
    Pan,        // Pan camera in view plane
    FirstPerson // First-person view with collision
};

class Camera {
public:
    Camera(GLFWwindow* window, float fov = 45.0f, float near = 0.1f, float far = 1000.0f);
    
    void update(float deltaTime);
    glm::mat4 getViewMatrix() const;
    glm::mat4 getProjectionMatrix() const;
    
    // Camera movement
    void moveForward(float distance);
    void moveRight(float distance);
    void moveUp(float distance);
    
    // Camera rotation
    void rotate(float yaw, float pitch);
    void orbit(float yaw, float pitch);
    void pan(float x, float y);
    
    // Mouse control
    void processMouseMovement(float xoffset, float yoffset, bool constrainPitch = true);
    void processMouseScroll(float yoffset);
    
    // Camera mode
    void setMode(CameraMode mode);
    CameraMode getMode() const { return mode; }
    
    // Target point for orbit mode
    void setTarget(const glm::vec3& target) { this->target = target; }
    glm::vec3 getTarget() const { return target; }
    
    // Orbit distance
    void setOrbitDistance(float distance) { orbitDistance = distance; }
    float getOrbitDistance() const { return orbitDistance; }
    
    // Getters
    glm::vec3 getPosition() const { return position; }
    glm::vec3 getFront() const { return front; }
    float getZoom() const { return zoom; }
    
    // Settings
    void setMovementSpeed(float speed) { movementSpeed = speed; }
    float getMovementSpeed() const { return movementSpeed; }
    
    void setMouseSensitivity(float sensitivity) { mouseSensitivity = sensitivity; }
    float getMouseSensitivity() const { return mouseSensitivity; }
    
    void setZoomLimits(float min, float max) { minZoom = min; maxZoom = max; }
    float getMinZoom() const { return minZoom; }
    float getMaxZoom() const { return maxZoom; }
    
private:
    GLFWwindow* window;
    glm::vec3 position;
    glm::vec3 front;
    glm::vec3 up;
    glm::vec3 right;
    glm::vec3 worldUp;
    glm::vec3 target;      // Target point for orbit mode
    
    float yaw;
    float pitch;
    float movementSpeed;
    float mouseSensitivity;
    float zoom;
    float minZoom;
    float maxZoom;
    float orbitDistance;   // Distance from target in orbit mode
    
    float fov;
    float near;
    float far;
    
    CameraMode mode;
    
    void updateCameraVectors();
    void updateOrbitPosition();
}; 