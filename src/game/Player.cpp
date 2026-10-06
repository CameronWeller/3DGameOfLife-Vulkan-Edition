#include "game/Player.h"

#include <algorithm>
#include <cmath>
#include <limits>

namespace gol3d {
namespace {

// True when the box from `low` to `high` overlaps the unit cell at `cell`.
bool boxOverlapsCell(const glm::vec3& low, const glm::vec3& high, const glm::vec3& cell) {
    return cell.x < high.x && cell.x + 1.0f > low.x && cell.y < high.y && cell.y + 1.0f > low.y &&
           cell.z < high.z && cell.z + 1.0f > low.z;
}

// The axis (0, 1 or 2) of the smallest component; ties go to the later axis.
int smallestAxis(const glm::vec3& v) {
    if (v.x < v.y) return v.x < v.z ? 0 : 2;
    return v.y < v.z ? 1 : 2;
}

// The axis of the largest component; ties go to the later axis.
int largestAxis(const glm::vec3& v) {
    if (v.x > v.y) return v.x > v.z ? 0 : 2;
    return v.y > v.z ? 1 : 2;
}

} // namespace

glm::vec3 Player::boxMin(const glm::vec3& eyePosition) {
    return eyePosition - glm::vec3(WIDTH / 2, EYE_HEIGHT, WIDTH / 2);
}

glm::vec3 Player::boxMax(const glm::vec3& eyePosition) {
    return eyePosition + glm::vec3(WIDTH / 2, HEIGHT - EYE_HEIGHT, WIDTH / 2);
}

glm::vec3 Player::forward() const {
    float yawRadians = glm::radians(yaw);
    float pitchRadians = glm::radians(pitch);
    return glm::vec3(std::cos(pitchRadians) * std::cos(yawRadians), std::sin(pitchRadians),
                     std::cos(pitchRadians) * std::sin(yawRadians));
}

void Player::setLook(float yawDegrees, float pitchDegrees) {
    yaw = yawDegrees;
    pitch = std::clamp(pitchDegrees, -MAX_PITCH, MAX_PITCH);
}

void Player::turn(float yawDegrees, float pitchDegrees) {
    yaw += yawDegrees;
    pitch = std::clamp(pitch + pitchDegrees, -MAX_PITCH, MAX_PITCH);
}

void Player::toggleFlying() {
    flying = !flying;
    verticalSpeed = 0.0f;
}

void Player::update(const MoveKeys& keys, float deltaTime, const IsSolid& isSolid) {
    // Horizontal direction from the keys, relative to where the player faces.
    float yawRadians = glm::radians(yaw);
    glm::vec3 flatForward(std::cos(yawRadians), 0.0f, std::sin(yawRadians));
    glm::vec3 right(-std::sin(yawRadians), 0.0f, std::cos(yawRadians));
    glm::vec3 direction(0.0f);
    if (keys.forward) direction += flatForward;
    if (keys.back) direction -= flatForward;
    if (keys.right) direction += right;
    if (keys.left) direction -= right;
    if (glm::dot(direction, direction) > 0.0f) direction = glm::normalize(direction);

    glm::vec3 delta;
    if (flying) {
        if (keys.up) direction.y += 1.0f;
        if (keys.down) direction.y -= 1.0f;
        delta = direction * FLY_SPEED * (keys.sprint ? 2.0f : 1.0f) * deltaTime;
    } else {
        onGround = standingOnSomething(isSolid);
        if (keys.up && onGround) verticalSpeed = JUMP_SPEED;
        verticalSpeed = std::max(verticalSpeed - GRAVITY * deltaTime, -TERMINAL_SPEED);
        delta = direction * (keys.sprint ? SPRINT_SPEED : WALK_SPEED) * deltaTime;
        delta.y = verticalSpeed * deltaTime;
    }
    move(delta, isSolid);
}

bool Player::standingOnSomething(const IsSolid& isSolid) const {
    constexpr float PROBE = 0.01f; // how far below the feet counts as "on"
    const glm::vec3 low = boxMin(eye);
    const glm::vec3 high = boxMax(eye);
    if (low.y >= 0.0f && low.y < PROBE) return true; // the ground

    int below = static_cast<int>(std::floor(low.y - PROBE));
    if (below >= static_cast<int>(std::floor(low.y))) return false; // feet are not near a block top
    for (int z = static_cast<int>(std::floor(low.z)); z < static_cast<int>(std::ceil(high.z));
         ++z) {
        for (int x = static_cast<int>(std::floor(low.x)); x < static_cast<int>(std::ceil(high.x));
             ++x) {
            if (isSolid(glm::ivec3(x, below, z))) return true;
        }
    }
    return false;
}

void Player::move(const glm::vec3& delta, const IsSolid& isSolid) {
    constexpr float GAP = 1e-3f; // stop this far short of a face, so the next step is not inside it
    for (int axis : {1, 0, 2}) {
        if (delta[axis] == 0.0f) continue;
        const bool positive = delta[axis] > 0;
        glm::vec3 next = eye;
        next[axis] += delta[axis];
        const glm::vec3 oldLow = boxMin(eye);
        const glm::vec3 oldHigh = boxMax(eye);
        const glm::vec3 newLow = boxMin(next);
        const glm::vec3 newHigh = boxMax(next);

        // Sweep every cell between the old and new box, so large steps (lag
        // frames, sprint flying) cannot tunnel through a wall. `stop` ends up at
        // the nearest blocking face along the movement.
        bool blocked = false;
        float stop =
            positive ? std::numeric_limits<float>::max() : std::numeric_limits<float>::lowest();
        const glm::ivec3 first = glm::ivec3(glm::floor(glm::min(newLow, oldLow)));
        const glm::ivec3 last = glm::ivec3(glm::ceil(glm::max(newHigh, oldHigh))) - 1;
        for (int z = first.z; z <= last.z; ++z) {
            for (int y = first.y; y <= last.y; ++y) {
                for (int x = first.x; x <= last.x; ++x) {
                    const glm::vec3 cell(x, y, z);
                    bool wasInside = boxOverlapsCell(oldLow, oldHigh, cell);
                    if (wasInside || !isSolid(glm::ivec3(x, y, z))) continue;
                    blocked = true;
                    stop =
                        positive ? std::min(stop, cell[axis]) : std::max(stop, cell[axis] + 1.0f);
                }
            }
        }
        // While walking, the y = 0 ground stops a fall.
        if (!flying && axis == 1 && newLow.y < 0.0f && oldLow.y >= 0.0f) {
            blocked = true;
            stop = std::max(stop, 0.0f);
        }
        if (blocked) {
            // Put the box against the blocking face.
            next[axis] = positive ? stop - (boxMax(eye)[axis] - eye[axis]) - GAP
                                  : stop + (eye[axis] - boxMin(eye)[axis]) + GAP;
            if (axis == 1) verticalSpeed = 0.0f;
        }
        eye = next;
    }
    onGround = !flying && standingOnSomething(isSolid);
}

bool Player::overlaps(const glm::ivec3& cell) const {
    return boxOverlapsCell(boxMin(eye), boxMax(eye), glm::vec3(cell));
}

CrosshairTarget Player::aim(const IsSolid& isSolid) const {
    // Grid traversal of Amanatides and Woo ("A Fast Voxel Traversal Algorithm",
    // 1987): step from cell to cell across whichever cell boundary the ray
    // reaches first. tMax[axis] is the ray distance to the next boundary on that
    // axis, tDelta[axis] the distance between boundaries.
    CrosshairTarget target;
    const glm::vec3 dir = forward();
    glm::ivec3 cell = glm::ivec3(glm::floor(eye));
    const glm::ivec3 step(dir.x > 0 ? 1 : -1, dir.y > 0 ? 1 : -1, dir.z > 0 ? 1 : -1);
    glm::vec3 tMax;
    glm::vec3 tDelta;
    for (int axis = 0; axis < 3; ++axis) {
        if (std::abs(dir[axis]) < 1e-8f) { // parallel to this axis: never crosses
            tMax[axis] = std::numeric_limits<float>::max();
            tDelta[axis] = std::numeric_limits<float>::max();
            continue;
        }
        float boundary = static_cast<float>(cell[axis] + (step[axis] > 0 ? 1 : 0));
        tMax[axis] = (boundary - eye[axis]) / dir[axis];
        tDelta[axis] = std::abs(1.0f / dir[axis]);
    }

    int lastAxis = -1; // axis of the boundary crossed to enter `cell`
    float distance = 0.0f;
    while (distance <= REACH) {
        if (isSolid(cell)) {
            target.hit = true;
            target.block = cell;
            if (lastAxis >= 0) {
                // The face we came through: place against it.
                target.normal = glm::ivec3(0);
                target.normal[lastAxis] = -step[lastAxis];
                target.place = cell + target.normal;
                target.canPlace = true;
            }
            return target;
        }
        lastAxis = smallestAxis(tMax); // the boundary the ray reaches first
        distance = tMax[lastAxis];
        cell[lastAxis] += step[lastAxis];
        tMax[lastAxis] += tDelta[lastAxis];
    }

    // No block within reach. Build on top of the y = 0 ground if it is in reach,
    // like Minecraft's surface.
    if (eye.y > 0.0f && dir.y < 0.0f && -eye.y / dir.y <= REACH) {
        glm::vec3 onGround = eye + dir * (-eye.y / dir.y);
        target.place = glm::ivec3(static_cast<int>(std::floor(onGround.x)), 0,
                                  static_cast<int>(std::floor(onGround.z)));
        target.normal = glm::ivec3(0, 1, 0);
        target.canPlace = true;
        return target;
    }

    // Otherwise into the air ahead, growing along the axis the view is closest to.
    glm::vec3 absDir = glm::abs(dir);
    int major = largestAxis(absDir);
    target.normal = glm::ivec3(0);
    target.normal[major] = step[major];
    target.place = glm::ivec3(glm::floor(eye + dir * AIR_PLACE_DISTANCE));
    target.canPlace = true;
    return target;
}

} // namespace gol3d
