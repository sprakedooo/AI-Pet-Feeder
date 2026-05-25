"""
Generate Smart Pet Feeder app icon.
Produces:
  assets/images/app_icon.png   — 1024x1024 source (for launcher icons)
  assets/images/logo.png       — 256x256 for in-app use (splash / login)
"""

import math
from PIL import Image, ImageDraw

def draw_rounded_rect(draw, xy, radius, fill):
    x0, y0, x1, y1 = xy
    draw.rectangle([x0 + radius, y0, x1 - radius, y1], fill=fill)
    draw.rectangle([x0, y0 + radius, x1, y1 - radius], fill=fill)
    draw.ellipse([x0, y0, x0 + radius * 2, y0 + radius * 2], fill=fill)
    draw.ellipse([x1 - radius * 2, y0, x1, y0 + radius * 2], fill=fill)
    draw.ellipse([x0, y1 - radius * 2, x0 + radius * 2, y1], fill=fill)
    draw.ellipse([x1 - radius * 2, y1 - radius * 2, x1, y1], fill=fill)

def draw_paw(draw, cx, cy, size, color):
    """Draw a simple paw print centred at (cx, cy)."""
    # Main pad (oval)
    pw, ph = size * 0.55, size * 0.45
    draw.ellipse([cx - pw, cy - ph * 0.3, cx + pw, cy + ph * 0.8], fill=color)
    # Four toe pads
    toe_r = size * 0.18
    offsets = [(-size*0.45, -size*0.45), (-size*0.16, -size*0.58),
               ( size*0.16, -size*0.58), ( size*0.45, -size*0.45)]
    for ox, oy in offsets:
        draw.ellipse([cx+ox-toe_r, cy+oy-toe_r, cx+ox+toe_r, cy+oy+toe_r], fill=color)

def draw_bowl(draw, cx, cy, w, h, rim_color, fill_color, food_color):
    """Draw a side-view food bowl."""
    # Bowl body (ellipse base)
    bx, by = w * 0.52, h * 0.28
    # Rim
    draw.ellipse([cx - bx - 8, cy - 10, cx + bx + 8, cy + 10],
                 fill=rim_color)
    # Bowl interior (trapezoid shape using polygon)
    draw.polygon([
        (cx - bx,      cy),
        (cx + bx,      cy),
        (cx + bx*0.7,  cy + h),
        (cx - bx*0.7,  cy + h),
    ], fill=fill_color)
    # Rim top cap
    draw.ellipse([cx - bx - 8, cy - 14, cx + bx + 8, cy + 14],
                 fill=rim_color)
    # Food (kibble dots inside bowl)
    import random
    random.seed(42)
    for _ in range(9):
        fx = cx + random.randint(int(-bx*0.55), int(bx*0.55))
        fy = cy + random.randint(6, int(h * 0.55))
        fr = random.randint(int(bx*0.08), int(bx*0.13))
        draw.ellipse([fx - fr, fy - fr, fx + fr, fy + fr], fill=food_color)

def draw_wifi_arc(draw, cx, cy, r, color, width=6):
    """Draw three concentric arcs to suggest connectivity/smart."""
    for i, scale in enumerate([0.35, 0.65, 1.0]):
        sr = int(r * scale)
        draw.arc([cx - sr, cy - sr, cx + sr, cy + sr],
                 start=210, end=330, fill=color, width=width - i)

def make_icon(size):
    img = Image.new('RGBA', (size, size), (0, 0, 0, 0))
    draw = ImageDraw.Draw(img)

    # ── Background rounded square ──────────────────────────────────────
    bg_green  = (76, 175, 80)      # #4CAF50  primary
    bg_dark   = (56, 142, 60)      # #388E3C  primaryDark
    radius    = size // 5

    # Gradient simulation: draw top half lighter, bottom darker
    for y in range(size):
        t = y / size
        r = int(bg_green[0] * (1-t) + bg_dark[0] * t)
        g = int(bg_green[1] * (1-t) + bg_dark[1] * t)
        b = int(bg_green[2] * (1-t) + bg_dark[2] * t)
        draw.rectangle([0, y, size, y+1], fill=(r, g, b))

    # Clip to rounded square by masking
    mask = Image.new('L', (size, size), 0)
    mdraw = ImageDraw.Draw(mask)
    draw_rounded_rect(mdraw, [0, 0, size, size], radius, 255)
    img.putalpha(mask)

    # Re-draw gradient only inside mask
    grad = Image.new('RGBA', (size, size), (0, 0, 0, 0))
    gdraw = ImageDraw.Draw(grad)
    for y in range(size):
        t = y / size
        r2 = int(bg_green[0] * (1-t) + bg_dark[0] * t)
        g2 = int(bg_green[1] * (1-t) + bg_dark[1] * t)
        b2 = int(bg_green[2] * (1-t) + bg_dark[2] * t)
        gdraw.rectangle([0, y, size, y+1], fill=(r2, g2, b2, 255))
    grad.putalpha(mask)
    img = grad

    draw = ImageDraw.Draw(img)

    # ── Paw print (top-centre, white) ─────────────────────────────────
    paw_size = size * 0.18
    draw_paw(draw, size * 0.5, size * 0.24, paw_size,
             (255, 255, 255, 220))

    # ── Bowl (centre) ─────────────────────────────────────────────────
    bw = size * 0.38
    bh = size * 0.20
    bcx, bcy = size * 0.5, size * 0.58
    draw_bowl(draw, bcx, bcy, bw, bh,
              rim_color=(255, 255, 255, 240),
              fill_color=(255, 255, 255, 60),
              food_color=(255, 200, 80, 230))

    # ── Small wifi arcs (top-right, suggesting smart/AI) ──────────────
    draw_wifi_arc(draw, size * 0.78, size * 0.26, size * 0.10,
                  (255, 255, 255, 180), width=max(2, size // 100))

    # ── Subtle inner glow ring ─────────────────────────────────────────
    glow = Image.new('RGBA', (size, size), (0, 0, 0, 0))
    gdraw2 = ImageDraw.Draw(glow)
    pad = size * 0.04
    for i in range(3):
        gdraw2.ellipse([pad+i*2, pad+i*2, size-pad-i*2, size-pad-i*2],
                       outline=(255, 255, 255, 18 - i*5), width=max(1, size//80))
    img = Image.alpha_composite(img, glow)

    return img

import os, pathlib
base = pathlib.Path(r"C:\Users\jubil\AI PET FEEDER\mobile")
out  = base / "assets" / "images"
out.mkdir(parents=True, exist_ok=True)

# 1024×1024 for launcher icon generation
icon_1024 = make_icon(1024)
icon_1024.save(str(out / "app_icon.png"))
print(f"Saved app_icon.png  ({(out/'app_icon.png').stat().st_size//1024} KB)")

# 256×256 for in-app logo (splash + login)
icon_256 = make_icon(256)
icon_256.save(str(out / "logo.png"))
print(f"Saved logo.png  ({(out/'logo.png').stat().st_size//1024} KB)")

print("Done.")
