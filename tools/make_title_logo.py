#!/usr/bin/env python3
"""Render the title-screen logo (gold Roman capitals) to 3ds_port/gfx/title_logo.png.

    python3 tools/make_title_logo.py Cinzel-900.ttf

Cinzel (SIL OFL 1.1, licenses/Cinzel-OFL.txt) at a fixed size, with a gold
gradient, a dark outline and a soft drop shadow. Needs Pillow. The PNG is
committed, so a normal build doesn't run this.
"""
import os
import sys

from PIL import Image, ImageDraw, ImageFilter, ImageFont

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
OUT = os.path.join(ROOT, "3ds_port", "gfx", "title_logo.png")


def main():
    font = ImageFont.truetype(sys.argv[1], 62)
    text = "FREECIV"
    l, t, r, b = font.getbbox(text)
    pad = 10
    w, h = r - l + pad * 2, b - t + pad * 2
    mask = Image.new("L", (w, h), 0)
    ImageDraw.Draw(mask).text((pad - l, pad - t), text, font=font, fill=255)

    # Outline: the glyphs grown by ~3 px.
    outline = mask.filter(ImageFilter.MaxFilter(7))
    shadow = outline.filter(ImageFilter.GaussianBlur(3))

    img = Image.new("RGBA", (w, h), (0, 0, 0, 0))
    sh = Image.new("RGBA", (w, h), (0, 0, 0, 170))
    img.paste(sh, (3, 4), shadow)
    ol = Image.new("RGBA", (w, h), (46, 26, 8, 255))
    img.paste(ol, (0, 0), outline)

    # Gold, light at the top to deep at the bottom, with a bright band.
    grad = Image.new("RGBA", (w, h))
    gp = grad.load()
    for y in range(h):
        f = (y - pad) / max(1, h - 2 * pad)
        f = min(1.0, max(0.0, f))
        if f < 0.45:
            k = f / 0.45
            c = (int(255 - 8 * k), int(244 - 34 * k), int(190 - 90 * k))
        else:
            k = (f - 0.45) / 0.55
            c = (int(247 - 57 * k), int(210 - 80 * k), int(100 - 60 * k))
        for x in range(w):
            gp[x, y] = c + (255,)
    img.paste(grad, (0, 0), mask)

    # Crop to content, keep even dimensions.
    bb = img.getbbox()
    img = img.crop(bb)
    img.save(OUT)
    print(OUT, img.size)


if __name__ == "__main__":
    main()
