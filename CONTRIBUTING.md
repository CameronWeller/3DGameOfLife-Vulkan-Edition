# Contributing to 3D Game of Life: Vulkan Edition

Contributions are welcome, from typo fixes to new rules to rendering work. The
[README](README.md#contributing) covers building and testing;
[docs/ARCHITECTURE.md](docs/ARCHITECTURE.md) explains how the code fits together.

## Workflow

1. Open an issue first for anything bigger than a small fix, so we can agree on the approach.
2. Fork the repository and branch from `main`.
3. Build and run the tests. Add or extend a test in `tests/` when you change logic that runs on
   the CPU; run `gol3d --verify` when you touch the simulation shaders.
4. Format and lint (below).
5. Commit with a short, imperative message prefixed like the history: `fix:`, `feat:`, `docs:`,
   `refactor:`, `test:`, `build:`, `ci:`.
6. Open a pull request against `main` describing what changed and how you tested it.
   Screenshots or GIFs help for anything visual.

By contributing, you agree that your contributions are licensed under the project's
[MIT License](LICENSE).

## Code style

The code is read far more often than it is written, often by people learning how a GPU
cellular automaton works. Write for them.

### Formatting

`clang-format` decides layout; don't fight it. The configuration is [`.clang-format`](.clang-format)
(4-space indent, 100 columns, braces on the same line). The Lint workflow checks every pull
request with clang-format 21.

```bash
clang-format -i $(git ls-files '*.cpp' '*.h')
pre-commit install   # optional: format and lint on every commit
```

### Naming

| Kind | Style | Example |
| -- | -- | -- |
| Types, enumerators | `PascalCase` | `ChunkWorld`, `CellKind::Ember` |
| Functions, variables, parameters | `camelCase` | `ensureChunk`, `blockSlot` |
| Constants (`constexpr`, `static const`) | `UPPER_CASE` | `CHUNK_SIZE`, `MAX_BATCH` |
| Private data members of classes | `camelCase_` | `activeSlots_` |
| Public fields of plain structs | `camelCase` | `Settings::renderDistance` |
| Namespaces | `lowercase` | `gol3d`, `gol3d::ui` |
| Files | `PascalCase.cpp/.h`; shaders `life3d_*.glsl` | `ChunkWorld.cpp` |

Name things for what they mean in the game, not for their type: `blockSlot`, not `b`; `stepCostMs`,
not `t1`. Single letters are fine for loop counters and coordinates (`x`, `y`, `z`, `i`). Put units
in names when they are not obvious (`budgetMs`, `secondsLeft`).

`clang-tidy` checks the naming rules and the rest of the list below; see [`.clang-tidy`](.clang-tidy).

### Structure

- One statement per line. No comma-operator tricks (`a = 1, b = 2;`), and declare one variable
  per statement.
- A body may skip its braces only when it fits on the same line as its `if`:
  `if (slot == NO_CHUNK) return;`. Loops always get braces.
- Prefer a named helper or a named intermediate over a long expression. If a line needs a
  comment to be understood, consider whether a better name would make the comment unnecessary.
- Keep functions to one job. When a function grows past a screen, look for the steps inside it
  and give each a name.
- Plain structs for data (`Settings`, `SavedWorld`); classes with private `_` members when there
  is an invariant to protect (`ChunkWorld`, `ChunkMap`).
- Casts are `static_cast<T>(x)` (or `reinterpret_cast` where needed), never `T(x)` or `(T)x`.
- Error messages for the player say what failed in plain words: `"Could not create the swap
  chain."`, not `"vkCreateSwapchainKHR failed!"`.
- Give numbers a name unless their meaning is obvious where they stand (`MAX_BATCH`, not `8`).
- Prefer a small struct with named fields over several `bool` arguments
  (`runBatch(1, {.animate = true})`, not `runBatch(1, true, true)`).
- Code that needs no GPU belongs in `gol3d_core` (see `CMakeLists.txt`) so it can be tested.

### Comments

- Every file starts with a comment saying what it is for and how it fits in. Every class and
  non-obvious function has a comment saying what it does and why, not how.
- Explain the clever parts: bit tricks, packed formats, index arithmetic, Vulkan
  synchronization. Cite the source for algorithms (`FIPS 180-4`, `Amanatides and Woo`).
- Describe the code as it is. Don't narrate history ("now", "used to", "the new engine"); that
  belongs in commit messages.
- When code must match another file (a shader struct, a binding number, a color table), say so
  in both places.

### Shaders

The same rules apply to GLSL. `shaders/life3d_bits.glsl` is also compiled as C++ by
`src/life/BitLife.h`, so it must stay in the common subset of both languages: no constructors,
no vector types, no GLSL-only built-ins.

## Checks before a pull request

```bash
cmake --preset release && cmake --build --preset release
ctest --preset release            # CPU tests
./build/gol3d --verify            # GPU against the CPU reference (needs a GPU)
clang-format --dry-run --Werror $(git ls-files '*.cpp' '*.h')
clang-tidy -p build src/game/Game.cpp   # or any file you changed
```

## Reporting bugs

Use the [issue tracker](https://github.com/CameronWeller/3DGameOfLife-Vulkan-Edition/issues).
Include your OS, GPU and driver version, what you did, what you expected, and what happened.
The <kbd>F3</kbd> overlay and the terminal output are both useful to paste in.
