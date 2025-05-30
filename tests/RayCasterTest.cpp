#include <gtest/gtest.h>
#include "RayCaster.h"
#include <glm/gtx/string_cast.hpp>

class RayCasterTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Set up a 10x10x10 grid for testing
        gridMin = glm::vec3(0.0f);
        gridMax = glm::vec3(10.0f);
        voxelSize = 1.0f;
    }

    glm::vec3 gridMin;
    glm::vec3 gridMax;
    float voxelSize;
};

TEST_F(RayCasterTest, RayIntersection) {
    // Test ray from outside hitting the grid
    RayCaster::Ray ray;
    ray.origin = glm::vec3(-5.0f, 5.0f, 5.0f);
    ray.direction = glm::normalize(glm::vec3(1.0f, 0.0f, 0.0f));

    RayCaster::HitResult hit = RayCaster::castRay(ray, gridMin, gridMax);
    EXPECT_TRUE(hit.hit);
    EXPECT_NEAR(hit.position.x, 0.0f, 0.001f);
    EXPECT_NEAR(hit.position.y, 5.0f, 0.001f);
    EXPECT_NEAR(hit.position.z, 5.0f, 0.001f);
    EXPECT_EQ(hit.normal, glm::vec3(-1.0f, 0.0f, 0.0f));
}

TEST_F(RayCasterTest, RayMiss) {
    // Test ray missing the grid
    RayCaster::Ray ray;
    ray.origin = glm::vec3(-5.0f, 5.0f, 5.0f);
    ray.direction = glm::normalize(glm::vec3(-1.0f, 0.0f, 0.0f));

    RayCaster::HitResult hit = RayCaster::castRay(ray, gridMin, gridMax);
    EXPECT_FALSE(hit.hit);
}

TEST_F(RayCasterTest, RayFromInside) {
    // Test ray starting from inside the grid
    RayCaster::Ray ray;
    ray.origin = glm::vec3(5.0f, 5.0f, 5.0f);
    ray.direction = glm::normalize(glm::vec3(1.0f, 0.0f, 0.0f));

    RayCaster::HitResult hit = RayCaster::castRay(ray, gridMin, gridMax);
    EXPECT_TRUE(hit.hit);
    EXPECT_NEAR(hit.position.x, 10.0f, 0.001f);
    EXPECT_NEAR(hit.position.y, 5.0f, 0.001f);
    EXPECT_NEAR(hit.position.z, 5.0f, 0.001f);
    EXPECT_EQ(hit.normal, glm::vec3(1.0f, 0.0f, 0.0f));
}

TEST_F(RayCasterTest, RayIntersectionAllFaces) {
    // Test ray hitting each face of the grid
    std::vector<std::pair<glm::vec3, glm::vec3>> rayTests = {
        // origin, direction, expected normal
        {glm::vec3(-5.0f, 5.0f, 5.0f), glm::vec3(1.0f, 0.0f, 0.0f)},  // Left face
        {glm::vec3(15.0f, 5.0f, 5.0f), glm::vec3(-1.0f, 0.0f, 0.0f)}, // Right face
        {glm::vec3(5.0f, -5.0f, 5.0f), glm::vec3(0.0f, 1.0f, 0.0f)},  // Bottom face
        {glm::vec3(5.0f, 15.0f, 5.0f), glm::vec3(0.0f, -1.0f, 0.0f)}, // Top face
        {glm::vec3(5.0f, 5.0f, -5.0f), glm::vec3(0.0f, 0.0f, 1.0f)},  // Front face
        {glm::vec3(5.0f, 5.0f, 15.0f), glm::vec3(0.0f, 0.0f, -1.0f)}  // Back face
    };

    for (const auto& test : rayTests) {
        RayCaster::Ray ray;
        ray.origin = test.first;
        ray.direction = glm::normalize(test.second);

        RayCaster::HitResult hit = RayCaster::castRay(ray, gridMin, gridMax);
        EXPECT_TRUE(hit.hit) << "Ray from " << glm::to_string(test.first) 
                            << " in direction " << glm::to_string(test.second) 
                            << " should hit the grid";
    }
}

TEST_F(RayCasterTest, GridPositionConversion) {
    // Test world to grid position conversion
    glm::vec3 worldPos(3.5f, 4.2f, 5.7f);
    glm::vec3 gridPos = RayCaster::getGridPosition(worldPos, voxelSize);
    EXPECT_EQ(gridPos, glm::vec3(3.0f, 4.0f, 5.0f));

    // Test grid to world position conversion
    glm::vec3 worldPos2 = RayCaster::getWorldPosition(gridPos, voxelSize);
    EXPECT_EQ(worldPos2, glm::vec3(3.0f, 4.0f, 5.0f));
}

TEST_F(RayCasterTest, GridPositionEdgeCases) {
    // Test positions at grid boundaries
    std::vector<glm::vec3> testPositions = {
        glm::vec3(0.0f, 0.0f, 0.0f),
        glm::vec3(10.0f, 10.0f, 10.0f),
        glm::vec3(-0.1f, -0.1f, -0.1f),
        glm::vec3(10.1f, 10.1f, 10.1f)
    };

    for (const auto& pos : testPositions) {
        glm::vec3 gridPos = RayCaster::getGridPosition(pos, voxelSize);
        glm::vec3 worldPos = RayCaster::getWorldPosition(gridPos, voxelSize);
        
        // For positions outside the grid, we should still get valid grid positions
        EXPECT_TRUE(gridPos.x >= 0.0f && gridPos.x <= 10.0f);
        EXPECT_TRUE(gridPos.y >= 0.0f && gridPos.y <= 10.0f);
        EXPECT_TRUE(gridPos.z >= 0.0f && gridPos.z <= 10.0f);
    }
}

TEST_F(RayCasterTest, RayDistance) {
    // Test ray distance calculation
    RayCaster::Ray ray;
    ray.origin = glm::vec3(-5.0f, 5.0f, 5.0f);
    ray.direction = glm::normalize(glm::vec3(1.0f, 0.0f, 0.0f));

    RayCaster::HitResult hit = RayCaster::castRay(ray, gridMin, gridMax);
    EXPECT_TRUE(hit.hit);
    EXPECT_NEAR(hit.distance, 5.0f, 0.001f);
}

TEST_F(RayCasterTest, RayIntersectionPrecision) {
    // Test ray intersection with high precision
    RayCaster::Ray ray;
    ray.origin = glm::vec3(-5.0f, 5.0f, 5.0f);
    ray.direction = glm::normalize(glm::vec3(1.0f, 0.0001f, 0.0001f));

    RayCaster::HitResult hit = RayCaster::castRay(ray, gridMin, gridMax);
    EXPECT_TRUE(hit.hit);
    EXPECT_NEAR(hit.position.x, 0.0f, 0.001f);
    EXPECT_NEAR(hit.position.y, 5.0f, 0.001f);
    EXPECT_NEAR(hit.position.z, 5.0f, 0.001f);
}

TEST_F(RayCasterTest, RayIntersectionParallel) {
    // Test ray parallel to grid faces
    std::vector<glm::vec3> directions = {
        glm::vec3(1.0f, 0.0f, 0.0f),
        glm::vec3(0.0f, 1.0f, 0.0f),
        glm::vec3(0.0f, 0.0f, 1.0f)
    };

    for (const auto& dir : directions) {
        RayCaster::Ray ray;
        ray.origin = glm::vec3(5.0f, 5.0f, 5.0f);
        ray.direction = dir;

        RayCaster::HitResult hit = RayCaster::castRay(ray, gridMin, gridMax);
        EXPECT_TRUE(hit.hit) << "Ray in direction " << glm::to_string(dir) << " should hit the grid";
    }
} 