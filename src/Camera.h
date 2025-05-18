#pragma once

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <GLFW/glfw3.h>

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
    
    // Mouse control
    void processMouseMovement(float xoffset, float yoffset, bool constrainPitch = true);
    void processMouseScroll(float yoffset);
    
    // Getters
    glm::vec3 getPosition() const { return position; }
    glm::vec3 getFront() const { return front; }
    float getZoom() const { return zoom; }
    
private:
    GLFWwindow* window;
    glm::vec3 position;
    glm::vec3 front;
    glm::vec3 up;
    glm::vec3 right;
    glm::vec3 worldUp;
    
    float yaw;
    float pitch;
    float movementSpeed;
    float mouseSensitivity;
    float zoom;
    
    float fov;
    float near;
    float far;
    
    void updateCameraVectors();
}; 