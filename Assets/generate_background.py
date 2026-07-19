#!/usr/bin/env python3
"""Procedurally render the WhammyDT brushed-metal chassis background."""

from __future__ import annotations

import struct
import zlib
from pathlib import Path

import numpy as np


WIDTH = 1000
HEIGHT = 600
SEED = 0x57484D59  # "WHMY"
OUTPUT = Path(__file__).with_name("background.png")


def write_rgb_png(path: Path, pixels: np.ndarray) -> None:
    """Write an 8-bit opaque RGB PNG using only Python's standard library."""
    pixels = np.ascontiguousarray(pixels, dtype=np.uint8)

    def chunk(kind: bytes, payload: bytes) -> bytes:
        body = kind + payload
        return struct.pack(">I", len(payload)) + body + struct.pack(">I", zlib.crc32(body))

    scanlines = b"".join(b"\x00" + row.tobytes() for row in pixels)
    data = (
        b"\x89PNG\r\n\x1a\n"
        + chunk(b"IHDR", struct.pack(">IIBBBBB", WIDTH, HEIGHT, 8, 2, 0, 0, 0))
        + chunk(b"IDAT", zlib.compress(scanlines, level=9))
        + chunk(b"IEND", b"")
    )
    path.write_bytes(data)


def smooth_axis(values: np.ndarray, radius: int, axis: int) -> np.ndarray:
    """Fast edge-padded box blur along one axis."""
    if radius <= 0:
        return values
    pad = [(0, 0)] * values.ndim
    pad[axis] = (radius, radius)
    padded = np.pad(values, pad, mode="reflect")
    padded = np.moveaxis(padded, axis, 0)
    cumulative = np.cumsum(padded, axis=0, dtype=np.float64)
    cumulative = np.concatenate([np.zeros_like(cumulative[:1]), cumulative], axis=0)
    width = radius * 2 + 1
    result = (cumulative[width:] - cumulative[:-width]) / width
    return np.moveaxis(result, 0, axis).astype(np.float32)


def composite(image: np.ndarray, color: tuple[float, float, float], alpha: np.ndarray) -> None:
    alpha = np.clip(alpha, 0.0, 1.0)[..., None]
    image[:] = image * (1.0 - alpha) + np.asarray(color, dtype=np.float32) * alpha


def rounded_box_sdf(
    x: np.ndarray,
    y: np.ndarray,
    left: float,
    top: float,
    right: float,
    bottom: float,
    radius: float,
) -> np.ndarray:
    cx = (left + right) * 0.5
    cy = (top + bottom) * 0.5
    hx = (right - left) * 0.5 - radius
    hy = (bottom - top) * 0.5 - radius
    qx = np.abs(x - cx) - hx
    qy = np.abs(y - cy) - hy
    outside = np.hypot(np.maximum(qx, 0.0), np.maximum(qy, 0.0))
    inside = np.minimum(np.maximum(qx, qy), 0.0)
    return outside + inside - radius


def stroke_sdf(image: np.ndarray, sdf: np.ndarray, distance: float, width: float, color, opacity: float) -> None:
    coverage = np.clip(width * 0.5 + 0.7 - np.abs(sdf - distance), 0.0, 1.0)
    composite(image, color, coverage * opacity)


def draw_chassis_rim(image: np.ndarray, x: np.ndarray, y: np.ndarray) -> None:
    # A narrow rounded lip, similar to the machined perimeter on the sibling UIs.
    sdf = rounded_box_sdf(x, y, 4.0, 4.0, WIDTH - 5.0, HEIGHT - 5.0, 16.0)
    stroke_sdf(image, sdf, 0.0, 2.0, (4, 5, 6), 0.98)
    stroke_sdf(image, sdf, -2.0, 1.0, (48, 52, 54), 0.70)
    stroke_sdf(image, sdf, -4.0, 1.0, (8, 9, 10), 0.95)
    stroke_sdf(image, sdf, -6.0, 1.0, (34, 37, 39), 0.55)

    inner = rounded_box_sdf(x, y, 10.0, 10.0, WIDTH - 11.0, HEIGHT - 11.0, 11.0)
    stroke_sdf(image, inner, 0.0, 1.2, (2, 3, 4), 0.85)
    stroke_sdf(image, inner, -1.6, 0.8, (39, 42, 44), 0.35)


def draw_header_divider(image: np.ndarray, x: np.ndarray, y: np.ndarray) -> None:
    # Header height follows the roughly 12% proportion used by both references.
    fade = np.clip(np.minimum((x - 16.0) / 6.0, (WIDTH - 17.0 - x) / 6.0), 0.0, 1.0)
    composite(image, (3, 4, 5), np.exp(-0.5 * ((y - 72.0) / 0.55) ** 2) * fade * 0.94)
    composite(image, (39, 42, 44), np.exp(-0.5 * ((y - 73.35) / 0.48) ** 2) * fade * 0.42)
    composite(image, (8, 9, 10), np.exp(-0.5 * ((y - 75.0) / 1.2) ** 2) * fade * 0.30)


def draw_screw(image: np.ndarray, x: np.ndarray, y: np.ndarray, cx: float, cy: float, angle: float) -> None:
    dx = x - cx
    dy = y - cy
    radius = np.hypot(dx, dy)

    # Recess, bright rim, and slightly convex dark fastener face.
    composite(image, (1, 2, 3), np.clip(1.0 - np.abs(radius - 10.0), 0.0, 1.0) * 0.95)
    composite(image, (52, 57, 59), np.clip(1.0 - np.abs(radius - 8.6), 0.0, 1.0) * 0.72)
    composite(image, (11, 13, 14), np.clip(8.2 - radius, 0.0, 1.0) * 0.95)

    face = np.clip(1.0 - (radius / 7.5) ** 2, 0.0, 1.0)
    light = np.clip((-0.50 * dx - 0.86 * dy + 3.5) / 10.0, 0.0, 1.0)
    face_color = np.stack((18 + 17 * light, 20 + 18 * light, 21 + 19 * light), axis=-1)
    alpha = np.clip(7.7 - radius, 0.0, 1.0)[..., None]
    image[:] = image * (1.0 - alpha) + face_color * alpha

    # Shallow machined slot, rotated differently on each screw.
    along = dx * np.cos(angle) + dy * np.sin(angle)
    across = -dx * np.sin(angle) + dy * np.cos(angle)
    slot = np.clip(1.0 - (np.abs(across) - 0.15), 0.0, 1.0) * np.clip(4.7 - np.abs(along), 0.0, 1.0)
    slot *= np.clip(7.0 - radius, 0.0, 1.0)
    composite(image, (4, 6, 7), slot * 0.78)
    slot_glint = slot * np.clip(across + 1.2, 0.0, 1.0)
    composite(image, (54, 63, 65), slot_glint * 0.28)

    # Tiny cool reflection links the hardware to the Moth Production palette.
    glint = np.exp(-((dx + 3.1) ** 2 + (dy + 3.6) ** 2) / 1.8)
    composite(image, (76, 170, 183), glint * face * 0.65)


def draw_vent_bank(image: np.ndarray, x: np.ndarray, y: np.ndarray, left: float) -> None:
    slot_width = 4.0
    gap = 3.0
    top = 543.0
    bottom = 566.0
    for index in range(22):
        x0 = left + index * (slot_width + gap)
        sdf = rounded_box_sdf(x, y, x0, top, x0 + slot_width, bottom, 2.0)
        shadow = np.clip(1.0 - np.abs(sdf - 1.0), 0.0, 1.0)
        rim = np.clip(0.85 - np.abs(sdf), 0.0, 1.0)
        fill = np.clip(0.25 - sdf, 0.0, 1.0)
        inner_glint = np.clip(0.75 - np.abs(sdf + 0.9), 0.0, 1.0)
        composite(image, (0, 1, 2), shadow * 0.82)
        composite(image, (48, 51, 52), rim * 0.52)
        composite(image, (2, 3, 4), fill * 0.94)
        composite(image, (43, 46, 47), inner_glint * 0.34)


def render() -> np.ndarray:
    rng = np.random.default_rng(SEED)
    yy, xx = np.mgrid[0:HEIGHT, 0:WIDTH].astype(np.float32)

    # Horizontal brushed aluminium: several correlated noise scales create fine
    # hairlines and longer soft streaks without turning into obvious scanlines.
    noise = rng.normal(0.0, 1.0, (HEIGHT, WIDTH)).astype(np.float32)
    long_brush = smooth_axis(noise, 42, axis=1)
    medium_brush = smooth_axis(noise, 13, axis=1)
    hairline = smooth_axis(noise, 2, axis=1)
    row_variation = smooth_axis(rng.normal(0.0, 1.0, (HEIGHT, 1)).astype(np.float32), 2, axis=0)
    broad_mottle = smooth_axis(smooth_axis(noise, 95, axis=1), 18, axis=0)

    texture = (
        5.8 * long_brush
        + 2.8 * medium_brush
        + 1.35 * hairline
        + 1.05 * noise
        + 1.25 * row_variation
        + 4.1 * broad_mottle
    )
    texture += 0.65 * np.sin(yy * 1.71 + xx * 0.010)
    texture += 0.42 * np.sin(yy * 0.47 - xx * 0.027)

    # Sparse elongated machining marks, kept subtle enough to read as material.
    for _ in range(34):
        cy = rng.uniform(18, HEIGHT - 18)
        cx = rng.uniform(50, WIDTH - 50)
        length = rng.uniform(75, 290)
        thickness = rng.uniform(0.20, 0.62)
        slope = rng.uniform(-0.018, 0.018)
        distance = np.abs(yy - cy - slope * (xx - cx))
        along = np.clip(1.0 - np.abs(xx - cx) / length, 0.0, 1.0) ** 2
        mark = np.exp(-0.5 * (distance / thickness) ** 2) * along
        texture += mark * rng.uniform(-2.8, 2.0)

    # Gentle edge vignette and a trace of top-to-bottom tonal falloff.
    nx = (xx - (WIDTH - 1) * 0.5) / (WIDTH * 0.5)
    ny = (yy - (HEIGHT - 1) * 0.5) / (HEIGHT * 0.5)
    vignette = np.clip((nx * nx + ny * ny - 0.18) / 0.95, 0.0, 1.0)
    vertical_tone = -1.3 * ny

    base = np.array([19.0, 21.0, 22.0], dtype=np.float32)
    image = base + texture[..., None] * np.array([0.90, 0.96, 1.00], dtype=np.float32)
    image += vertical_tone[..., None]
    image -= vignette[..., None] * np.array([7.4, 7.1, 6.8], dtype=np.float32)

    # The slim header band is fractionally lighter, as on the reference panels.
    header_lift = np.clip((76.0 - yy) / 76.0, 0.0, 1.0) * 1.25
    image += header_lift[..., None]

    draw_chassis_rim(image, xx, yy)
    draw_header_divider(image, xx, yy)
    draw_vent_bank(image, xx, yy, 58.0)
    draw_vent_bank(image, xx, yy, 792.0)

    screws = (
        (25.0, 25.0, 0.55),
        (975.0, 25.0, -0.36),
        (25.0, 575.0, -0.70),
        (975.0, 575.0, 0.28),
    )
    for screw in screws:
        draw_screw(image, xx, yy, *screw)

    return np.clip(np.rint(image), 0, 255).astype(np.uint8)


def main() -> None:
    pixels = render()
    write_rgb_png(OUTPUT, pixels)
    luminance = pixels.astype(np.float32).mean(axis=2)
    percentiles = np.percentile(luminance, [1, 5, 50, 95, 99])
    print(f"Wrote {OUTPUT} ({WIDTH}x{HEIGHT}, opaque RGB)")
    print(
        "Luminance: "
        f"min={luminance.min():.1f} max={luminance.max():.1f} "
        f"mean={luminance.mean():.2f} std={luminance.std():.2f}; "
        f"p01/p05/p50/p95/p99={percentiles.round(1).tolist()}"
    )


if __name__ == "__main__":
    main()
