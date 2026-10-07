// Keyboard and mouse. Controls follow Minecraft's defaults where Minecraft has
// one (WASD, Space, Shift, Ctrl, 1-9, F1/F2/F3/F11, Esc, Tab), with two
// deliberate changes: left click *places* and right click *removes*, and
// pressing the selected hotbar number again empties the hand. Simulation keys
// sit on keys Minecraft leaves unbound. printControls() lists them all.

#include <algorithm>
#include <cmath>
#include <iostream>

#include <GLFW/glfw3.h>

#include "game/Game.h"
#include "imgui.h"
#include "platform/Paths.h"

namespace gol3d {
namespace {

constexpr float DOUBLE_TAP_SECONDS = 0.3f;       // two Space presses this close toggle flying
constexpr float MOUSE_DEGREES_PER_PIXEL = 0.15f; // at 100% sensitivity
constexpr float BREAK_REPEAT_SECONDS = 0.25f;    // a held right button removes this often
constexpr float PLACE_REPEAT_SECONDS = 0.20f;    // a held left button places this often
constexpr double SLOT_NAME_SECONDS = 2.0; // the hotbar shows a new selection's name this long

// J and Shift+J fast-forward this many generations (the pause menu's Skip
// buttons match them).
constexpr uint64_t FAST_FORWARD_GENERATIONS = 100;
constexpr uint64_t LONG_FAST_FORWARD_GENERATIONS = 1000;
// Speed changes are counted in doublings: Shift makes one press 2^3 = 8x.
constexpr int SHIFT_SPEED_STEPS = 3;

constexpr int QUARTER_TURNS_PER_TURN = 4;
constexpr int DEGREES_PER_QUARTER_TURN = 90;

Game* gameOf(GLFWwindow* window) {
    return static_cast<Game*>(glfwGetWindowUserPointer(window));
}

// `value` wrapped into [0, count). Unlike `%` alone, it also wraps negative
// values, so stepping back from 0 lands on count - 1.
int wrapIndex(int value, int count) {
    return (value % count + count) % count;
}

// When the hotbar should stop showing the selection's name, if it changes now.
double slotNameDeadline() {
    return glfwGetTime() + SLOT_NAME_SECONDS;
}

// The extent of the stamp a scripted --place is about to put down.
void printStampBounds(const CellBounds& bounds, int rotation, int tilt) {
    std::cout << "stamp bounds (" << bounds.min.x << "," << bounds.min.y << "," << bounds.min.z
              << ")-(" << bounds.max.x << "," << bounds.max.y << "," << bounds.max.z
              << ") rotation " << rotation * DEGREES_PER_QUARTER_TURN << " tilt "
              << tilt * DEGREES_PER_QUARTER_TURN << std::endl;
}

} // namespace

// GLFW calls plain functions, so each callback finds the Game through the
// window's user pointer.
void Game::installInputCallbacks() {
    glfwSetWindowUserPointer(window_, this);
    glfwSetFramebufferSizeCallback(
        window_, [](GLFWwindow* window, int, int) { gameOf(window)->renderer_.onResize(); });
    glfwSetKeyCallback(window_, [](GLFWwindow* window, int key, int, int action, int mods) {
        gameOf(window)->onKey(key, action, mods);
    });
    glfwSetMouseButtonCallback(window_, [](GLFWwindow* window, int button, int action, int) {
        gameOf(window)->onMouseButton(button, action);
    });
    glfwSetCursorPosCallback(window_, [](GLFWwindow* window, double x, double y) {
        gameOf(window)->onCursorMove(x, y);
    });
    glfwSetScrollCallback(window_, [](GLFWwindow* window, double, double yOffset) {
        gameOf(window)->onScroll(yOffset);
    });
    glfwSetWindowFocusCallback(window_, [](GLFWwindow* window, int focused) {
        gameOf(window)->onFocusChange(focused == GLFW_TRUE);
    });
}

// ----------------------------------------------------------------- keyboard

// Every key event lands here. Ctrl+Q quits from anywhere; menus get only the
// keys that close them; the rest goes to onPlayingKey().
void Game::onKey(int key, int action, int mods) {
    const bool ctrl = (mods & GLFW_MOD_CONTROL) != 0;
    if (action == GLFW_PRESS && ctrl && key == GLFW_KEY_Q) {
        glfwSetWindowShouldClose(window_, GLFW_TRUE);
        return;
    }

    // In menus only Esc (and Tab in the inventory) do anything; ImGui handles the rest.
    if (screen_ != Screen::Playing) {
        if (action != GLFW_PRESS) return;
        const bool closesScreen =
            key == GLFW_KEY_ESCAPE || (key == GLFW_KEY_TAB && screen_ == Screen::Inventory);
        if (!closesScreen) return;
        switch (screen_) {
            case Screen::Paused:
                resumeGame();
                break;
            case Screen::Inventory:
                closeInventory();
                break;
            case Screen::Settings:
                saveSettingsIfPersistent();
                screen_ = Screen::Paused;
                break;
            case Screen::NewWorld:
                screen_ = Screen::Paused;
                break;
            case Screen::Playing:
                break;
        }
        return;
    }

    // F3 alone toggles the debug overlay when released; F3+G shows chunk borders.
    if (key == GLFW_KEY_F3) {
        if (action == GLFW_PRESS) f3UsedInCombo_ = false;
        if (action == GLFW_RELEASE && !f3UsedInCombo_) showDebug_ = !showDebug_;
        return;
    }
    // Holding N keeps stepping, at the keyboard's repeat rate.
    if (action == GLFW_REPEAT && key == GLFW_KEY_N && !ctrl) {
        stepOnce();
        return;
    }
    if (action == GLFW_PRESS) onPlayingKey(key, mods);
}

// A key press while playing.
void Game::onPlayingKey(int key, int mods) {
    const bool ctrl = (mods & GLFW_MOD_CONTROL) != 0;
    const bool shift = (mods & GLFW_MOD_SHIFT) != 0;

    // An open lesson panel takes the arrow keys and Backspace.
    tutorial::Tutorial::Request request = tutorial_.handleKey(key);
    if (request != tutorial::Tutorial::Request::None) {
        handleTutorialRequest(request);
        return;
    }
    // GLFW's number keys are consecutive, so 1..STAMP_COUNT map straight to slots.
    if (key >= GLFW_KEY_1 && key < GLFW_KEY_1 + STAMP_COUNT) {
        const int slot = key - GLFW_KEY_1;
        // Pressing the selected number again empties the hand.
        selectSlot(slot == brush_.hotbarSlot ? Brush::EMPTY_HAND : slot);
        return;
    }

    switch (key) {
        // Menus.
        case GLFW_KEY_ESCAPE:
            openPauseMenu();
            break;
        case GLFW_KEY_TAB:
            openInventory();
            break;

        // The brush.
        case GLFW_KEY_Q: // Ctrl+Q (quit) was handled in onKey
            rotateBrush(-1);
            break;
        case GLFW_KEY_E:
            rotateBrush(1);
            break;
        case GLFW_KEY_Z:
            tiltBrush(-1);
            break;
        case GLFW_KEY_C:
            tiltBrush(1);
            break;
        case GLFW_KEY_M:
            cycleMaterial(shift ? -1 : 1);
            break;

        // Movement.
        case GLFW_KEY_SPACE: {
            const double now = glfwGetTime();
            if (now - lastSpacePress_ < DOUBLE_TAP_SECONDS) {
                player_.toggleFlying();
                lastSpacePress_ = -1.0; // a third press starts a new double tap
            } else {
                lastSpacePress_ = now;
            }
            break;
        }

        // The world.
        case GLFW_KEY_S:
            if (ctrl) saveWorldWithMessage();
            break;
        case GLFW_KEY_O:
            if (ctrl) loadWorldWithMessage();
            break;
        case GLFW_KEY_N:
            if (ctrl) {
                newWorld(/*seeded=*/!shift);
                notify(shift ? "New empty world" : std::string("New world: ") + rule().name);
            } else {
                stepOnce();
            }
            break;
        case GLFW_KEY_R: {
            // Next rule, or the previous one with Shift; both wrap around the list.
            const size_t count = lifeRules().size();
            if (shift) {
                ruleIndex_ = (ruleIndex_ + count - 1) % count;
            } else {
                ruleIndex_ = (ruleIndex_ + 1) % count;
            }
            announceRule();
            break;
        }

        // The simulation.
        case GLFW_KEY_G:
            if (glfwGetKey(window_, GLFW_KEY_F3) == GLFW_PRESS) {
                showChunkBorders_ = !showChunkBorders_; // Minecraft's F3+G
                f3UsedInCombo_ = true;
            } else {
                setRunning(!running_);
            }
            break;
        case GLFW_KEY_J:
            // While a fast-forward runs, J cancels it instead (queueFastForward()).
            queueFastForward(shift ? LONG_FAST_FORWARD_GENERATIONS : FAST_FORWARD_GENERATIONS);
            break;
        // Speed doubles or halves per press; Shift makes it 8x.
        case GLFW_KEY_EQUAL:
        case GLFW_KEY_KP_ADD:
        case GLFW_KEY_RIGHT_BRACKET:
            changeSpeed(shift ? SHIFT_SPEED_STEPS : 1);
            break;
        case GLFW_KEY_MINUS:
        case GLFW_KEY_KP_SUBTRACT:
        case GLFW_KEY_LEFT_BRACKET:
            changeSpeed(shift ? -SHIFT_SPEED_STEPS : -1);
            break;

        // Display.
        case GLFW_KEY_F1:
            hudVisible_ = !hudVisible_;
            break;
        case GLFW_KEY_F2:
            renderer_.requestScreenshot(newScreenshotPath());
            break;
        case GLFW_KEY_F11:
            toggleFullscreen();
            break;
        case GLFW_KEY_H:
            printControls();
            break;
        default:
            break;
    }
}

// -------------------------------------------------------------------- mouse

void Game::onMouseButton(int button, int action) {
    if (screen_ != Screen::Playing) return; // menus handle their own clicks
    if (!cursorCaptured_) {
        // The first click only grabs the mouse, unless it lands on the tutorial panel.
        const bool overPanel = imguiReady_ && ImGui::GetIO().WantCaptureMouse;
        if (action == GLFW_PRESS && !overPanel) setCursorCaptured(true);
        return;
    }
    // A press acts at once and starts the repeat timer (updateHeldButtons()).
    if (button == GLFW_MOUSE_BUTTON_LEFT) {
        placeButton_.held = action == GLFW_PRESS;
        placeButton_.sinceLastRepeat = 0.0f;
        if (placeButton_.held) placeStamp();
    } else if (button == GLFW_MOUSE_BUTTON_RIGHT) {
        breakButton_.held = action == GLFW_PRESS;
        breakButton_.sinceLastRepeat = 0.0f;
        if (breakButton_.held) breakBlock();
    }
}

// GLFW reports absolute cursor positions (unbounded while the cursor is
// captured); the view turns by the change since the last report.
void Game::onCursorMove(double x, double y) {
    if (screen_ == Screen::Playing && cursorCaptured_ && haveCursorPosition_) {
        // settings_.sensitivity is a percentage.
        const float degreesPerPixel =
            MOUSE_DEGREES_PER_PIXEL * static_cast<float>(settings_.sensitivity) / 100.0f;
        const float verticalSign = settings_.invertY ? -1.0f : 1.0f;
        const float yawChange = static_cast<float>(x - lastCursorX_) * degreesPerPixel;
        const float pitchChange =
            static_cast<float>(y - lastCursorY_) * degreesPerPixel * verticalSign;
        player_.turn(yawChange, -pitchChange); // screen y grows downward
    }
    lastCursorX_ = x;
    lastCursorY_ = y;
    haveCursorPosition_ = true;
}

// Like Minecraft: scrolling down selects the next slot. From an empty hand,
// scrolling picks the first or last slot.
void Game::onScroll(double yOffset) {
    if (screen_ != Screen::Playing || yOffset == 0.0) return;
    const bool down = yOffset < 0;
    int next = 0;
    if (brush_.emptyHand()) {
        next = down ? 0 : STAMP_COUNT - 1;
    } else {
        next = brush_.hotbarSlot + (down ? 1 : -1);
    }
    selectSlot(wrapIndex(next, STAMP_COUNT));
}

void Game::onFocusChange(bool focused) {
    // Like Minecraft, losing focus mid-game opens the pause menu.
    if (!focused && screen_ == Screen::Playing && cursorCaptured_) openPauseMenu();
}

void Game::setCursorCaptured(bool captured) {
    cursorCaptured_ = captured;
    haveCursorPosition_ = false; // the first move after capturing must not jerk the view
    glfwSetInputMode(window_, GLFW_CURSOR, captured ? GLFW_CURSOR_DISABLED : GLFW_CURSOR_NORMAL);
    if (!captured) {
        placeButton_.held = false;
        breakButton_.held = false;
    }
}

// ----------------------------------------------------------- per frame

// Movement keys only count while the mouse is captured; gravity always applies.
void Game::updateMovement(float deltaTime) {
    auto down = [&](int key) { return cursorCaptured_ && glfwGetKey(window_, key) == GLFW_PRESS; };
    MoveKeys keys;
    keys.forward = down(GLFW_KEY_W);
    keys.back = down(GLFW_KEY_S);
    keys.right = down(GLFW_KEY_D);
    keys.left = down(GLFW_KEY_A);
    keys.up = down(GLFW_KEY_SPACE);
    keys.down = down(GLFW_KEY_LEFT_SHIFT);
    keys.sprint = down(GLFW_KEY_LEFT_CONTROL);
    player_.update(keys, deltaTime, solidCells());
}

// A held mouse button repeats its action at a fixed interval, like Minecraft's.
void Game::updateHeldButtons(float deltaTime) {
    // True when `button` is held and its interval has passed; restarts the interval.
    auto repeatDue = [deltaTime](HeldButton& button, float intervalSeconds) {
        if (!button.held) return false;
        button.sinceLastRepeat += deltaTime;
        if (button.sinceLastRepeat < intervalSeconds) return false;
        button.sinceLastRepeat = 0.0f;
        return true;
    };
    if (repeatDue(breakButton_, BREAK_REPEAT_SECONDS)) breakBlock();
    if (repeatDue(placeButton_, PLACE_REPEAT_SECONDS)) placeStamp();
}

// ------------------------------------------------------------ the brush

// The current brush placed at the crosshair. `solid` fills soups completely
// (for outlines and bounds) instead of drawing them from the RNG.
StampPlacement Game::placementAtTarget(bool solid) const {
    StampPlacement placement;
    placement.stamp = static_cast<Stamp>(brush_.hotbarSlot);
    placement.anchor = target_.place;
    placement.normal = target_.normal;
    placement.rotation = brush_.rotation;
    placement.tilt = brush_.tilt;
    placement.viewDirection = player_.forward();
    placement.solid = solid;
    return placement;
}

// Stamps are made of the selected material and only fill empty cells, never
// the cells the player stands in.
void Game::placeStamp() {
    if (!target_.canPlace || brush_.emptyHand()) return;
    for (const glm::ivec3& cell : stampCells(placementAtTarget(/*solid=*/false), rule(), rng_)) {
        if (!player_.overlaps(cell)) placeCell(cell, brush_.material);
    }
}

void Game::breakBlock() {
    if (target_.hit) clearCell(target_.block);
}

void Game::selectSlot(int hotbarSlot) {
    brush_.hotbarSlot = hotbarSlot;
    slotNameUntil_ = slotNameDeadline();
}

IsSolid Game::solidCells() const {
    return [this](const glm::ivec3& cell) { return world_.isOccupied(cell); };
}

// A seed for the New World screen, kept positive so it reads well in the field:
// the mask clears the sign bit of the 32-bit draw.
int Game::newRandomSeed() {
    return static_cast<int>(rng_() & 0x7FFFFFFF);
}

// Rotating, tilting and changing material show the hotbar name again, which
// carries the brush's orientation and material.
void Game::rotateBrush(int quarterTurns) {
    brush_.rotation = wrapIndex(brush_.rotation + quarterTurns, QUARTER_TURNS_PER_TURN);
    slotNameUntil_ = slotNameDeadline();
}

void Game::tiltBrush(int quarterTurns) {
    brush_.tilt = wrapIndex(brush_.tilt + quarterTurns, QUARTER_TURNS_PER_TURN);
    slotNameUntil_ = slotNameDeadline();
}

// CellKind values are the indices of cellTypes(), so stepping the index steps the material.
void Game::cycleMaterial(int direction) {
    const int count = static_cast<int>(cellTypes().size());
    const int next = wrapIndex(static_cast<int>(brush_.material) + direction, count);
    brush_.material = static_cast<CellKind>(next);
    notify(std::string("Building with ") + cellType(brush_.material).name);
    slotNameUntil_ = slotNameDeadline();
}

// ------------------------------------------------------------- screens

// Fullscreen takes the primary monitor at its current video mode; leaving it
// restores the window's last position and size.
void Game::toggleFullscreen() {
    fullscreen_ = !fullscreen_;
    if (fullscreen_) {
        glfwGetWindowPos(window_, &windowedX_, &windowedY_);
        glfwGetWindowSize(window_, &windowedWidth_, &windowedHeight_);
        GLFWmonitor* monitor = glfwGetPrimaryMonitor();
        const GLFWvidmode* mode = glfwGetVideoMode(monitor);
        glfwSetWindowMonitor(window_, monitor, 0, 0, mode->width, mode->height, mode->refreshRate);
    } else {
        glfwSetWindowMonitor(window_, nullptr, windowedX_, windowedY_, windowedWidth_,
                             windowedHeight_, 0);
    }
    renderer_.onResize();
}

// The pause menu stops the simulation and remembers whether it ran, for resumeGame().
void Game::openPauseMenu() {
    if (screen_ != Screen::Playing) return;
    runningBeforePause_ = running_;
    running_ = false;
    screen_ = Screen::Paused;
    setCursorCaptured(false);
}

void Game::resumeGame() {
    screen_ = Screen::Playing;
    setRunning(runningBeforePause_);
    setCursorCaptured(true);
}

void Game::openInventory() {
    screen_ = Screen::Inventory;
    setCursorCaptured(false);
}

void Game::closeInventory() {
    screen_ = Screen::Playing;
    setCursorCaptured(true);
}

void Game::openNewWorldScreen() {
    newWorldForm_.rule = static_cast<int>(ruleIndex_);
    newWorldForm_.seed = newRandomSeed();
    newWorldForm_.empty = false;
    screen_ = Screen::NewWorld;
}

bool Game::worldFrozen() const {
    return screen_ == Screen::Paused || screen_ == Screen::Settings || screen_ == Screen::NewWorld;
}

// ------------------------------------------------------- scripted input

// One scripted-input option from the command line (--pos, --look, --place...).
// They drive the game like a player would, in order, before the frame loop
// starts; some print what happened so a test run can check it.
void Game::applyScriptAction(const ScriptAction& action) {
    const IsSolid isSolid = solidCells();
    switch (action.kind) {
        case ScriptAction::Position:
            player_.eye = action.vector;
            break;
        case ScriptAction::Look:
            player_.setLook(action.vector.x, action.vector.y);
            break;
        case ScriptAction::Slot:
            brush_.hotbarSlot = std::clamp(action.number - 1, Brush::EMPTY_HAND, STAMP_COUNT - 1);
            break;
        case ScriptAction::Rotate:
            rotateBrush(action.number);
            break;
        case ScriptAction::Tilt:
            tiltBrush(action.number);
            break;
        case ScriptAction::Material:
            brush_.material = static_cast<CellKind>(action.number);
            break;
        case ScriptAction::Push: {
            player_.move(action.vector, isSolid);
            const glm::vec3 feet = player_.feet();
            std::cout << "push: feet at " << feet.x << " " << feet.y << " " << feet.z << std::endl;
            break;
        }
        case ScriptAction::Resize:
            glfwSetWindowSize(window_, static_cast<int>(action.vector.x),
                              static_cast<int>(action.vector.y));
            break;
        case ScriptAction::Place:
        case ScriptAction::Break: {
            const bool place = action.kind == ScriptAction::Place;
            // Scripts run before the frame loop, which would otherwise update the aim.
            target_ = player_.aim(isSolid);
            const uint64_t before = population_;
            if (place && !brush_.emptyHand() && target_.canPlace) {
                // A solid placement draws nothing from rng_, so the real one below is unaffected.
                const CellBounds bounds =
                    boundsOf(stampCells(placementAtTarget(/*solid=*/true), rule(), rng_));
                printStampBounds(bounds, brush_.rotation, brush_.tilt);
            }
            if (place) {
                placeStamp();
            } else {
                breakBlock();
            }
            if (refreshPending_) rebuild(Rebuild::Edit);
            std::cout << (place ? "place " : "break ") << handName()
                      << (target_.hit ? " at block face" : " in air") << ": population " << before
                      << " -> " << population_ << std::endl;
            break;
        }
    }
}

void Game::printControls() const {
    std::cout
        << "\n3D Game of Life - Minecraft-style controls\n"
           "  Mouse  look (click the window to grab the mouse)   Esc  pause menu and settings\n"
           "  W A S D  move   Space  jump (fly up)   Left Shift  fly down   Left Ctrl  sprint\n"
           "  Double-tap Space  toggle flying\n"
           "  Left click  place the selected stamp   Right click  remove the outlined block\n"
           "  Tab  stamps and rules   Q / E  rotate the stamp   Z / C  tilt it around x   1-"
        << STAMP_COUNT
        << " / scroll  hotbar (press the selected number again for an empty hand):\n ";
    for (int i = 0; i < STAMP_COUNT; ++i) {
        std::cout << "  " << (i + 1) << " " << stampInfos()[i].name;
    }
    std::cout
        << "\n"
           "  M / Shift+M  building material: Life, Stone (inert wall), Ember (permanent live "
           "neighbor)\n"
           "  G  run/pause generations   N  single generation\n"
           "  [ ]  half / double speed ("
        << SimulationSpeed::formatRate(std::ldexp(1.0, SimulationSpeed::MIN_EXPONENT)) << " to "
        << SimulationSpeed::formatRate(std::ldexp(1.0, SimulationSpeed::MAX_EXPONENT))
        << " gen/s, then max; Shift: 8x)\n"
           "  J / Shift+J  fast-forward 100 / 1000 generations (J again cancels)\n"
           "  R / Shift+R  next/previous rule   Ctrl+N  new world   Ctrl+Shift+N  empty world\n"
           "  Ctrl+S  save the world   Ctrl+O  load it (user data folder)\n"
           "  F1  hide HUD   F2  screenshot   F3  debug info   F3+G  chunk borders   F11  "
           "fullscreen\n"
           "  H  help   Ctrl+Q  quit\n"
           "Rules:\n";
    for (const LifeRule& listed : lifeRules()) {
        std::cout << "  " << listed.name << "  " << describeRule(listed) << "\n";
    }
    std::cout << std::endl;
}

} // namespace gol3d
