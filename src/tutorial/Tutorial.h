#pragma once

// In-game tutorial: a lesson panel (Dear ImGui) over the running world. The game
// owns the world; the panel only reports what the player asked for, and the game
// loads the lesson's scene (see TutorialLessons.h) when asked.

#include <cstddef>
#include <cstdint>
#include <string>

#include "tutorial/TutorialLessons.h"

namespace gol3d::tutorial {

// Which lesson is open, and the panel that shows it. Methods that react to the
// player return a Request telling the game what to do next.
class Tutorial {
public:
    enum class Request {
        None,
        LoadScene, // load the current lesson's scene (a new lesson, or Replay)
        Close,     // the tutorial was closed
    };

    // What the panel shows about the running world.
    struct Status {
        std::string rule; // notation, e.g. S5-7/B6
        uint64_t generation = 0;
        uint64_t population = 0;
        bool running = false;
    };

    bool active() const { return isOpen_; }
    size_t lessonIndex() const { return index_; }
    const Lesson& lesson() const { return lessons()[index_]; }

    // Opens at a lesson (clamped to the valid range); the caller loads its scene.
    void open(size_t lesson);
    void close() { isOpen_ = false; }

    // Left/Right arrows change lessons, Backspace replays. Takes a GLFW key code.
    Request handleKey(int key);

    // Draws the panel at the right edge; `scale` is the GUI scale.
    Request draw(float scale, const Status& status);

    // Outline color of a mark, in linear RGB like shaders/life3d_boxes.vert.
    static glm::vec3 markColor(Mark mark);

private:
    // Switches to `lesson` if it exists.
    Request go(size_t lesson);
    // The Back, Replay and Next/Finish buttons.
    Request drawNavigation();

    bool isOpen_ = false;
    size_t index_ = 0;
};

} // namespace gol3d::tutorial
