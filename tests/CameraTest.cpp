#include <gtest/gtest.h>
#include "Camera.h"
#include <GLFW/glfw3.h>
#include <memory>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtx/string_cast.hpp>

class CameraTest : public ::testing::Test {
protected:
    void SetUp() override {
        if (!glfwInit()) {
            throw std::runtime_error("Failed to initialize GLFW");
        }
        window = glfwCreateWindow(800, 600, "Camera Test", nullptr, nullptr);
        if (!window) {
            glfwTerminate();
            throw std::runtime_error("Failed to create GLFW window");
        }
        camera = std::make_unique<Camera>(window, 45.0f, 0.1f, 1000.0f);
    }

    void TearDown() override {
        camera.reset();
        glfwDestroyWindow(window);
        glfwTerminate();
    }

    GLFWwindow* window;
    std::unique_ptr<Camera> camera;
};

TEST_F(CameraTest, Initialization) {
    EXPECT_EQ(camera->getPosition(), glm::vec3(125.0f, 125.0f, 125.0f));
    EXPECT_EQ(camera->getTarget(), glm::vec3(125.0f, 125.0f, 125.0f));
    EXPECT_EQ(camera->getUp(), glm::vec3(0.0f, 1.0f, 0.0f));
}

TEST_F(CameraTest, Movement) {
    // Test forward movement
    camera->moveForward(10.0f);
    glm::vec3 expectedPos = camera->getPosition() + glm::normalize(camera->getTarget() - camera->getPosition()) * 10.0f;
    EXPECT_NEAR(camera->getPosition().x, expectedPos.x, 0.001f);
    EXPECT_NEAR(camera->getPosition().y, expectedPos.y, 0.001f);
    EXPECT_NEAR(camera->getPosition().z, expectedPos.z, 0.001f);

    // Test right movement
    glm::vec3 initialPos = camera->getPosition();
    camera->moveRight(10.0f);
    glm::vec3 direction = glm::normalize(camera->getTarget() - camera->getPosition());
    glm::vec3 right = glm::normalize(glm::cross(direction, camera->getUp()));
    glm::vec3 expectedRightPos = initialPos + right * 10.0f;
    EXPECT_NEAR(camera->getPosition().x, expectedRightPos.x, 0.001f);
    EXPECT_NEAR(camera->getPosition().y, expectedRightPos.y, 0.001f);
    EXPECT_NEAR(camera->getPosition().z, expectedRightPos.z, 0.001f);

    // Test up movement
    initialPos = camera->getPosition();
    camera->moveUp(10.0f);
    glm::vec3 expectedUpPos = initialPos + camera->getUp() * 10.0f;
    EXPECT_NEAR(camera->getPosition().x, expectedUpPos.x, 0.001f);
    EXPECT_NEAR(camera->getPosition().y, expectedUpPos.y, 0.001f);
    EXPECT_NEAR(camera->getPosition().z, expectedUpPos.z, 0.001f);
}

TEST_F(CameraTest, Rotation) {
    // Test yaw rotation
    float initialYaw = camera->getYaw();
    camera->rotate(45.0f, 0.0f);
    EXPECT_NEAR(camera->getYaw(), initialYaw + 45.0f, 0.001f);

    // Test pitch rotation
    float initialPitch = camera->getPitch();
    camera->rotate(0.0f, 30.0f);
    EXPECT_NEAR(camera->getPitch(), initialPitch + 30.0f, 0.001f);

    // Test pitch constraints
    camera->rotate(0.0f, 100.0f); // Should be clamped to 89 degrees
    EXPECT_NEAR(camera->getPitch(), 89.0f, 0.001f);
    camera->rotate(0.0f, -200.0f); // Should be clamped to -89 degrees
    EXPECT_NEAR(camera->getPitch(), -89.0f, 0.001f);
}

TEST_F(CameraTest, CameraModes) {
    // Test Fly mode
    camera->setMode(CameraMode::Fly);
    EXPECT_EQ(camera->getMode(), CameraMode::Fly);
    
    // Test Orbit mode
    camera->setMode(CameraMode::Orbit);
    EXPECT_EQ(camera->getMode(), CameraMode::Orbit);
    
    // Test Pan mode
    camera->setMode(CameraMode::Pan);
    EXPECT_EQ(camera->getMode(), CameraMode::Pan);
    
    // Test FirstPerson mode
    camera->setMode(CameraMode::FirstPerson);
    EXPECT_EQ(camera->getMode(), CameraMode::FirstPerson);
}

TEST_F(CameraTest, ViewMatrix) {
    // Test view matrix calculation
    glm::mat4 viewMatrix = camera->getViewMatrix();
    EXPECT_TRUE(glm::determinant(viewMatrix) != 0.0f); // Should be invertible
    
    // Test that view matrix correctly transforms camera position to origin
    glm::vec4 cameraPos = viewMatrix * glm::vec4(camera->getPosition(), 1.0f);
    EXPECT_NEAR(cameraPos.x, 0.0f, 0.001f);
    EXPECT_NEAR(cameraPos.y, 0.0f, 0.001f);
    EXPECT_NEAR(cameraPos.z, 0.0f, 0.001f);
}

TEST_F(CameraTest, ProjectionMatrix) {
    // Test projection matrix calculation
    glm::mat4 projMatrix = camera->getProjectionMatrix();
    EXPECT_TRUE(glm::determinant(projMatrix) != 0.0f); // Should be invertible
    
    // Test that near plane is at z = -1 and far plane is at z = 1
    glm::vec4 nearPoint = projMatrix * glm::vec4(0.0f, 0.0f, -0.1f, 1.0f);
    glm::vec4 farPoint = projMatrix * glm::vec4(0.0f, 0.0f, -1000.0f, 1.0f);
    EXPECT_NEAR(nearPoint.z / nearPoint.w, -1.0f, 0.001f);
    EXPECT_NEAR(farPoint.z / farPoint.w, 1.0f, 0.001f);
}

TEST_F(CameraTest, MouseInput) {
    // Test mouse movement processing
    float initialYaw = camera->getYaw();
    float initialPitch = camera->getPitch();
    
    camera->processMouseMovement(10.0f, 5.0f, true);
    EXPECT_NEAR(camera->getYaw(), initialYaw + 10.0f * camera->getMouseSensitivity(), 0.001f);
    EXPECT_NEAR(camera->getPitch(), initialPitch + 5.0f * camera->getMouseSensitivity(), 0.001f);
}

TEST_F(CameraTest, Zoom) {
    // Test zoom functionality
    float initialZoom = camera->getZoom();
    
    camera->processMouseScroll(1.0f);
    EXPECT_NEAR(camera->getZoom(), initialZoom - 1.0f, 0.001f);
    
    // Test zoom constraints
    camera->processMouseScroll(100.0f); // Should be clamped to maxZoom
    EXPECT_NEAR(camera->getZoom(), camera->getMaxZoom(), 0.001f);
    
    camera->processMouseScroll(-100.0f); // Should be clamped to minZoom
    EXPECT_NEAR(camera->getZoom(), camera->getMinZoom(), 0.001f);
}

TEST_F(CameraTest, OrbitMode) {
    camera->setMode(CameraMode::Orbit);
    
    // Test orbit distance
    float initialDistance = camera->getOrbitDistance();
    camera->processMouseScroll(10.0f);
    EXPECT_NEAR(camera->getOrbitDistance(), initialDistance - 10.0f * 5.0f, 0.001f);
    
    // Test orbit rotation
    float initialYaw = camera->getYaw();
    float initialPitch = camera->getPitch();
    camera->orbit(45.0f, 30.0f);
    EXPECT_NEAR(camera->getYaw(), initialYaw + 45.0f, 0.001f);
    EXPECT_NEAR(camera->getPitch(), initialPitch + 30.0f, 0.001f);
}

TEST_F(CameraTest, PanMode) {
    camera->setMode(CameraMode::Pan);
    
    // Test panning
    glm::vec3 initialPos = camera->getPosition();
    glm::vec3 initialTarget = camera->getTarget();
    
    camera->pan(10.0f, 5.0f);
    
    glm::vec3 right = glm::normalize(glm::cross(
        glm::normalize(camera->getTarget() - camera->getPosition()),
        camera->getUp()
    ));
    
    glm::vec3 expectedPos = initialPos + right * 10.0f * camera->getMovementSpeed() +
                           camera->getUp() * 5.0f * camera->getMovementSpeed();
    glm::vec3 expectedTarget = initialTarget + right * 10.0f * camera->getMovementSpeed() +
                              camera->getUp() * 5.0f * camera->getMovementSpeed();
    
    EXPECT_NEAR(camera->getPosition().x, expectedPos.x, 0.001f);
    EXPECT_NEAR(camera->getPosition().y, expectedPos.y, 0.001f);
    EXPECT_NEAR(camera->getPosition().z, expectedPos.z, 0.001f);
    EXPECT_NEAR(camera->getTarget().x, expectedTarget.x, 0.001f);
    EXPECT_NEAR(camera->getTarget().y, expectedTarget.y, 0.001f);
    EXPECT_NEAR(camera->getTarget().z, expectedTarget.z, 0.001f);
} 