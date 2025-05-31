#pragma once

#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

namespace VulkanHIP {

class Grid3D; // Forward declaration

enum class CameraMode {
    Fly,
    Orbit,
    Pan,
    FirstPerson
};

class Camera {
public:
    Camera(GLFWwindow* window = nullptr, float fov = 45.0f, float near = 0.1f, float far = 1000.0f);
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
    glm::vec3 position{0.0f, 0.0f, 0.0f};
    glm::vec3 front{0.0f, 0.0f, -1.0f};
    glm::vec3 up{0.0f, 1.0f, 0.0f};
    glm::vec3 right{1.0f, 0.0f, 0.0f};
    glm::vec3 worldUp{0.0f, 1.0f, 0.0f};
    glm::vec3 target{0.0f, 0.0f, 0.0f};

    // Euler angles
    float yaw = -90.0f;
    float pitch = 0.0f;

    // Camera options
    float movementSpeed = 2.5f;
    float mouseSensitivity = 0.1f;
    float zoom = 45.0f;
    float minZoom = 1.0f;
    float maxZoom = 90.0f;
    float orbitDistance = 5.0f;

    // Projection parameters
    float fov = 45.0f;
    float near = 0.1f;
    float far = 1000.0f;

    // Mode and window
    CameraMode mode = CameraMode::Fly;
    GLFWwindow* window = nullptr;

    // Grid reference for collision detection
    Grid3D* grid_ = nullptr;

    // Helper methods
    void updateCameraVectors();
    bool checkCollision(const glm::vec3& newPosition) const;
    glm::vec3 resolveCollision(const glm::vec3& currentPos, const glm::vec3& targetPos) const;
};

} // namespace VulkanHIP