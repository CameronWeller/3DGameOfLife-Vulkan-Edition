#pragma once

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

class Camera {
public:
    Camera() = default;
    ~Camera() = default;

    void setPosition(const glm::vec3& position) { position_ = position; }
    void setTarget(const glm::vec3& target) { target_ = target; }
    void setUp(const glm::vec3& up) { up_ = up; }

    const glm::vec3& getPosition() const { return position_; }
    const glm::vec3& getTarget() const { return target_; }
    const glm::vec3& getUp() const { return up_; }

    glm::mat4 getViewMatrix() const {
        return glm::lookAt(position_, target_, up_);
    }

    void moveForward(float distance) {
        glm::vec3 direction = glm::normalize(target_ - position_);
        position_ += direction * distance;
        target_ += direction * distance;
    }

    void moveRight(float distance) {
        glm::vec3 direction = glm::normalize(target_ - position_);
        glm::vec3 right = glm::normalize(glm::cross(direction, up_));
        position_ += right * distance;
        target_ += right * distance;
    }

    void moveUp(float distance) {
        position_ += up_ * distance;
        target_ += up_ * distance;
    }

    void rotate(float yaw, float pitch) {
        glm::vec3 direction = glm::normalize(target_ - position_);
        glm::vec3 right = glm::normalize(glm::cross(direction, up_));

        // Rotate around up vector (yaw)
        glm::mat4 yawRotation = glm::rotate(glm::mat4(1.0f), yaw, up_);
        direction = glm::vec3(yawRotation * glm::vec4(direction, 0.0f));

        // Rotate around right vector (pitch)
        glm::mat4 pitchRotation = glm::rotate(glm::mat4(1.0f), pitch, right);
        direction = glm::vec3(pitchRotation * glm::vec4(direction, 0.0f));

        target_ = position_ + direction;
    }

private:
    glm::vec3 position_ = glm::vec3(0.0f, 0.0f, 0.0f);
    glm::vec3 target_ = glm::vec3(0.0f, 0.0f, -1.0f);
    glm::vec3 up_ = glm::vec3(0.0f, 1.0f, 0.0f);
}; 