#!/usr/bin/env python3
"""Capture the in-game 3D GIFs and screenshots used by README.md.

Drives the real game (build/prototype/gol3d) with its scripted-input flags: every
frame is one short run that advances N generations, places the camera and saves a
screenshot. Needs a Vulkan driver, Pillow and ffmpeg.

When gamescope is installed, the script re-runs itself inside one headless
gamescope session, so no game window opens on your desktop or takes focus. Set
GOL3D_CAPTURE_VISIBLE=1 to use your own display instead.

    cmake -S prototype -B build/prototype -G Ninja -DCMAKE_BUILD_TYPE=Release
    cmake --build build/prototype
    python3 scripts/readme-media/capture_3d.py
"""

import math
import os
import shutil
import struct
import subprocess
import sys
import tempfile
from pathlib import Path

from PIL import Image, ImageDraw, ImageFont

ROOT = Path(__file__).resolve().parents[2]
GAME = ROOT / "build" / "prototype" / "gol3d"
OUT = ROOT / "docs" / "media"
SEED = 7


def shoot(path, size, *args):
    cmd = [
        str(GAME),
        "--seed",
        str(SEED),
        "--resize",
        f"{size[0]},{size[1]}",
        *map(str, args),
        "--screenshot",
        str(path),
    ]
    result = subprocess.run(cmd, capture_output=True, text=True)
    if result.returncode != 0 or not Path(path).exists():
        sys.exit(f"gol3d failed: {' '.join(cmd)}\n{result.stdout}\n{result.stderr}")


def orbit_camera(center, radius, height, angle):
    """--pos/--look arguments for a camera on a circle, looking at center."""
    eye = (
        center[0] + radius * math.cos(angle),
        height,
        center[2] + radius * math.sin(angle),
    )
    d = [c - e for c, e in zip(center, eye)]
    length = math.sqrt(sum(v * v for v in d))
    yaw = math.degrees(math.atan2(d[2], d[0]))
    pitch = math.degrees(math.asin(d[1] / length))
    return [
        "--pos",
        ",".join(f"{v:.3f}" for v in eye),
        "--look",
        f"{yaw:.3f},{pitch:.3f}",
    ]


def label(image, text):
    """Dark caption band across the top edge (it also hides the game's status line)."""
    draw = ImageDraw.Draw(image)
    font = ImageFont.load_default(size=max(14, image.height // 18))
    box = draw.textbbox((0, 0), text, font=font)
    draw.rectangle((0, 0, image.width, box[3] + 12), fill=(20, 24, 32))
    draw.text((8, 5), text, font=font, fill=(235, 240, 250))


def encode_gif(frames_dir, pattern, out, fps, width, hold_last=0, colors=64):
    """Palette-optimized GIF via ffmpeg, kept under the 1000 KB pre-commit limit.
    hold_last repeats the final frame (seconds)."""
    pad = f",tpad=stop_mode=clone:stop_duration={hold_last}" if hold_last else ""
    graph = (
        f"fps={fps},scale={width}:-1:flags=lanczos{pad},split[a][b];"
        f"[a]palettegen=max_colors={colors}:stats_mode=diff[p];"
        "[b][p]paletteuse=dither=none:diff_mode=rectangle"
    )
    subprocess.run(
        [
            "ffmpeg",
            "-loglevel",
            "error",
            "-y",
            "-framerate",
            str(fps),
            "-i",
            str(frames_dir / pattern),
            "-filter_complex",
            graph,
            "-loop",
            "0",
            str(out),
        ],
        check=True,
    )
    print(f"wrote {out.relative_to(ROOT)} ({out.stat().st_size // 1024} KiB)")


def encode_gif_pillow(paths, out, fps, width, hold_last=0, colors=64):
    """GIF via Pillow with one shared palette. Used where ffmpeg's palette filters
    drop frames (seen with the glider clip on ffmpeg 7)."""
    frames = []
    for path in paths:
        image = Image.open(path).convert("RGB")
        frames.append(
            image.resize(
                (width, round(image.height * width / image.width)), Image.LANCZOS
            )
        )
    # Build the shared palette from a strip of sampled frames so every shade is covered.
    samples = frames[:: max(1, len(frames) // 6)]
    strip = Image.new("RGB", (frames[0].width, frames[0].height * len(samples)))
    for i, frame in enumerate(samples):
        strip.paste(frame, (0, i * frame.height))
    palette = strip.quantize(colors=colors)
    frames = [f.quantize(palette=palette, dither=Image.Dither.NONE) for f in frames]
    durations = [1000 // fps] * len(frames)
    durations[-1] += int(hold_last * 1000)
    frames[0].save(
        out,
        save_all=True,
        append_images=frames[1:],
        duration=durations,
        loop=0,
        optimize=True,
    )
    print(f"wrote {out.relative_to(ROOT)} ({out.stat().st_size // 1024} KiB)")


def crop_hotbar(image, keep=0.86):
    """Drop the bottom strip (hotbar) and the status line so orbit shots show only the world."""
    top = max(16, image.height // 20)
    return image.convert("RGB").crop((0, top, image.width, int(image.height * keep)))


def hero(tmp):
    """Life 5766 soup settling. The camera holds still so GIF frames compress well."""
    frames = 36
    camera = orbit_camera((0, 12, 0), 54, 27, math.radians(40))
    for i in range(frames):
        path = tmp / f"hero_{i:03d}.png"
        shoot(
            path, (960, 540), "--rule", 1, "--steps", i, "--fly", "--slot", 0, *camera
        )
        image = crop_hotbar(Image.open(path))
        label(image, f"Life 5766  S5-7/B6   generation {i}")
        image.save(path)
    encode_gif(tmp, "hero_%03d.png", OUT / "hero-life5766.gif", 6, 720, hold_last=1.5)


def rules_grid(tmp):
    """Four rules side by side, the same number of generations each."""
    settle = ((0, 11, 0), 50, 27)  # fixed cameras: (look at, distance, height)
    grow = ((0, 14, 0), 62, 38)
    rules = [  # (rule index, caption, camera)
        (1, "Life 5766  S5-7/B6", settle),
        (2, "Life 4555  S4-5/B5", settle),
        (3, "Conway's numbers  S2-3/B3", grow),
        (7, "Coral  S5-8/B6-7,9,12", grow),
    ]
    frames = 32
    tile = (480, 300)
    for i in range(frames):
        sheet = Image.new("RGB", (tile[0] * 2 + 4, tile[1] * 2 + 4), (20, 24, 32))
        for k, (rule, caption, camera) in enumerate(rules):
            path = tmp / f"rule{rule}_{i:03d}.png"
            center, radius, height = camera
            shoot(
                path,
                (tile[0], int(tile[1] / 0.84)),
                "--rule",
                rule,
                "--steps",
                i,
                "--fly",
                "--slot",
                0,
                *orbit_camera(center, radius, height, math.radians(40)),
            )
            image = crop_hotbar(Image.open(path)).resize(tile, Image.LANCZOS)
            label(image, f"{caption}   gen {i}")
            sheet.paste(image, ((k % 2) * (tile[0] + 4), (k // 2) * (tile[1] + 4)))
        sheet.save(tmp / f"rules_{i:03d}.png")
    encode_gif(tmp, "rules_%03d.png", OUT / "rules-compared.gif", 5, 720, hold_last=2)


def sandbox(tmp):
    """Place three 16x16x16 soups on the ground, then run Life 5766."""
    spots = [-22, 0, 22]
    camera = ["--pos", "0,22,46", "--look", "-90,-22"]
    place = []
    frame = 0
    shoot(
        tmp / f"sandbox_{frame:03d}.png",
        (960, 540),
        "--empty",
        "--fly",
        "--slot",
        5,
        *camera,
    )
    frame += 1
    for x in spots:
        # Stand 3 blocks up, look steeply down so the ground is within reach.
        place += ["--pos", f"{x},3,10", "--look", "-90,-60", "--slot", "5", "--place"]
        for _ in range(2):  # hold each placement for two frames
            shoot(
                tmp / f"sandbox_{frame:03d}.png",
                (960, 540),
                "--empty",
                "--fly",
                *place,
                *camera,
            )
            frame += 1
    world = tmp / "sandbox.life3d"
    subprocess.run(
        [
            str(GAME),
            "--seed",
            str(SEED),
            "--empty",
            "--fly",
            *place,
            "--frames",
            "1",
            "--resize",
            "320,180",
            "--save",
            str(world),
        ],
        check=True,
        capture_output=True,
    )
    for gen in range(1, 36):
        shoot(
            tmp / f"sandbox_{frame:03d}.png",
            (960, 540),
            "--load",
            world,
            "--steps",
            gen,
            "--fly",
            "--slot",
            0,
            *camera,
        )
        frame += 1
    encode_gif(
        tmp, "sandbox_%03d.png", OUT / "sandbox-build.gif", 6, 720, hold_last=1.5
    )


def write_save(path, rule_index, cells):
    """A world in the game's save format ("L3D1"; see saveWorld in src/main_minimal.cpp)."""
    header = struct.pack(
        "<4sIQ3fffQ", b"L3D1", rule_index, 0, 0, 2, 0, 0, 0, len(cells)
    )
    path.write_bytes(header + b"".join(struct.pack("<3i", *c) for c in cells))


def glider(tmp):
    """Bays' Life 5766 glider: Conway's glider, two layers thick (include/Life3DPatterns.h)."""
    conway = [(1, 0), (2, 1), (0, 2), (1, 2), (2, 2)]
    world = tmp / "glider.life3d"
    write_save(world, 0, [(x, y, z) for y in (0, 1) for x, z in conway])
    frames = 25  # six periods: the glider moves (+1, 0, +1) every 4 generations
    camera = orbit_camera((4.5, 0.5, 4.5), 6.5, 7, math.radians(-60))
    for i in range(frames):
        path = tmp / f"glider_{i:03d}.png"
        shoot(
            path,
            (800, 450),
            "--load",
            world,
            "--steps",
            i,
            "--fly",
            "--slot",
            0,
            *camera,
        )
        image = crop_hotbar(Image.open(path))
        label(image, f"Life 5766 glider   generation {i}")
        image.save(path)
    paths = [tmp / f"glider_{i:03d}.png" for i in range(frames)]
    encode_gif_pillow(
        paths, OUT / "glider-life5766.gif", 5, 560, hold_last=1, colors=192
    )


def tutorial():
    """Tutorial lesson 2: every cell outlined by what happens to it next."""
    path = OUT / "tutorial-survive-birth.png"
    shoot(path, (1280, 720), "--empty", "--menu", "tutorial:2")
    Image.open(path).convert("RGB").resize((960, 540), Image.LANCZOS).save(
        path, optimize=True
    )
    print(f"wrote {path.relative_to(ROOT)}")


def menus():
    for name in ("inventory", "pause", "settings"):
        path = OUT / f"menu-{name}.png"
        shoot(path, (1280, 720), "--menu", name)
        Image.open(path).convert("RGB").resize((960, 540), Image.LANCZOS).save(
            path, optimize=True
        )
        print(f"wrote {path.relative_to(ROOT)}")


def run_headless_if_possible():
    """Re-exec this script inside a headless gamescope so game windows stay off-screen."""
    if os.environ.get("GOL3D_CAPTURE_NESTED") or os.environ.get(
        "GOL3D_CAPTURE_VISIBLE"
    ):
        return
    gamescope = shutil.which("gamescope")
    if not gamescope:
        print("gamescope not found: game windows will open on this display")
        return
    env = dict(os.environ, GOL3D_CAPTURE_NESTED="1")
    cmd = [
        gamescope,
        "--backend",
        "headless",
        "-W",
        "1280",
        "-H",
        "720",
        "--",
        sys.executable,
        __file__,
        *sys.argv[1:],
    ]
    sys.exit(subprocess.run(cmd, env=env).returncode)


def main():
    if not GAME.exists():
        sys.exit(f"Build the game first: {GAME} not found")
    run_headless_if_possible()
    if not shutil.which("ffmpeg"):
        sys.exit("ffmpeg is required")
    OUT.mkdir(parents=True, exist_ok=True)
    wanted = set(sys.argv[1:]) or {
        "hero",
        "rules",
        "sandbox",
        "glider",
        "tutorial",
        "menus",
    }
    with tempfile.TemporaryDirectory() as tmpdir:
        tmp = Path(tmpdir)
        if "hero" in wanted:
            hero(tmp)
        if "rules" in wanted:
            rules_grid(tmp)
        if "sandbox" in wanted:
            sandbox(tmp)
        if "glider" in wanted:
            glider(tmp)
    if "tutorial" in wanted:
        tutorial()
    if "menus" in wanted:
        menus()


if __name__ == "__main__":
    main()
