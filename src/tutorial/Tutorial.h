#pragma once

// In-game tutorial: a lesson panel (Dear ImGui) over the running world. The game
// owns the world; the panel only reports what the player asked for, and the game
// loads the lesson's scene (see TutorialLessons.h) when asked.

#include <cstddef>
#include <cstdint>
#include <string>

#include "TutorialLessons.h"

namespace VulkanHIP::tutorial {

class Tutorial {
public:
    enum class Request { None, LoadScene, Close };

    // What the panel shows about the running world.
    struct Status {
        std::string rule;  // notation, e.g. S5-7/B6
        uint64_t generation = 0;
        uint64_t population = 0;
        bool running = false;
    };

    bool active() const { return isOpen; }
    size_t lessonIndex() const { return index; }
    const Lesson& lesson() const { return lessons()[index]; }

    // Opens at a lesson (clamped to the valid range); the caller loads its scene.
    void open(size_t lesson);
    void close() { isOpen = false; }

    // Left/Right arrows change lessons, Backspace replays. Takes a GLFW key code.
    Request handleKey(int key);

    // Draws the panel at the right edge; `scale` is the GUI scale.
    Request draw(float scale, const Status& status);

    // Outline color of a mark, in linear RGB like shaders/life3d_world.vert.
    static void markColor(Mark mark, float rgb[3]);

private:
    Request go(size_t lesson);

    bool isOpen = false;
    size_t index = 0;
};

} // namespace VulkanHIP::tutorial
