# Playing 3D Life

The repository builds `gol3d`, a Minecraft-style 3D Game of Life. You walk or fly
through an unbounded world, break and place live blocks, and watch the automaton
evolve.
It needs only Vulkan, GLFW and GLM. CMake uses installed GLFW and GLM when it
finds them and otherwise downloads pinned releases, so no vcpkg setup is needed.

```bash
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build
./build/gol3d
```

Requirements: a Vulkan 1.3 GPU driver (with compute subgroup operations, which
every current desktop driver has), `glslc` (from shaderc or the Vulkan SDK), and
on Linux the X11 or Wayland development headers that GLFW needs.

## Controls

The mapping follows Minecraft's defaults, except that left click places and right
click removes. Simulation actions use keys Minecraft leaves unbound.

| Input | Action |
| -- | -- |
| Mouse | Look around. Click the window to grab the mouse. |
| Esc | Pause menu (world freezes): Resume, run/pause, speed and fast-forward buttons, Stamps & Rules, Tutorial, New World, Save/Load, Settings, Quit |
| Tab | Stamps & Rules screen: pick a stamp or the empty hand, a material, and switch rules (hover for descriptions) |
| M, Shift+M | Next / previous material for stamps: Life, Stone, Ember |
| Q / E | Rotate the selected stamp a quarter turn around the surface you place on |
| Z / C | Tilt the selected stamp a quarter turn around the world x axis (Z climbs, C dives for a glider) |
| W A S D | Move |
| Space | Jump; fly up while flying |
| Double-tap Space | Toggle flying (as in creative mode) |
| Left Shift | Fly down |
| Left Ctrl | Sprint |
| Left click | Place the selected hotbar stamp (hold to repeat) |
| Right click | Remove the outlined block (hold to repeat) |
| 1-9, scroll wheel | Select a hotbar stamp. Pressing the selected number again empties your hand: nothing is placed and no placement outline is shown. |
| G | Run or pause generations |
| N | Advance one generation (hold to repeat) |
| [ and ] (or - and +) | Half / double speed: 1/32 to 8192 generations per second, then max (starts at 1). Shift: 8x per press |
| J, Shift+J | Fast-forward 100 / 1000 generations at full speed, then return to the set speed. J again cancels. |
| R, Shift+R | Next / previous rule. Cells are kept. |
| Ctrl+N, Ctrl+Shift+N | New world seeded with the rule's soup / empty world |
| Ctrl+S, Ctrl+O | Save / load `world.life3d` in the user data folder |
| F1 | Hide the crosshair, hotbar and outlines |
| F2 | Screenshot (`screenshots/life3d-<time>.png` in the user data folder) |
| F3 | Debug overlay: position, chunk, facing, chunks in use and allocated, GPU memory, blocks drawn, GPU time per generation and per block list, fps |
| F3+G | Show chunk borders |
| F11 | Fullscreen |
| H | Print the controls |
| Ctrl+Q | Quit |

The top-left panel shows the rule and generation, whether the world is running,
the speed (and, when the world is too big for it, the speed actually reached),
the population with a graph of its recent history, and the building material.
Selecting a stamp shows its name above the hotbar. Messages such as saves and
rule changes appear mid-screen. The window title carries the same
status.

**Settings** (Esc, then Settings):
- field of view
- render distance (fog)
- mouse sensitivity and invert mouse
- smooth lighting (ambient occlusion) and animated births and deaths
- GUI scale (Auto scales with the window height, or 1 to 4)
- simulation speed, and time per frame: the most milliseconds of simulation a
  frame may use (default 8; fast-forward and max speed use at least 25)
- HUD, chunk borders and fullscreen

They save to `~/.config/gol3d/options.txt` (`%APPDATA%\gol3d` on Windows) and
load on start. Scripted and test runs ignore the file.

**Updates:** at startup the game asks GitHub's releases API whether a newer
release exists. You can turn this off with Settings > Check for Updates, and
Settings > Check Now checks on demand. When an update exists, a message appears
and the pause menu offers it:
- **Windows installer:** downloads the new installer, verifies its SHA-256
  against the digest GitHub publishes, installs it silently and restarts the
  game.
- **AppImage:** downloads the new AppImage, verifies it, replaces the file in
  place, then offers Restart Now. The AppImage also carries zsync update
  information for AppImageUpdate.
- **`.deb`, `.rpm`, Arch, `.dmg` and archives:** opens the release page, since
  the package manager or you install those.

Downloads use the system `curl` and are accepted only from this repository's
release URLs.

The user data folder for saves and screenshots:
- Linux: `~/.local/share/gol3d`
- Windows: `%APPDATA%\gol3d`
- macOS: `~/Library/Application Support/gol3d`

**Hotbar stamps:**
- Single cell
- 2x2x2 block
- 3D plus
- 8³ and 16³ random soups
- 5x5 wall
- 8-block pillar
- The current rule's seed soup
- Glider: Bays' glider (Life 4555's under Life 4555, Life 5766's otherwise). It
  lies flat on the ground and stands up on a wall; Q/E set its heading and Z/C
  tilt it so it climbs or dives.

**Targeting:** reach is 6 blocks. What you place goes on the first of these:
- the face of the block you target;
- the y = 0 ground;
- empty air 4 blocks ahead.

A white outline shows where the whole stamp will go, including its rotation and tilt. Stamps grow
away from the surface. A wall placed on the ground stands up across your view.
Cells that would overlap the player are skipped.

**Player:** you use Minecraft's body size (0.6 x 1.8 blocks, eyes at 1.62) and
movement values:
- walking 4.3 blocks/s, sprinting 5.6;
- gravity 32 blocks/s²;
- jumps about 1.25 blocks high;
- flying 10.9 blocks/s, doubled when sprinting.

You collide with live blocks and, while walking, stand on the y = 0 ground. Life
itself can still grow below it. A block born inside you never traps you; you can
always move out. A new world spawns you on the ground, facing the rule's seed,
which rests on the ground.

## Tutorial

Esc, then **Tutorial...**, opens eight short lessons. Each one clears the world,
places a small pattern, picks a rule and moves the camera; a panel on the right
explains what to look for. The world keeps running normally: press N to step and
G to run, and click the world to look around.

| Input | Action |
| -- | -- |
| Next >, Left/Right arrows | Next / previous lesson |
| Replay, Backspace | Reset the lesson's scene |
| Close, Finish | Close the panel and keep playing in the scene |

1. **Neighbors**: one block with its 26 neighbors outlined.
2. **Survive and birth**: a Life 5766 blinker slab with every cell outlined by
   what happens to it next (born, dies, survives).
3. **Conway's numbers in 3D**: Conway's glider, one layer thick, under B3/S23.
   It grows without limit.
4. **Life 5766: a still life**: the 2x2x2 block.
5. **Life 5766: oscillators**: Conway's blinker and toad, two layers thick
   (period 2).
6. **Life 5766: the glider**: Conway's glider, two layers thick (period 4).
7. **Life 4555**: the block dies there; 4555's own 10-cell glider (period 4).
8. **Beyond Life**: the block under Crystal, and the non-Life-like rules.

Lesson content lives in `src/tutorial/TutorialLessons.cpp`, the panel in
`src/tutorial/Tutorial.cpp`, and the patterns in `src/life/Patterns.h`.

## Rules

Every rule is a two-state rule over the 26 cells of the 3x3x3 cube around a cell,
written as survive/birth neighbor counts. The Stamps & Rules screen (Tab) and the
New World screen spell each rule out in words.

Conway's Life is B3/S23 over 8 neighbors, but those numbers do not carry over to
3D. Applied literally, B3/S23 grows without limit because 3 of 26 neighbors are
easy to find. The closest 3D analogs are Carter Bays' rules that meet his
criteria for a true Game of Life (*Candidates for the Game of Life in Three
Dimensions*, Complex Systems 1, 1987): random soups settle into still lifes and
oscillators instead of dying out or exploding, and gliders exist. The game
defaults to Life 5766.

Life 5766 contains Conway's game. Stack a 2D pattern two layers thick: a live
cell with n live 2D neighbors then has 2n + 1 live 3D neighbors (n per layer plus
its twin), and an empty cell in the slab has 2n. S5-7 keeps 2n + 1 in {5, 7}, so
n is 2 or 3; B6 needs 2n = 6, so n is 3. That is exactly Conway's S23/B3. Cells
just above or below the slab see one layer's 3x3 window, so the copy stays exact
while no such window holds exactly 6 cells. Bays' 5766 block, blinker and glider
are Conway's block, blinker and glider two layers thick. Life 4555 has no such
copy (B5 is odd), and its glider is a different 10-cell shape.

| Rule | Notation | In words | Behavior |
| -- | -- | -- | -- |
| Life 5766 (default) | S5-7/B6 | Survives with 5 to 7, born with exactly 6 | Bays' 3D Life, the closest match to Conway: soups settle into still lifes and oscillators; gliders exist |
| Life 4555 | S4-5/B5 | Survives with 4 or 5, born with exactly 5 | Bays' livelier Life-like rule: soups churn longer before settling; gliders exist |
| Conway B3/S23 | S2-3/B3 | Survives with 2 or 3, born with exactly 3 | Conway's literal numbers, for comparison: grows without limit in 3D |
| Clouds | S13-26/B13-14,17-19 | Survives with 13 to 26, born with 13, 14 or 17 to 19 | Dense soups erode into stable blobs; not Life-like |
| Crystal | S4/B4 | Survives and is born with exactly 4 | Expands forever in a flickering lattice |
| Slow Crystal | S4-6/B5 | Survives with 4 to 6, born with exactly 5 | Creeps outward, leaving solid structure |
| Coral | S5-8/B6-7,9,12 | Survives with 5 to 8, born with 6, 7, 9 or 12 | Grows fast into coral-like branches |
| Architecture | S4-6/B3 | Survives with 4 to 6, born with exactly 3 | Explodes and keeps churning |

## Materials

Stamps are made of the selected material:
- **Life:** ordinary cells that follow the rule.
- **Stone:** an inert block. No life is born in it and the rule does not count
  it, so a wall one block thick stops a pattern from spreading.
- **Ember:** a block that never changes but always counts as a live neighbor.

Placing fills only empty cells; right click removes any block. Saves (`L3D2`)
store blocks after the live cells; `L3D1` saves from earlier versions load too.
The kinds live in `src/life/CellTypes.h`, which also explains how the GPU stores
them (bit planes per behavior) and how to add more.

## How it works

[ARCHITECTURE.md](ARCHITECTURE.md) is a guided tour of the source. In short:

- **World:** cells are stored one bit each in 32³ chunks (1024 rows of 32 cells)
  that exist only where life is or can reach within the next 8 generations.
  There is no wraparound; space outside the chunk set is dead. The pool of chunk
  slots starts at 512 and doubles as needed, up to `--chunks N` (default 32768,
  at most 131072). When the world reaches the limit, the game pauses once and
  says so; press G to continue, and growth then stops at the edge.
- **Step pass:** `shaders/life3d_step.comp` computes a row of 32 cells per GPU
  thread. A workgroup loads its 32 x 8 slab of rows plus a one-row border into
  shared memory (across chunk borders through each chunk's table of 26
  neighbors), sums the 26 neighbor rows with bitwise full adders into five bit
  planes and applies the rule with a 32-way multiplexer
  (`shaders/life3d_bits.glsl`). Up to 8 generations run per GPU submission.
- **Build pass:** `shaders/life3d_build.comp` counts each chunk's population,
  flags neighbor chunks that live cells or Embers within 8 cells of a face could
  reach, and lists the visible blocks: occupied cells with an uncovered face, in
  chunks within the render distance and a view 20 degrees wider than the
  camera's, nearest chunks first. Each listed block carries its 26 neighbors.
  After it, the CPU adds the flagged chunks and frees chunks that are empty and
  unreachable.
- **Rendering:** `shaders/life3d_blocks.vert` draws the three faces of each
  block that can face the camera (12 vertices, indexed), drops faces covered by
  a neighbor, darkens corners by the blocks around them and grows or shrinks
  cells born or killed by the last change. Fog fades into a sky with a sun glow.
  At most 4.2 million blocks are drawn.
- **Governor:** GPU timestamps measure what a generation and a block list cost.
  Each frame owes the simulation `speed x frame time` generations and pays only
  what fits in the frame's time budget, so a world too big for the requested
  speed slows the tick rate, not the frame rate. A single generation longer
  than the budget is followed by idle frames to keep the game responsive.
- **Vulkan 1.3:** dynamic rendering, synchronization2 and compute subgroup
  operations, through volk. `GOL3D_VALIDATION=1` turns on the validation layers
  when they are installed; `GOL3D_GPU=TEXT` picks a GPU by name.
- **Timeouts:** every GPU wait times out after 4 s. On timeout the game exits
  with an error instead of hanging.

## Releases

Every push to `main` runs `.github/workflows/release.yml` and publishes a GitHub
release, versioned from `VERSION` (major.minor) plus the workflow run number:

| Platform | Packages |
| -- | -- |
| Linux x86_64, aarch64 | `.deb`, `.rpm`, `.tar.gz`, AppImage |
| Arch Linux x86_64 | `.pkg.tar.zst` (built from `packaging/arch/PKGBUILD`) |
| Windows x64, ARM64 | NSIS installer `.exe`, `.zip` |
| macOS 12+ (universal) | `.dmg` with MoltenVK bundled; not notarized |

A `SHA256SUMS` file is published alongside. Pull requests build the same
packages as workflow artifacts without publishing. Locally, `cpack` in the build
directory makes the packages for the host platform.

## Testing

```bash
ctest --test-dir build -LE gpu    # CPU-only
ctest --test-dir build            # CPU checks plus the GPU check
```

`gol3d --verify` steps soups with Stone and Ember blocks, centered on a chunk
corner, for 24 generations, one generation per GPU batch and then eight. It
compares every cell with a dense CPU reference (`src/life/LifeRules.h`).
The `bit_life` test runs the shader's bit-sliced arithmetic
(`shaders/life3d_bits.glsl`, through `src/life/BitLife.h`) on the CPU against the
same reference for every rule, and checks the chunk hash map, so CI covers the
engine without a GPU. `gol3d --rule N --bench G` times G generations of the
simulation alone.
Scripted input drives the real picking and editing code for end-to-end
screenshots. For example:

```bash
./build/gol3d --empty --pos 0.5,1.5,-6 --look 90,0 --place --slot 6 --place \
  --screenshot shot.png
```

Other scripted actions:
- `--push DX,DY,DZ` moves the player through the collision code;
- `--save PATH` writes the world on exit;
- `--load PATH` opens a save.
- `--rotate N` turns the stamp N quarter turns.
- `--tilt N` tilts the stamp N quarter turns around x.
- `--material NAME` builds with life, stone or ember.
- `--speed N` sets the speed in generations per second; `--view N` the render
  distance; `--debug` shows the F3 overlay; `--hide-hud` hides the HUD.
- `--update-feed URL` checks a releases JSON (a `file://` URL works) instead of GitHub.

Run `gol3d --help` for all options.

The `patterns` test checks every pattern claim the tutorial makes against the
CPU reference: still lifes, oscillator periods, glider periods and shifts, that
doubled Conway patterns follow Conway's 2D game under Life 5766, and what each
lesson's scene does. Tutorial lessons render headlessly with
`--menu tutorial:N` (lesson N); `--steps` then advances that lesson's scene:

```bash
./build/gol3d --empty --menu tutorial:3 --steps 12 --screenshot lesson3.png
```

The gliders came from a brute-force soup search, built on request:

```bash
cmake --build build --target find_patterns
./build/find_patterns "Life 4555" 20000
```
