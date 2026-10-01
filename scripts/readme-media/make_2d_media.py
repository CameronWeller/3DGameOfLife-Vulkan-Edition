#!/usr/bin/env python3
"""Render the 2D Conway's Game of Life GIFs and the neighborhood diagram in README.md.

Pure Python + Pillow, no game build needed:

    python3 scripts/readme-media/make_2d_media.py
"""

import random
from pathlib import Path

from PIL import Image, ImageDraw, ImageFont

OUT = Path(__file__).resolve().parents[2] / "docs" / "media"

BG = (17, 21, 28)
GRID = (32, 38, 50)
LIVE = (94, 230, 168)
BORN = (124, 196, 255)
DYING = (255, 107, 107)
TEXT = (225, 232, 245)
MUTED = (140, 150, 170)


def font(size):
    return ImageFont.load_default(size=size)


def parse(rows):
    """Cells from an ASCII picture: 'O' is alive."""
    return {
        (x, y) for y, row in enumerate(rows) for x, ch in enumerate(row) if ch == "O"
    }


def neighbors(cell):
    x, y = cell
    return [(x + dx, y + dy) for dx in (-1, 0, 1) for dy in (-1, 0, 1) if dx or dy]


def step(cells, width=None, height=None):
    """One B3/S23 generation. With width/height, cells outside the box are dropped."""
    counts = {}
    for cell in cells:
        for n in neighbors(cell):
            counts[n] = counts.get(n, 0) + 1
    nxt = {c for c, k in counts.items() if k == 3 or (k == 2 and c in cells)}
    if width is not None:
        nxt = {(x, y) for x, y in nxt if 0 <= x < width and 0 <= y < height}
    return nxt


def draw_board(
    cells,
    width,
    height,
    size,
    born=(),
    dying=(),
    offset=(0, 0),
    image=None,
    origin=(0, 0),
):
    """Draw a width x height board of size-pixel cells onto image at origin."""
    if image is None:
        image = Image.new("RGB", (width * size + 1, height * size + 1), BG)
    draw = ImageDraw.Draw(image)
    ox, oy = origin
    if size >= 8:
        for x in range(width + 1):
            draw.line((ox + x * size, oy, ox + x * size, oy + height * size), fill=GRID)
        for y in range(height + 1):
            draw.line((ox, oy + y * size, ox + width * size, oy + y * size), fill=GRID)
    pad = 1 if size >= 8 else 0
    for colour, group in ((LIVE, cells), (BORN, born), (DYING, dying)):
        for x, y in group:
            x, y = x - offset[0], y - offset[1]
            if 0 <= x < width and 0 <= y < height:
                draw.rectangle(
                    (
                        ox + x * size + pad,
                        oy + y * size + pad,
                        ox + (x + 1) * size - pad,
                        oy + (y + 1) * size - pad,
                    ),
                    fill=colour,
                )
    return image


def save_gif(frames, path, durations):
    if isinstance(durations, int):
        durations = [durations] * len(frames)
    frames = [f.convert("P", palette=Image.ADAPTIVE, colors=32) for f in frames]
    frames[0].save(
        path,
        save_all=True,
        append_images=frames[1:],
        duration=durations,
        loop=0,
        optimize=True,
        disposal=1,
    )
    print(f"wrote {path.name} ({path.stat().st_size // 1024} KiB)")


def caption(image, text, y, colour=TEXT, size=18):
    draw = ImageDraw.Draw(image)
    draw.text((image.width // 2, y), text, font=font(size), fill=colour, anchor="mt")


def rules_explained():
    """A glider stepping, with each step split into 'what changes' and 'result'."""
    cells = parse([".O...", "..O..", "OOO.."])
    cells = {(x + 2, y + 2) for x, y in cells}
    w, h, size, top = 10, 8, 36, 64
    frames, durations = [], []
    for gen in range(4):
        nxt = step(cells)
        born, dying = nxt - cells, cells - nxt
        for phase in range(2):
            image = Image.new("RGB", (w * size + 1, h * size + 1 + top + 40), BG)
            if phase == 0:
                draw_board(
                    cells - dying,
                    w,
                    h,
                    size,
                    born=born,
                    dying=dying,
                    image=image,
                    origin=(0, top),
                )
                caption(image, f"generation {gen}: which cells change?", 10)
                d = ImageDraw.Draw(image)
                y = image.height - 30
                d.rectangle((24, y, 40, y + 16), fill=BORN)
                d.text((46, y), "born (exactly 3)", font=font(15), fill=MUTED)
                d.rectangle((200, y, 216, y + 16), fill=DYING)
                d.text((222, y), "dies (<2 or >3)", font=font(15), fill=MUTED)
                caption(
                    image, "live cells with 2 or 3 neighbors survive", 36, MUTED, 15
                )
            else:
                draw_board(nxt, w, h, size, image=image, origin=(0, top))
                caption(image, f"generation {gen + 1}", 10)
                caption(image, "every cell updates at the same time", 36, MUTED, 15)
            frames.append(image)
            durations.append(1300)
        cells = nxt
    save_gif(frames, OUT / "conway-rules.gif", durations)


def glider():
    cells = parse([".O.", "..O", "OOO"])
    w, h, size = 20, 14, 18
    cells = {(x + 1, y + 1) for x, y in cells}
    frames = []
    for _ in range(48):
        frames.append(draw_board(cells, w, h, size))
        cells = step(cells)
    save_gif(frames, OUT / "conway-glider.gif", 140)


def oscillators():
    patterns = [
        ("blinker (p2)", parse(["OOO"]), 5, 5, (1, 2)),
        ("toad (p2)", parse([".OOO", "OOO."]), 6, 6, (1, 2)),
        ("beacon (p2)", parse(["OO..", "OO..", "..OO", "..OO"]), 6, 6, (1, 1)),
        (
            "pulsar (p3)",
            parse(
                [
                    "..OOO...OOO..",
                    ".............",
                    "O....O.O....O",
                    "O....O.O....O",
                    "O....O.O....O",
                    "..OOO...OOO..",
                    ".............",
                    "..OOO...OOO..",
                    "O....O.O....O",
                    "O....O.O....O",
                    "O....O.O....O",
                    ".............",
                    "..OOO...OOO..",
                ]
            ),
            17,
            17,
            (2, 2),
        ),
    ]
    size, gap, top = 12, 28, 34
    boards = [
        (name, {(x + dx, y + dy) for x, y in cells}, w, h)
        for name, cells, w, h, (dx, dy) in patterns
    ]
    col = lambda w: max(w * size, 110)
    total_w = sum(col(w) for _, _, w, _ in boards) + gap * (len(boards) + 1)
    total_h = max(h * size for _, _, _, h in boards) + top + gap
    frames = []
    for _ in range(6):
        image = Image.new("RGB", (total_w, total_h), BG)
        x = gap
        for i, (name, cells, w, h) in enumerate(boards):
            y = top + (total_h - top - gap - h * size) // 2
            draw_board(
                cells, w, h, size, image=image, origin=(x + (col(w) - w * size) // 2, y)
            )
            ImageDraw.Draw(image).text(
                (x + col(w) // 2, 10), name, font=font(15), fill=TEXT, anchor="mt"
            )
            boards[i] = (name, step(cells), w, h)
            x += col(w) + gap
        frames.append(image)
    save_gif(frames, OUT / "conway-oscillators.gif", 500)


def glider_gun():
    gun = parse(
        [
            "........................O...........",
            "......................O.O...........",
            "............OO......OO............OO",
            "...........O...O....OO............OO",
            "OO........O.....O...OO..............",
            "OO........O...O.OO....O.O...........",
            "..........O.....O.......O...........",
            "...........O...O....................",
            "............OO......................",
        ]
    )
    w, h, size = 64, 40, 9
    cells = {(x + 2, y + 2) for x, y in gun}
    frames = []
    for gen in range(150):  # 5 periods of 30
        # Simulate on a larger board so gliders leave cleanly instead of piling up at the edge.
        frames.append(draw_board(cells, w, h, size))
        cells = step(cells, w + 10, h + 10)
    save_gif(frames, OUT / "conway-glider-gun.gif", 70)


def soup():
    rng = random.Random(1970)
    w = h = 80
    cells = {(x, y) for x in range(w) for y in range(h) if rng.random() < 0.35}
    frames = []
    for gen in range(220):
        if gen % 2 == 0:
            frames.append(draw_board(cells, w, h, 5))
        cells = step(cells, w, h)
    save_gif(frames, OUT / "conway-soup.gif", [80] * (len(frames) - 1) + [2500])


def neighborhoods():
    """8 neighbors on a 3x3 square next to 26 neighbors in a 3x3x3 cube, plus its three layers."""
    image = Image.new("RGB", (1180, 420), BG)
    draw = ImageDraw.Draw(image)

    def square_grid(ox, oy, size, centre):
        for x in range(3):
            for y in range(3):
                colour = LIVE if centre and (x, y) == (1, 1) else BORN
                draw.rectangle(
                    (
                        ox + x * size + 3,
                        oy + y * size + 3,
                        ox + (x + 1) * size - 3,
                        oy + (y + 1) * size - 3,
                    ),
                    fill=colour,
                )

    # 2D: 3x3 square
    square_grid(90, 115, 70, True)
    draw.text((195, 40), "2D: 8 neighbors", font=font(24), fill=TEXT, anchor="mt")
    draw.text(
        (195, 370),
        "Conway: born with 3, survives with 2-3",
        font=font(15),
        fill=MUTED,
        anchor="mt",
    )

    # 3D: exploded isometric 3x3x3 cube, drawn back to front
    edge = 58
    cx, cy = 590, 215

    def project(x, y, z):  # x toward the viewer on the right, z on the left, y up
        return (cx + (x - z) * edge * 0.866, cy + (x + z) * edge * 0.5 - y * edge)

    def shade(colour, k):
        return tuple(int(c * k) for c in colour)

    cubes = [(x, y, z) for x in range(3) for y in range(3) for z in range(3)]
    cubes.sort(key=lambda c: (c[0] + c[2], c[1]))  # back to front, bottom to top
    s = 0.52  # small cubes so the lattice reads
    for x, y, z in cubes:
        colour = LIVE if (x, y, z) == (1, 1, 1) else BORN
        bx, by, bz = x - 1.5 + (1 - s) / 2, y - 1.5 + (1 - s) / 2, z - 1.5 + (1 - s) / 2
        top = [
            project(bx, by + s, bz),
            project(bx + s, by + s, bz),
            project(bx + s, by + s, bz + s),
            project(bx, by + s, bz + s),
        ]
        left = [
            project(bx, by, bz + s),
            project(bx + s, by, bz + s),
            project(bx + s, by + s, bz + s),
            project(bx, by + s, bz + s),
        ]
        right = [
            project(bx + s, by, bz),
            project(bx + s, by, bz + s),
            project(bx + s, by + s, bz + s),
            project(bx + s, by + s, bz),
        ]
        if (x, y, z) == (2, 2, 2):
            # The front corner cube hides the centre exactly; draw it as a wireframe.
            for face in (left, right, top):
                draw.polygon(face, outline=BORN)
            continue
        draw.polygon(left, fill=shade(colour, 0.62), outline=BG)
        draw.polygon(right, fill=shade(colour, 0.8), outline=BG)
        draw.polygon(top, fill=colour, outline=BG)
    draw.text((cx, 40), "3D: 26 neighbors", font=font(24), fill=TEXT, anchor="mt")
    draw.text(
        (cx, 370),
        "Bays' Life 5766: born with 6, survives with 5-7",
        font=font(15),
        fill=MUTED,
        anchor="mt",
    )

    # The same cube as three layers: 9 + 8 + 9 = 26
    for i, (name, centre) in enumerate(
        (("layer above: 9", False), ("same layer: 8", True), ("layer below: 9", False))
    ):
        oy = 50 + i * 112
        draw.text((905, oy), name, font=font(15), fill=MUTED, anchor="lt")
        square_grid(1040, oy - 8, 30, centre)
    draw.text((1000, 370), "9 + 8 + 9 = 26", font=font(15), fill=MUTED, anchor="mt")
    path = OUT / "neighborhoods.png"
    image.save(path, optimize=True)
    print(f"wrote {path.name}")


def main():
    OUT.mkdir(parents=True, exist_ok=True)
    rules_explained()
    glider()
    oscillators()
    glider_gun()
    soup()
    neighborhoods()


if __name__ == "__main__":
    main()
