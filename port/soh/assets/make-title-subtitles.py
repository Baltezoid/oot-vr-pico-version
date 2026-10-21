#!/usr/bin/env python3
# Makes the subtitles of the EXPLORE and ARENA quests for the title of file select. The images are 8 times the size
# of the 128x32 texture that the game draws (the ".8x." in the file name), so they are sharp in VR.
#
# Usage: make-title-subtitles.py <serif bold font .ttf>
# Example font: LiberationSerif-Bold.ttf (package fonts-liberation).
import math
import os
import sys

from PIL import Image, ImageChops, ImageDraw, ImageFilter, ImageFont

S = 8
W, H = 128 * S, 32 * S
OUT_DIR = os.path.join(os.path.dirname(os.path.abspath(__file__)), "custom", "objects", "object_mag")


def text_mask(font, text):
    mask = Image.new("L", (W, H), 0)
    draw = ImageDraw.Draw(mask)
    box = draw.textbbox((0, 0), text, font=font)
    width, height = box[2] - box[0], box[3] - box[1]
    # The text ends at x 98, and the icon is at the right of it.
    x = 98 * S - width - box[0]
    y = (H - height) // 2 - box[1]
    draw.text((x, y), text, font=font, fill=255)
    return mask, y + box[1], height


def vertical_gradient(stops, top, height):
    image = Image.new("RGBA", (W, H))
    draw = ImageDraw.Draw(image)
    for row in range(H):
        t = min(max((row - top) / max(1, height), 0.0), 1.0) * (len(stops) - 1)
        i = min(int(t), len(stops) - 2)
        k = t - i
        color = tuple(int(stops[i][c] + (stops[i + 1][c] - stops[i][c]) * k) for c in range(3))
        draw.line([(0, row), (W, row)], fill=color + (255,))
    return image


def outline_of(mask):
    return mask.filter(ImageFilter.MaxFilter(S + 1)).filter(ImageFilter.GaussianBlur(S * 0.6))


def save(image, name):
    path = os.path.join(OUT_DIR, name)
    image.save(path)
    print(path)


def make_arena(font):
    mask, top, height = text_mask(font, "ARENA")
    cx, cy = 113 * S, 17 * S

    def sword(angle):
        ca, sa = math.cos(angle), math.sin(angle)

        def p(u, v):
            return (cx + u * ca - v * sa, cy + u * sa + v * ca)

        blade = [p(-3 * S, -1.2 * S), p(9.5 * S, -1.2 * S), p(12 * S, 0), p(9.5 * S, 1.2 * S), p(-3 * S, 1.2 * S)]
        guard = [p(-4.2 * S, -3 * S), p(-3 * S, -3 * S), p(-3 * S, 3 * S), p(-4.2 * S, 3 * S)]
        grip = [p(-7.5 * S, -0.9 * S), p(-4.2 * S, -0.9 * S), p(-4.2 * S, 0.9 * S), p(-7.5 * S, 0.9 * S)]
        return blade, guard, grip, p(-8.2 * S, 0)

    blades = Image.new("L", (W, H), 0)
    hilts = Image.new("L", (W, H), 0)
    draw_blades = ImageDraw.Draw(blades)
    draw_hilts = ImageDraw.Draw(hilts)
    for blade, guard, grip, pommel in (sword(math.radians(-45)), sword(math.radians(-135))):
        draw_blades.polygon(blade, fill=255)
        draw_hilts.polygon(guard, fill=255)
        draw_hilts.polygon(grip, fill=255)
        r = 1.3 * S
        draw_hilts.ellipse([pommel[0] - r, pommel[1] - r, pommel[0] + r, pommel[1] + r], fill=255)

    out = Image.new("RGBA", (W, H), (0, 0, 0, 0))
    out.paste((70, 30, 15, 230), (0, 0), outline_of(ImageChops.lighter(mask, ImageChops.lighter(blades, hilts))))
    out.paste(vertical_gradient([(255, 236, 150), (250, 150, 40), (170, 40, 20)], top, height), (0, 0), mask)
    out.paste(vertical_gradient([(235, 240, 250), (175, 190, 220)], 0, H), (0, 0), blades)
    out.paste((200, 140, 40, 255), (0, 0), hilts)
    save(out, "gTitleArenaSubtitleTex.8x.rgba32.png")


def make_explore(font):
    mask, top, height = text_mask(font, "EXPLORE")
    cx, cy, r = 113.5 * S, 16.5 * S, 10.5 * S

    def disc(radius):
        disc_mask = Image.new("L", (W, H), 0)
        ImageDraw.Draw(disc_mask).ellipse([cx - radius, cy - radius, cx + radius, cy + radius], fill=255)
        return disc_mask

    def needle(angle, half_width, length):
        ca, sa = math.cos(angle), math.sin(angle)
        tip = (cx + length * ca, cy + length * sa)
        left = (cx - half_width * sa, cy + half_width * ca)
        right = (cx + half_width * sa, cy - half_width * ca)
        return [left, tip, right]

    out = Image.new("RGBA", (W, H), (0, 0, 0, 0))
    out.paste((15, 25, 55, 230), (0, 0), outline_of(ImageChops.lighter(mask, disc(r))))
    out.paste(vertical_gradient([(225, 245, 255), (120, 190, 235), (35, 90, 170)], top, height), (0, 0), mask)
    out.paste(vertical_gradient([(250, 215, 110), (215, 160, 40)], cy - r, 2 * r), (0, 0), disc(r))
    out.paste((248, 242, 225, 255), (0, 0), disc(r - 2.2 * S))

    draw = ImageDraw.Draw(out)
    for i in range(8):
        angle = i * math.pi / 4
        inner = (r - 4.2 * S) if i % 2 == 0 else (r - 3.4 * S)
        outer = r - 2.6 * S
        draw.line(
            [(cx + inner * math.cos(angle), cy + inner * math.sin(angle)),
             (cx + outer * math.cos(angle), cy + outer * math.sin(angle))],
            fill=(150, 150, 150, 255), width=int(0.8 * S))
    north_east = math.radians(-50)
    draw.polygon(needle(north_east, 1.6 * S, 6.2 * S), fill=(215, 30, 35, 255))
    draw.polygon(needle(north_east + math.pi, 1.6 * S, 6.2 * S), fill=(30, 60, 175, 255))
    pin = 0.9 * S
    draw.ellipse([cx - pin, cy - pin, cx + pin, cy + pin], fill=(200, 150, 50, 255))
    save(out, "gTitleExploreSubtitleTex.8x.rgba32.png")


def main():
    if len(sys.argv) != 2:
        sys.exit(__doc__ or "Usage: make-title-subtitles.py <serif bold font .ttf>")
    font = ImageFont.truetype(sys.argv[1], 16 * S)
    make_arena(font)
    make_explore(font)


if __name__ == "__main__":
    main()
