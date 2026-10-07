// Player movement, collision and aiming. All of it is plain arithmetic on the
// player's box and the cell grid; the world is only seen through IsSolid.

#include "game/Player.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <optional>

namespace gol3d {
namespace {

// How far short of a face move() stops the box, so the next move does not start
// inside the cell it just touched.
constexpr float CONTACT_GAP = 1e-3f;

// How far below the feet standingOnSomething() looks for support. It is larger
// than CONTACT_GAP, so a player that move() has just set down on a block counts
// as standing on it.
constexpr float STANDING_PROBE = 0.01f;

// A ray whose direction along an axis is smaller than this never crosses that
// axis's cell boundaries.
constexpr float PARALLEL_EPSILON = 1e-8f;

constexpr int Y_AXIS = 1;

// An axis-aligned box from corner `low` to corner `high`.
struct Box {
    glm::vec3 low;
    glm::vec3 high;
};

// True when the box overlaps the unit cell whose minimum corner is `cell`.
bool boxOverlapsCell(const Box& box, const glm::vec3& cell) {
    const bool overlapsX = cell.x < box.high.x && cell.x + 1.0f > box.low.x;
    const bool overlapsY = cell.y < box.high.y && cell.y + 1.0f > box.low.y;
    const bool overlapsZ = cell.z < box.high.z && cell.z + 1.0f > box.low.z;
    return overlapsX && overlapsY && overlapsZ;
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

// Where along `axis` a box moving from `before` to `after` (one axis only) must
// stop: the nearest face of a solid cell in its way, or nothing. Every cell
// between the two boxes is checked, so a large step (a lag frame, sprint
// flying) cannot tunnel through a wall. Cells the box already overlapped are
// ignored, so a block born inside the player never traps it.
std::optional<float> nearestBlockingFace(const Box& before, const Box& after, int axis,
                                         bool movingPositive, const IsSolid& isSolid) {
    std::optional<float> nearest;
    const glm::ivec3 first = glm::ivec3(glm::floor(glm::min(after.low, before.low)));
    const glm::ivec3 last = glm::ivec3(glm::ceil(glm::max(after.high, before.high))) - 1;
    for (int z = first.z; z <= last.z; ++z) {
        for (int y = first.y; y <= last.y; ++y) {
            for (int x = first.x; x <= last.x; ++x) {
                const glm::vec3 cell(x, y, z);
                const bool wasInside = boxOverlapsCell(before, cell);
                if (wasInside || !isSolid(glm::ivec3(x, y, z))) continue;
                // The side of the cell the box runs into.
                const float face = movingPositive ? cell[axis] : cell[axis] + 1.0f;
                if (!nearest) {
                    nearest = face;
                } else {
                    nearest = movingPositive ? std::min(*nearest, face) : std::max(*nearest, face);
                }
            }
        }
    }
    return nearest;
}

// Walks the ray from `eye` along the unit vector `direction` through the grid,
// up to `reach`, using the voxel traversal of Amanatides and Woo ("A Fast Voxel
// Traversal Algorithm for Ray Tracing", 1987): step from cell to cell across
// whichever cell boundary the ray reaches first. Returns the first solid cell,
// with stamps placed against the face the ray entered it through.
std::optional<CrosshairTarget> findBlockAlongRay(const glm::vec3& eye, const glm::vec3& direction,
                                                 float reach, const IsSolid& isSolid) {
    glm::ivec3 cell = glm::ivec3(glm::floor(eye));
    const glm::ivec3 step(direction.x > 0 ? 1 : -1, direction.y > 0 ? 1 : -1,
                          direction.z > 0 ? 1 : -1);
    // tMax[axis]: ray distance to the next cell boundary on that axis.
    // tDelta[axis]: ray distance between two boundaries on that axis.
    glm::vec3 tMax;
    glm::vec3 tDelta;
    for (int axis = 0; axis < 3; ++axis) {
        if (std::abs(direction[axis]) < PARALLEL_EPSILON) {
            tMax[axis] = std::numeric_limits<float>::max();
            tDelta[axis] = std::numeric_limits<float>::max();
            continue;
        }
        const float boundary = static_cast<float>(cell[axis] + (step[axis] > 0 ? 1 : 0));
        tMax[axis] = (boundary - eye[axis]) / direction[axis];
        tDelta[axis] = std::abs(1.0f / direction[axis]);
    }

    int lastAxis = -1; // axis of the boundary crossed to enter `cell`; -1 in the eye's own cell
    float distance = 0.0f;
    while (distance <= reach) {
        if (isSolid(cell)) {
            CrosshairTarget target;
            target.hit = true;
            target.block = cell;
            // With the eye inside a block there is no face to place against.
            if (lastAxis >= 0) {
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
    return std::nullopt;
}

// Builds on top of the y = 0 ground where the ray meets it, like Minecraft's
// surface, if that is within `reach`.
std::optional<CrosshairTarget> placeOnGround(const glm::vec3& eye, const glm::vec3& direction,
                                             float reach) {
    const bool lookingDownFromAbove = eye.y > 0.0f && direction.y < 0.0f;
    if (!lookingDownFromAbove) return std::nullopt;
    const float distanceToGround = -eye.y / direction.y;
    if (distanceToGround > reach) return std::nullopt;

    const glm::vec3 groundPoint = eye + direction * distanceToGround;
    CrosshairTarget target;
    target.place = glm::ivec3(static_cast<int>(std::floor(groundPoint.x)), 0,
                              static_cast<int>(std::floor(groundPoint.z)));
    target.normal = glm::ivec3(0, 1, 0);
    target.canPlace = true;
    return target;
}

// Places into empty air `distance` ahead, growing along the axis the view is
// closest to.
CrosshairTarget placeInAir(const glm::vec3& eye, const glm::vec3& direction, float distance) {
    const int mainAxis = largestAxis(glm::abs(direction));
    CrosshairTarget target;
    target.normal = glm::ivec3(0);
    target.normal[mainAxis] = direction[mainAxis] > 0 ? 1 : -1;
    target.place = glm::ivec3(glm::floor(eye + direction * distance));
    target.canPlace = true;
    return target;
}

} // namespace

glm::vec3 Player::boxMin(const glm::vec3& eyePosition) {
    return eyePosition - glm::vec3(WIDTH / 2, EYE_HEIGHT, WIDTH / 2);
}

glm::vec3 Player::boxMax(const glm::vec3& eyePosition) {
    return eyePosition + glm::vec3(WIDTH / 2, HEIGHT - EYE_HEIGHT, WIDTH / 2);
}

glm::vec3 Player::forward() const {
    const float yawRadians = glm::radians(yaw);
    const float pitchRadians = glm::radians(pitch);
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
    const float yawRadians = glm::radians(yaw);
    const glm::vec3 flatForward(std::cos(yawRadians), 0.0f, std::sin(yawRadians));
    const glm::vec3 right(-std::sin(yawRadians), 0.0f, std::cos(yawRadians));
    glm::vec3 direction(0.0f);
    if (keys.forward) direction += flatForward;
    if (keys.back) direction -= flatForward;
    if (keys.right) direction += right;
    if (keys.left) direction -= right;
    // Walking diagonally is no faster than walking straight.
    if (glm::dot(direction, direction) > 0.0f) direction = glm::normalize(direction);

    glm::vec3 delta;
    if (flying) {
        if (keys.up) direction.y += 1.0f;
        if (keys.down) direction.y -= 1.0f;
        const float sprintFactor = keys.sprint ? FLY_SPRINT_FACTOR : 1.0f;
        delta = direction * FLY_SPEED * sprintFactor * deltaTime;
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
    const glm::vec3 low = boxMin(eye);
    const glm::vec3 high = boxMax(eye);
    if (low.y >= 0.0f && low.y < STANDING_PROBE) return true; // on the y = 0 ground

    // The feet rest on a block top only when probing a little lower crosses
    // into the row of cells below them.
    const int rowBelow = static_cast<int>(std::floor(low.y - STANDING_PROBE));
    const int feetRow = static_cast<int>(std::floor(low.y));
    if (rowBelow >= feetRow) return false;

    // Any solid cell in that row under the box's footprint holds the player up.
    const int firstX = static_cast<int>(std::floor(low.x));
    const int endX = static_cast<int>(std::ceil(high.x));
    const int firstZ = static_cast<int>(std::floor(low.z));
    const int endZ = static_cast<int>(std::ceil(high.z));
    for (int z = firstZ; z < endZ; ++z) {
        for (int x = firstX; x < endX; ++x) {
            if (isSolid(glm::ivec3(x, rowBelow, z))) return true;
        }
    }
    return false;
}

void Player::move(const glm::vec3& delta, const IsSolid& isSolid) {
    for (int axis : {1, 0, 2}) { // y first, like Minecraft
        if (delta[axis] == 0.0f) continue;
        const bool movingPositive = delta[axis] > 0;
        glm::vec3 next = eye;
        next[axis] += delta[axis];
        const Box before{boxMin(eye), boxMax(eye)};
        const Box after{boxMin(next), boxMax(next)};

        std::optional<float> stopAt =
            nearestBlockingFace(before, after, axis, movingPositive, isSolid);
        // While walking, the y = 0 ground stops a fall.
        const bool fallsThroughGround =
            !flying && axis == Y_AXIS && after.low.y < 0.0f && before.low.y >= 0.0f;
        if (fallsThroughGround) stopAt = stopAt ? std::max(*stopAt, 0.0f) : 0.0f;

        if (stopAt) {
            // Put the box's leading side just short of the blocking face.
            if (movingPositive) {
                const float eyeToLeadingSide = before.high[axis] - eye[axis];
                next[axis] = *stopAt - eyeToLeadingSide - CONTACT_GAP;
            } else {
                const float eyeToLeadingSide = eye[axis] - before.low[axis];
                next[axis] = *stopAt + eyeToLeadingSide + CONTACT_GAP;
            }
            if (axis == Y_AXIS) verticalSpeed = 0.0f;
        }
        eye = next;
    }
    onGround = !flying && standingOnSomething(isSolid);
}

bool Player::overlaps(const glm::ivec3& cell) const {
    return boxOverlapsCell(Box{boxMin(eye), boxMax(eye)}, glm::vec3(cell));
}

CrosshairTarget Player::aim(const IsSolid& isSolid) const {
    const glm::vec3 direction = forward();
    if (auto block = findBlockAlongRay(eye, direction, REACH, isSolid)) return *block;
    if (auto ground = placeOnGround(eye, direction, REACH)) return *ground;
    return placeInAir(eye, direction, AIR_PLACE_DISTANCE);
}

} // namespace gol3d
