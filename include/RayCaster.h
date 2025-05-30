#pragma once

#include <glm/glm.hpp>

class RayCaster {
public:
    struct Ray {
        glm::vec3 origin;
        glm::vec3 direction;
    };

    struct HitResult {
        bool hit;
        glm::vec3 position;
        glm::vec3 normal;
        float distance;
    };

    static HitResult castRay(const Ray& ray, const glm::vec3& gridMin, const glm::vec3& gridMax);
    static glm::vec3 getGridPosition(const glm::vec3& worldPos, float voxelSize);
    static glm::vec3 getWorldPosition(const glm::vec3& gridPos, float voxelSize);

private:
    static float getIntersectionDistance(const Ray& ray, const glm::vec3& min, const glm::vec3& max);
    static glm::vec3 getNormal(const glm::vec3& hitPoint, const glm::vec3& min, const glm::vec3& max);
}; 