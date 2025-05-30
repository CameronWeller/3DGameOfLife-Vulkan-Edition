#pragma once

#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

class Grid3D; // Forward declaration

enum class CameraMode {
    Fly,
    Orbit,
    Pan,
    FirstPerson
};

class Camera {
public:
    Camera(GLFWwindow* window, float fov = 45.0f, float near = 0.1f, float far = 1000.0f);
    ~Camera() = default;

    // Core functionality
    void update(float deltaTime);
    glm::mat4 getViewMatrix() const;
    glm::mat4 getProjectionMatrix() const;

    // Movement
    void moveForward(float distance);
    void moveRight(float distance);
    void moveUp(float distance);
    void rotate(float yaw, float pitch);

    // Mouse input
    void processMouseMovement(float xoffset, float yoffset, bool constrainPitch = true);
    void processMouseScroll(float yoffset);

    // Getters
    const glm::vec3& getPosition() const { return position; }
    const glm::vec3& getFront() const { return front; }
    const glm::vec3& getUp() const { return up; }
    const glm::vec3& getRight() const { return right; }
    const glm::vec3& getTarget() const { return target; }
    float getZoom() const { return zoom; }
    CameraMode getMode() const { return mode; }

    // Setters
    void setPosition(const glm::vec3& pos) { position = pos; }
    void setTarget(const glm::vec3& tgt) { target = tgt; }
    void setMode(CameraMode newMode);
    void setGrid(Grid3D* grid) { grid_ = grid; }

private:
    // Camera vectors
    glm::vec3 position;
    glm::vec3 front;
    glm::vec3 up;
    glm::vec3 right;
    glm::vec3 worldUp;
    glm::vec3 target;

    // Euler angles
    float yaw;
    float pitch;

    // Camera options
    float movementSpeed;
    float mouseSensitivity;
    float zoom;
    float minZoom;
    float maxZoom;
    float orbitDistance;

    // Projection parameters
    float fov;
    float near;
    float far;

    // Mode and window
    CameraMode mode;
    GLFWwindow* window;

    // Grid reference for collision detection
    Grid3D* grid_;

    // Helper methods
    void updateCameraVectors();
    bool checkCollision(const glm::vec3& newPosition) const;
    glm::vec3 resolveCollision(const glm::vec3& currentPos, const glm::vec3& targetPos) const;
};