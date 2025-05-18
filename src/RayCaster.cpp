#include "RayCaster.h"
#include <algorithm>

RayCaster::HitResult RayCaster::castRay(const Ray& ray, const glm::vec3& gridMin, const glm::vec3& gridMax) {
    HitResult result;
    result.hit = false;
    
    // Calculate intersection with grid bounds
    float t = getIntersectionDistance(ray, gridMin, gridMax);
    
    if (t >= 0.0f) {
        result.hit = true;
        result.position = ray.origin + ray.direction * t;
        result.normal = getNormal(result.position, gridMin, gridMax);
        result.distance = t;
    }
    
    return result;
}

float RayCaster::getIntersectionDistance(const Ray& ray, const glm::vec3& min, const glm::vec3& max) {
    float tmin = (min.x - ray.origin.x) / ray.direction.x;
    float tmax = (max.x - ray.origin.x) / ray.direction.x;
    
    if (tmin > tmax) std::swap(tmin, tmax);
    
    float tymin = (min.y - ray.origin.y) / ray.direction.y;
    float tymax = (max.y - ray.origin.y) / ray.direction.y;
    
    if (tymin > tymax) std::swap(tymin, tymax);
    
    if ((tmin > tymax) || (tymin > tmax))
        return -1.0f;
    
    if (tymin > tmin)
        tmin = tymin;
    
    if (tymax < tmax)
        tmax = tymax;
    
    float tzmin = (min.z - ray.origin.z) / ray.direction.z;
    float tzmax = (max.z - ray.origin.z) / ray.direction.z;
    
    if (tzmin > tzmax) std::swap(tzmin, tzmax);
    
    if ((tmin > tzmax) || (tzmin > tmax))
        return -1.0f;
    
    if (tzmin > tmin)
        tmin = tzmin;
    
    if (tzmax < tmax)
        tmax = tzmax;
    
    return tmin;
}

glm::vec3 RayCaster::getNormal(const glm::vec3& hitPoint, const glm::vec3& min, const glm::vec3& max) {
    const float epsilon = 0.001f;
    
    if (std::abs(hitPoint.x - min.x) < epsilon) return glm::vec3(-1.0f, 0.0f, 0.0f);
    if (std::abs(hitPoint.x - max.x) < epsilon) return glm::vec3(1.0f, 0.0f, 0.0f);
    if (std::abs(hitPoint.y - min.y) < epsilon) return glm::vec3(0.0f, -1.0f, 0.0f);
    if (std::abs(hitPoint.y - max.y) < epsilon) return glm::vec3(0.0f, 1.0f, 0.0f);
    if (std::abs(hitPoint.z - min.z) < epsilon) return glm::vec3(0.0f, 0.0f, -1.0f);
    if (std::abs(hitPoint.z - max.z) < epsilon) return glm::vec3(0.0f, 0.0f, 1.0f);
    
    return glm::vec3(0.0f);
}

glm::vec3 RayCaster::getGridPosition(const glm::vec3& worldPos, float voxelSize) {
    return glm::vec3(
        std::floor(worldPos.x / voxelSize),
        std::floor(worldPos.y / voxelSize),
        std::floor(worldPos.z / voxelSize)
    );
}

glm::vec3 RayCaster::getWorldPosition(const glm::vec3& gridPos, float voxelSize) {
    return gridPos * voxelSize;
} 