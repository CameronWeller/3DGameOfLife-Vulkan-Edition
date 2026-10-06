// Checks for src/game/Player.h: walking, jumping, flying, collision with blocks
// and the ground, and where the crosshair aims. The world is a std::set of
// solid cells.

#include <cmath>
#include <set>
#include <tuple>

#include "TestSupport.h"
#include "game/Player.h"

using namespace gol3d;
using testing::expect;

namespace {

using CellSet = std::set<std::tuple<int, int, int>>;

IsSolid solidCells(const CellSet& cells) {
    return [&cells](const glm::ivec3& c) { return cells.count({c.x, c.y, c.z}) > 0; };
}

bool near(float a, float b, float tolerance = 0.01f) {
    return std::abs(a - b) <= tolerance;
}

// Runs `frames` frames of 1/60 s with the given keys.
void simulate(Player& player, const MoveKeys& keys, int frames, const IsSolid& isSolid) {
    for (int i = 0; i < frames; ++i) {
        player.update(keys, 1.0f / 60.0f, isSolid);
    }
}

void checkStandingAndFalling() {
    CellSet empty;
    Player player;
    player.eye = glm::vec3(0.5f, Player::EYE_HEIGHT, 0.5f); // feet on the y = 0 ground
    simulate(player, MoveKeys{}, 30, solidCells(empty));
    expect(near(player.feet().y, 0.0f), "a walking player stands on the ground");
    expect(player.onGround, "a player on the ground knows it");

    // Drop from 10 blocks up onto a block at (0, 0, 0).
    CellSet block = {{0, 0, 0}};
    player.eye = glm::vec3(0.5f, 10.0f + Player::EYE_HEIGHT, 0.5f);
    simulate(player, MoveKeys{}, 120, solidCells(block));
    expect(near(player.feet().y, 1.0f), "a falling player lands on top of a block");
    expect(player.onGround && player.verticalSpeed == 0.0f, "landing stops the fall");

    // Jumping leaves the ground and comes back down.
    MoveKeys jump;
    jump.up = true;
    player.update(jump, 1.0f / 60.0f, solidCells(block));
    expect(player.feet().y > 1.0f && player.verticalSpeed > 0.0f, "Space jumps");
    simulate(player, MoveKeys{}, 120, solidCells(block));
    expect(near(player.feet().y, 1.0f), "a jump lands where it started");
}

void checkWallsAndTunneling() {
    // A wall two blocks tall at x = 3.
    CellSet wall;
    for (int z = -3; z <= 3; ++z) {
        for (int y = 0; y < 2; ++y) {
            wall.insert({3, y, z});
        }
    }
    Player player;
    player.eye = glm::vec3(0.5f, Player::EYE_HEIGHT, 0.5f);
    player.move(glm::vec3(10.0f, 0.0f, 0.0f), solidCells(wall));
    float front = player.eye.x + Player::WIDTH / 2;
    expect(front <= 3.0f && front > 2.99f, "walking stops against a wall");

    // One huge step (a lag spike) must not pass through the wall either.
    player.eye = glm::vec3(0.5f, Player::EYE_HEIGHT, 0.5f);
    player.move(glm::vec3(1000.0f, 0.0f, 0.0f), solidCells(wall));
    expect(player.eye.x < 3.0f, "large steps do not tunnel through walls");

    // A block born inside the player never traps it.
    CellSet inside = {{0, 0, 0}, {0, 1, 0}};
    player.eye = glm::vec3(0.5f, Player::EYE_HEIGHT, 0.5f);
    player.move(glm::vec3(2.0f, 0.0f, 0.0f), solidCells(inside));
    expect(near(player.eye.x, 2.5f), "the player walks out of a block that appeared inside it");
}

void checkFlying() {
    CellSet empty;
    Player player;
    player.eye = glm::vec3(0.5f, 5.0f, 0.5f);
    player.toggleFlying();
    expect(player.flying, "double-tap Space starts flying");
    simulate(player, MoveKeys{}, 60, solidCells(empty));
    expect(near(player.eye.y, 5.0f), "a flying player does not fall");
    player.move(glm::vec3(0.0f, -10.0f, 0.0f), solidCells(empty));
    expect(player.eye.y < 0.0f, "flying passes below the ground plane");

    MoveKeys forward;
    forward.forward = true;
    player.eye = glm::vec3(0.0f);
    player.setLook(0.0f, 0.0f); // facing +x
    simulate(player, forward, 60, solidCells(empty));
    expect(near(player.eye.x, Player::FLY_SPEED, 0.05f),
           "flight covers FLY_SPEED blocks per second");
}

void checkAim() {
    // A block straight ahead, two blocks away.
    CellSet block = {{3, 5, 0}};
    Player player;
    player.eye = glm::vec3(0.5f, 5.5f, 0.5f);
    player.setLook(0.0f, 0.0f); // facing +x
    CrosshairTarget target = player.aim(solidCells(block));
    expect(target.hit && target.block == glm::ivec3(3, 5, 0), "the crosshair hits the block ahead");
    expect(target.canPlace && target.normal == glm::ivec3(-1, 0, 0) &&
               target.place == glm::ivec3(2, 5, 0),
           "stamps go against the face that was hit");

    // Nothing in reach, looking down at the ground: build on it.
    CellSet empty;
    player.eye = glm::vec3(0.5f, 3.0f, 0.5f);
    player.setLook(0.0f, -60.0f);
    target = player.aim(solidCells(empty));
    expect(!target.hit && target.canPlace && target.place.y == 0 &&
               target.normal == glm::ivec3(0, 1, 0),
           "looking at the ground places on it");

    // Looking at the sky: place into the air ahead, growing along the view's main axis.
    player.setLook(90.0f, 10.0f); // mostly +z
    target = player.aim(solidCells(empty));
    expect(!target.hit && target.canPlace && target.normal == glm::ivec3(0, 0, 1),
           "stamps placed in the air grow along the view's major axis");
}

void checkLookAndOverlap() {
    Player player;
    player.setLook(10.0f, 120.0f);
    expect(player.pitch == Player::MAX_PITCH, "pitch is clamped short of straight up");
    player.turn(5.0f, -500.0f);
    expect(player.yaw == 15.0f && player.pitch == -Player::MAX_PITCH,
           "turning adds yaw and clamps pitch");

    player.eye = glm::vec3(0.5f, Player::EYE_HEIGHT, 0.5f);
    expect(player.overlaps({0, 0, 0}) && player.overlaps({0, 1, 0}),
           "the player's body fills two cells");
    expect(!player.overlaps({0, 2, 0}) && !player.overlaps({1, 0, 0}), "and no more");
}

} // namespace

int main() {
    checkStandingAndFalling();
    checkWallsAndTunneling();
    checkFlying();
    checkAim();
    checkLookAndOverlap();
    return testing::finish("player");
}
