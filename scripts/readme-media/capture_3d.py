#!/usr/bin/env python3
"""Capture the in-game 3D GIFs and screenshots used by README.md.

Drives the real game (build/prototype/gol3d) with its scripted-input flags: every
frame is one short run that advances N generations, places the camera and saves a
screenshot. Needs a display, a Vulkan driver, Pillow and ffmpeg.

    cmake -S prototype -B build/prototype -G Ninja -DCMAKE_BUILD_TYPE=Release
    cmake --build build/prototype
    python3 scripts/readme-media/capture_3d.py
"""

import math
import shutil
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


def menus():
    for name in ("inventory", "pause", "settings"):
        path = OUT / f"menu-{name}.png"
        shoot(path, (1280, 720), "--menu", name)
        Image.open(path).convert("RGB").resize((960, 540), Image.LANCZOS).save(
            path, optimize=True
        )
        print(f"wrote {path.relative_to(ROOT)}")


def main():
    if not GAME.exists():
        sys.exit(f"Build the game first: {GAME} not found")
    if not shutil.which("ffmpeg"):
        sys.exit("ffmpeg is required")
    OUT.mkdir(parents=True, exist_ok=True)
    wanted = set(sys.argv[1:]) or {"hero", "rules", "sandbox", "menus"}
    with tempfile.TemporaryDirectory() as tmpdir:
        tmp = Path(tmpdir)
        if "hero" in wanted:
            hero(tmp)
        if "rules" in wanted:
            rules_grid(tmp)
        if "sandbox" in wanted:
            sandbox(tmp)
    if "menus" in wanted:
        menus()


if __name__ == "__main__":
    main()
