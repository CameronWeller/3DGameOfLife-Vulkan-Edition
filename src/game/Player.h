#pragma once

// The player: a camera with Minecraft's body and movement. One cell is one
// block; speeds are Minecraft's, in blocks per second. Walking players fall,
// jump and collide with blocks and the y = 0 ground; flying players (double-tap
// Space, as in creative mode) move freely but still collide with blocks.
//
// The player knows nothing about chunks: everything that touches the world goes
// through an IsSolid callback, which also lets the tests run it against a
// handful of blocks in a std::set.

#include <functional>

#include <glm/glm.hpp>

namespace gol3d {

// True when the block at a cell blocks movement (any live cell or block).
using IsSolid = std::function<bool(const glm::ivec3& cell)>;

// What the crosshair points at.
struct CrosshairTarget {
    bool hit = false;           // a block is within reach
    glm::ivec3 block{0};        // that block (removed by right click)
    bool canPlace = false;      // a stamp can be placed
    glm::ivec3 place{0};        // where the stamp's anchor goes
    glm::ivec3 normal{0, 1, 0}; // direction the stamp grows into
};

// Movement keys held this frame.
struct MoveKeys {
    bool forward = false, back = false, left = false, right = false;
    bool up = false;   // Space: jump, or rise while flying
    bool down = false; // Left Shift: sink while flying
    bool sprint = false;
};

class Player {
public:
    // Minecraft's player box: 0.6 wide, 1.8 tall, eyes 1.62 above the feet.
    static constexpr float WIDTH = 0.6f;
    static constexpr float HEIGHT = 1.8f;
    static constexpr float EYE_HEIGHT = 1.62f;

    static constexpr float WALK_SPEED = 4.317f;
    static constexpr float SPRINT_SPEED = 5.612f;
    static constexpr float FLY_SPEED = 10.92f; // sprinting doubles it
    static constexpr float GRAVITY = 32.0f;
    static constexpr float JUMP_SPEED = 8.9f; // clears a 1.25-block jump
    static constexpr float TERMINAL_SPEED = 78.4f;

    // The farthest block the crosshair can target, and how far ahead stamps go
    // when placed into empty air.
    static constexpr float REACH = 6.0f;
    static constexpr float AIR_PLACE_DISTANCE = 4.0f;
    static constexpr float MAX_PITCH = 89.9f;

    glm::vec3 eye{0.0f};
    float yaw = 0.0f;   // degrees; 0 looks along +x, 90 along +z
    float pitch = 0.0f; // degrees; positive looks up
    bool flying = false;
    bool onGround = false;
    float verticalSpeed = 0.0f; // blocks per second, while walking

    // Unit vector the camera looks along.
    glm::vec3 forward() const;
    glm::vec3 feet() const { return eye - glm::vec3(0.0f, EYE_HEIGHT, 0.0f); }

    void setLook(float yawDegrees, float pitchDegrees);
    void turn(float yawDegrees, float pitchDegrees);
    void toggleFlying();

    // One frame of movement: keys, gravity and collision.
    void update(const MoveKeys& keys, float deltaTime, const IsSolid& isSolid);

    // Moves by `delta`, one axis at a time (y first, like Minecraft), stopping
    // against blocks the player was not already inside, so a block born inside
    // the player never traps it. While walking, the y = 0 ground is solid.
    void move(const glm::vec3& delta, const IsSolid& isSolid);

    // True when a block, or the ground while walking, is directly under the feet.
    bool standingOnSomething(const IsSolid& isSolid) const;

    // True when the player's box overlaps the cell.
    bool overlaps(const glm::ivec3& cell) const;

    // Walks the crosshair ray through the grid up to REACH. Stamps go against the
    // face that was hit, else on the y = 0 ground within reach, else into empty
    // air a few blocks ahead.
    CrosshairTarget aim(const IsSolid& isSolid) const;

private:
    // Corners of the player's box with the eyes at `eyePosition`.
    static glm::vec3 boxMin(const glm::vec3& eyePosition);
    static glm::vec3 boxMax(const glm::vec3& eyePosition);
};

} // namespace gol3d
