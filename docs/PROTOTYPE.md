# Playable prototype

`prototype/` builds `gol3d`, a Minecraft-style 3D Game of Life. You walk or fly
through an unbounded world, break and place live blocks, and watch the automaton
evolve.
It needs only Vulkan, GLFW and GLM. CMake uses installed GLFW and GLM when it
finds them and otherwise downloads pinned releases, so no vcpkg setup is needed.

```bash
cmake -S prototype -B build/prototype -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build/prototype
./build/prototype/gol3d
```

Requirements: a Vulkan 1.0 GPU driver, `glslc` (from shaderc or the Vulkan SDK), and
on Linux the X11 or Wayland development headers that GLFW needs.

## Controls

The mapping follows Minecraft's defaults, except that left click places and right
click removes. Simulation actions use keys Minecraft leaves unbound.

| Input | Action |
| -- | -- |
| Mouse | Look around. Click the window to grab the mouse. |
| Esc | Pause menu (world freezes): Back to Game, Stamps & Rules, New World, Save/Load, Settings, Quit |
| E | Stamps & Rules screen: pick a stamp or the empty hand, and switch rules (hover for descriptions) |
| W A S D | Move |
| Space | Jump; fly up while flying |
| Double-tap Space | Toggle flying (as in creative mode) |
| Left Shift | Fly down |
| Left Ctrl | Sprint |
| Left click | Place the selected hotbar stamp (hold to repeat) |
| Right click | Remove the outlined block (hold to repeat) |
| 1-8, scroll wheel | Select a hotbar stamp. Pressing the selected number again empties your hand: nothing is placed and no placement outline is shown. |
| G | Run or pause generations |
| N | Advance one generation (hold to repeat) |
| [ and ] (or - and +) | Slower / faster: 0.5 to 60 generations per second (starts at 1) |
| R, Shift+R | Next / previous rule. Cells are kept. |
| Ctrl+N, Ctrl+Shift+N | New world seeded with the rule's soup / empty world |
| Ctrl+S, Ctrl+O | Save / load `world.life3d` in the user data folder |
| F1 | Hide the crosshair, hotbar and outlines |
| F2 | Screenshot (`screenshots/life3d-<time>.png` in the user data folder) |
| F3 | Debug overlay: position, chunk, facing, live cells, chunk budget, fps |
| F3+G | Show chunk borders |
| F11 | Fullscreen |
| H | Print the controls |
| Ctrl+Q | Quit |

The top-left corner shows the rule, generation and speed, and whether the world
is running. Selecting a stamp shows its name above the hotbar. Messages such as
saves and rule changes appear mid-screen. The window title carries the same
status.

**Settings** (Esc, then Settings):
- field of view
- render distance (fog)
- mouse sensitivity and invert mouse
- GUI scale (Auto, or 1 to 4)
- simulation speed
- HUD, chunk borders and fullscreen

They save to `~/.config/gol3d/options.txt` (`%APPDATA%\gol3d` on Windows) and
load on start. Scripted and test runs ignore the file.

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

**Targeting:** reach is 6 blocks. What you place goes on the first of these:
- the face of the block you target;
- the y = 0 ground;
- empty air 4 blocks ahead.

A white outline marks the spot when you're not targeting a block. Stamps grow
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

## Rules

Every rule is a two-state rule over the 26 cells of the 3x3x3 cube around a cell,
written as survive/birth neighbor counts. The Stamps & Rules screen (E) and the
New World screen spell each rule out in words.

Conway's Life is B3/S23 over 8 neighbors, but those numbers do not carry over to
3D. Applied literally, B3/S23 grows without limit because 3 of 26 neighbors are
easy to find. The closest 3D analogs are Carter Bays' rules that meet his
criteria for a true Game of Life (*Candidates for the Game of Life in Three
Dimensions*, Complex Systems 1, 1987): random soups settle into still lifes and
oscillators instead of dying out or exploding, and gliders exist. The game
defaults to Life 5766.

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

## How it works

- **World:** cells are stored in 16³ chunks that exist only where life is or can
  spread next. There is no wraparound; space outside the chunk set is dead.
- **Simulation:** `shaders/life3d_chunks.comp` steps every active chunk using a
  table of its 26 neighbor chunks. It also does three other things:
  - appends live cells to the instance list for the indirect draw;
  - counts each chunk's population;
  - flags which neighbor chunks live border cells touch.

  After each pass, the CPU uses that report to add chunks where life can spread
  and free chunks that emptied.
- **Budget:** the chunk budget defaults to 2048 chunks (`--chunks N`, up to
  16000), and at most about 1M blocks are drawn. The window title shows
  `(LIMIT)` or `(draw capped)` when a budget is reached. The first time a world
  reaches the chunk limit, the game pauses and says so. Press G to continue;
  growth then stops at the edge.
- **Rendering:** blocks are instanced cubes with fixed per-face shading and
  distance fog. They're drawn over a sky gradient, with a block grid on y = 0.
- **Timeouts:** every GPU wait times out after 2 s. On timeout the game exits
  with an error instead of hanging.

## Testing

```bash
ctest --test-dir build/prototype            # CPU rule checks + GPU check
ctest --test-dir build/prototype -LE gpu    # CPU-only
```

`gol3d --verify` steps soups centered on a chunk corner for 20 generations. It
compares every cell with a dense CPU reference (`include/Life3DRules.h`).
Scripted input drives the real picking and editing code for end-to-end
screenshots. For example:

```bash
./build/prototype/gol3d --empty --pos 0.5,1.5,-6 --look 90,0 --place --slot 6 --place \
  --screenshot shot.png
```

Other scripted actions:
- `--push DX,DY,DZ` moves the player through the collision code;
- `--save PATH` writes the world on exit;
- `--load PATH` opens a save.

Run `gol3d --help` for all options.
