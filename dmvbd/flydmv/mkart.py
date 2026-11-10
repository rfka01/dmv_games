#!/usr/bin/env python3
"""Draws the bitmaps of FLYDMV.AD (own artwork, CC0/MIT with DMVBD):
a flying NCR DMV (all-in-one: screen left, two floppy drives right) with
a small rocket flame, 5.25" floppies in rainbow colours (4 tumble
frames), a smiling sun and a planet. Writes 4-bit Windows BMPs with the
standard 16-colour palette, black = background.
Code: Claude, Prompts: rfka01"""
from PIL import Image, ImageDraw
import math, sys, os

PAL = [(0,0,0),(128,0,0),(0,128,0),(128,128,0),(0,0,64),   # 4: night sky, 25 % blue on the DMV
       (128,0,128),(0,128,128),(128,128,128),
       (192,192,192),(255,0,0),(0,255,0),(255,255,0),(0,0,255),(255,0,255),(0,255,255),(255,255,255)]
BLK, DRED, DGRN, OLIV, NAVY, PURP, TEAL, GREY, SILV, RED, GRN, YEL, BLU, MAG, CYA, WHT = range(16)

def new(w, h, bg=0):
    im = Image.new('P', (w, h), bg)
    im.putpalette([c for p in PAL for c in p] + [0] * (768 - 48))
    return im

def save_bmp(im, path):
    """4-bit DIB, BITMAPINFOHEADER, bottom-up."""
    w, h = im.size
    px = im.load()
    stride = ((w * 4 + 31) // 32) * 4
    rows = []
    for y in range(h - 1, -1, -1):
        r = bytearray(stride)
        for x in range(w):
            v = px[x, y] & 15
            r[x // 2] |= v << (0 if x & 1 else 4)
        rows.append(bytes(r))
    bits = b''.join(rows)
    import struct
    hdr = struct.pack('<IiiHHIIiiII', 40, w, h, 1, 4, 0, len(bits), 0, 0, 16, 16)
    pal = b''.join(bytes((b, g, r, 0)) for r, g, b in PAL)
    off = 14 + 40 + 64
    fh = b'BM' + struct.pack('<IHHI', off + len(bits), 0, 0, off)
    open(path, 'wb').write(fh + hdr + pal + bits)

def dmv(flame):
    """NCR DMV after the gray cabinet of our MAME layout (dmv.lay), 96 x 56:
    screen left, panel with two vertical floppy drives right, knobs and the
    red power switch below the drives, NCR tiles on top. Flying left, so
    the rocket flame is at the right."""
    im = new(96, 56, NAVY); d = ImageDraw.Draw(im)
    oy = 4
    # rocket flame behind the cabinet
    fl = [(86, 20 + oy), (95, 24 + oy + flame), (86, 30 + oy)]
    d.polygon(fl, fill=YEL); d.polygon([(86, 23 + oy), (91, 25 + oy + flame // 2), (86, 27 + oy)], fill=RED)
    # shell (light gray, darker rim) with the inner frame
    # The DMV shows only 8 solid colours; grey is a 50 % dot pattern, so the
    # light grey cabinet is drawn white with grey and black detail lines.
    d.rounded_rectangle([0, oy, 86, oy + 47], 3, fill=WHT, outline=GREY)
    # screen bezel (bevelled recess) and dark screen with green text lines
    d.rectangle([4, oy + 3, 57, oy + 44], fill=GREY)
    d.polygon([(4, oy + 3), (57, oy + 3), (53, oy + 6), (8, oy + 6)], fill=WHT)
    d.line([(4, oy + 3), (57, oy + 3)], fill=GREY)
    d.rounded_rectangle([8, oy + 6, 53, oy + 40], 2, fill=BLK)
    for i, ln in enumerate((30, 22, 36, 16, 26)):
        d.line([(11, oy + 9 + 5 * i), (11 + ln, oy + 9 + 5 * i)], fill=GRN)
    d.rectangle([11, oy + 35, 13, oy + 36], fill=GRN)          # cursor
    # drive panel
    d.rectangle([59, oy + 3, 83, oy + 43], fill=WHT, outline=GREY)
    for x in (61, 72):                                         # two drives, front cream
        d.rectangle([x, oy + 6, x + 9, oy + 33], fill=WHT, outline=BLK)
        d.rectangle([x + 2, oy + 14, x + 7, oy + 19], fill=GREY)  # window
        d.line([(x + 3, oy + 7), (x + 3, oy + 32)], fill=BLK)     # slot
        d.rectangle([x + 1, oy + 8, x + 2, oy + 11], fill=BLK)     # door lever
        d.rectangle([x + 1, oy + 28, x + 1, oy + 30], fill=DRED) # drive LED
    d.rectangle([61, oy + 36, 73, oy + 41], outline=GREY)      # knob plate
    d.rectangle([63, oy + 38, 65, oy + 39], fill=BLK); d.rectangle([69, oy + 38, 71, oy + 39], fill=BLK)
    d.rectangle([76, oy + 36, 78, oy + 42], fill=RED)          # power switch
    d.point([(77, oy + 34)], fill=GRN)                         # power LED
    # NCR tiles on the top edge
    for i in range(3):
        d.rectangle([69 + 4 * i, oy, 71 + 4 * i, oy + 2], fill=BLK)
    # speed lines
    for y in (14, 28, 42):
        d.line([(88, y + oy - 6), (93, y + oy - 6)], fill=WHT) if y != 28 else None
    return im

def floppy(color, angle):
    """5.25" floppy, 24 x 24, rotated by angle (degrees)."""
    big = new(48, 48, NAVY); d = ImageDraw.Draw(big)
    d.rectangle([4, 4, 43, 43], fill=color, outline=WHT if color == BLU else color)
    d.rectangle([16, 4, 31, 11], fill=WHT)                 # label
    d.ellipse([18, 18, 29, 29], fill=BLK)                  # hub hole
    d.ellipse([21, 21, 26, 26], fill=color)
    d.rounded_rectangle([20, 32, 27, 43], 3, fill=BLK)     # head slot
    d.rectangle([10, 21, 12, 23], fill=BLK)                # index hole
    big = big.rotate(angle, resample=Image.NEAREST, fillcolor=NAVY)
    sm = big.resize((24, 24), Image.NEAREST)
    return sm

def sun():
    """Pale glowing sun with a tiny friendly face, soft halo of dots."""
    im = new(64, 64, NAVY); d = ImageDraw.Draw(im)
    c = 32
    for yy in range(64):                       # halo: sparse yellow and white dots
        for xx in range(64):
            r = math.hypot(xx - c, yy - c)
            if 15 < r < 30:
                k = (xx * 3 + yy * 5) % 7
                if (r < 21 and k < 3) or (r < 26 and k == 0) or (r >= 26 and (xx + 2 * yy) % 11 == 0):
                    im.putpixel((xx, yy), YEL if r < 23 else WHT)
    d.ellipse([c - 15, c - 15, c + 15, c + 15], fill=YEL)
    d.ellipse([c - 12, c - 12, c + 12, c + 12], fill=WHT)
    d.rectangle([c - 6, c - 4, c - 5, c - 2], fill=OLIV)  # eyes
    d.rectangle([c + 5, c - 4, c + 6, c - 2], fill=OLIV)
    d.line([(c - 4, c + 3), (c - 2, c + 5), (c + 2, c + 5), (c + 4, c + 3)], fill=OLIV)  # smile
    return im

def planet():
    """Cloudy ocean planet with an island, lit from the upper left."""
    W_, H_ = 176, 120
    im = new(W_, H_, NAVY); d = ImageDraw.Draw(im)
    c, cy, r = 88, 92, 80                      # lower part is cut by the screen edge
    d.ellipse([c - r - 3, cy - r - 3, c + r + 3, cy + r + 3], fill=WHT)      # glow rim
    d.ellipse([c - r, cy - r, c + r, cy + r], fill=CYA)
    # cloud swirls
    for (x0, y0, w0, a0, a1) in ((40, 30, 60, 200, 330), (90, 40, 70, 180, 300), (60, 70, 50, 10, 160),
                                 (120, 80, 60, 200, 340), (30, 90, 40, 270, 60), (110, 25, 34, 20, 170)):
        d.arc([x0, y0, x0 + w0, y0 + w0 // 2], a0, a1, fill=WHT, width=2)
    # a long continent along the left limb, bent like a boomerang, with a
    # peninsula, a bay and a lake; an archipelago towards the lower right
    d.polygon([(22, 64), (30, 44), (44, 30), (60, 22), (74, 20), (80, 26), (70, 30), (58, 36), (50, 46),
               (54, 52), (64, 50), (68, 58), (58, 66), (46, 70), (40, 82), (44, 96), (36, 104), (28, 94),
               (24, 80)], fill=GRN, outline=DGRN)
    d.ellipse([33, 52, 39, 56], fill=CYA)                                   # lake
    d.line([(36, 64), (42, 76)], fill=DGRN)                                 # mountains
    for (x, y, rx, ry) in ((104, 70, 5, 3), (114, 76, 4, 2), (122, 84, 6, 3), (98, 82, 3, 2)):
        d.ellipse([x - rx, y - ry, x + rx, y + ry], fill=GRN, outline=DGRN)
    d.ellipse([84, 18, 96, 22], fill=WHT)                                   # polar ice
    # night side lower right: checker the ocean with navy
    for yy in range(H_):
        for xx in range(W_):
            dx, dy = xx - c, yy - cy
            if dx * dx + dy * dy > r * r: continue
            # distance outside the lit circle (shifted to the upper left) gives a curved terminator
            lx, ly = dx + 34, dy + 34
            q = math.hypot(lx, ly) - r
            if q <= 0: continue
            p = im.getpixel((xx, yy))
            if p == CYA:
                im.putpixel((xx, yy), (TEAL if (xx + yy) & 1 else CYA) if q < 10 else TEAL if q < 22 else NAVY)
            elif p == WHT and q >= 10: im.putpixel((xx, yy), SILV if q < 22 else GREY)
            elif p in (GRN,) and q >= 10: im.putpixel((xx, yy), DGRN)
    return im

def star(k):
    """Sparkle star, 4 twinkle frames, 9 x 9."""
    im = new(9, 9, NAVY); d = ImageDraw.Draw(im)
    if k == 0: d.point([(4, 4)], fill=WHT)
    elif k == 1: d.line([(3, 4), (5, 4)], fill=WHT); d.line([(4, 3), (4, 5)], fill=WHT)
    elif k == 2:
        d.line([(1, 4), (7, 4)], fill=WHT); d.line([(4, 1), (4, 7)], fill=WHT); d.point([(4, 4)], fill=YEL)
    else:
        d.line([(0, 4), (8, 4)], fill=WHT); d.line([(4, 0), (4, 8)], fill=WHT)
        d.point([(3, 3), (5, 3), (3, 5), (5, 5)], fill=CYA)
    return im

def banner(phase):
    """Banner behind the DMV: two tow ropes, then a white cloth with the
    product name in own serif lettering, waving (phase 0..2). 208 x 40."""
    from PIL import ImageFont
    W_, H_, ROPE = 224, 40, 22
    im = new(W_, H_, NAVY); d = ImageDraw.Draw(im)
    f = ImageFont.truetype('/usr/share/fonts/truetype/dejavu/DejaVuSerif-Bold.ttf', 15)
    # cloth as a mask, text drawn flat and then shifted column by column (wave)
    flat = Image.new('P', (W_ - ROPE, 26), WHT); fd = ImageDraw.Draw(flat)
    fd.rectangle([0, 0, W_ - ROPE - 1, 25], outline=GREY)
    tw = f.getlength('DECISION MATE V')
    x0 = ((W_ - ROPE) - tw - 16) / 2
    fd.text((x0, 13), 'DECISION MATE V', font=f, fill=BLK, anchor='lm')
    # a small red heart after the lettering
    hx, hy = x0 + tw + 10, 13
    fd.ellipse([hx - 6, hy - 5, hx, hy + 1], fill=RED); fd.ellipse([hx - 1, hy - 5, hx + 5, hy + 1], fill=RED)
    fd.polygon([(hx - 6, hy - 1), (hx + 5, hy - 1), (hx, hy + 6)], fill=RED)
    fd.line([(3, 3), (W_ - ROPE - 4, 3)], fill=RED); fd.line([(3, 22), (W_ - ROPE - 4, 22)], fill=RED)
    fl = flat.load(); px = im.load()
    for x in range(W_ - ROPE):
        off = int(round(3 * math.sin((x / 34.0 - phase * 2.1)) * min(1.0, x / 40.0)))
        for y in range(26):
            yy = 7 + y + off
            if 0 <= yy < H_: px[ROPE + x, yy] = fl[x, y]
    # ropes from the DMV's tail (left edge, middle) to the cloth corners
    d.line([(0, 20), (ROPE, 7)], fill=WHT); d.line([(0, 20), (ROPE, 32)], fill=WHT)
    return im

def main(out):
    os.makedirs(out, exist_ok=True)
    for i in range(2): save_bmp(dmv(i * 3), f'{out}/DMV{i}.BMP')
    rainbow = [RED, YEL, GRN, CYA, BLU, MAG]
    for c, col in enumerate(rainbow):
        for k in range(4): save_bmp(floppy(col, k * 22.5), f'{out}/D{c}{k}.BMP')
    save_bmp(sun(), f'{out}/SUN.BMP'); save_bmp(planet(), f'{out}/PLANET.BMP')
    for k in range(4): save_bmp(star(k), f'{out}/STAR{k}.BMP')
    for k in range(3): save_bmp(banner(k), f'{out}/BAN{k}.BMP')
    # setting 3 (type 3 = choice list, default = entry number)
    import struct
    names = ['Disketten', 'Banner', 'Wechsel']
    b = bytearray(32 + 16 * len(names))
    struct.pack_into('<H', b, 0, 3); b[2:2 + 7] = b'Ladung:'
    struct.pack_into('<HH', b, 22, len(names), 2)
    for k, n in enumerate(names): b[32 + 16 * k:32 + 16 * k + len(n)] = n.encode()
    open(f'{out}/CTL3.BIN', 'wb').write(bytes(b))
    big = new(224, 130, NAVY)
    for k in range(3): big.paste(banner(k), (0, k * 44))
    big.convert('RGB').resize((224 * 4, 130 * 4), Image.NEAREST).save(f'{out}/banner.png')
    # preview: a full 640 x 400 scene
    sheet = new(640, 400, NAVY)
    sheet.paste(sun(), (120, 18)); sheet.paste(planet(), (640 - 196, 400 - 120))
    import random; rr = random.Random(5)
    for i in range(40): sheet.paste(star(rr.randrange(4)), (rr.randrange(0, 630) & ~1, rr.randrange(0, 390) & ~1))
    sheet.paste(dmv(0), (300, 140))
    for i, col in enumerate(rainbow):
        sheet.paste(floppy(col, i * 22.5), (410 + i * 34, 150 + (i % 3) * 12))
    sheet.convert('RGB').resize((1280, 800), Image.NEAREST).save(f'{out}/preview.png')
    big = new(96 * 2 + 8, 56); big.paste(dmv(0), (0, 0)); big.paste(dmv(3), (104, 0))
    big.convert('RGB').resize((200 * 6, 56 * 6), Image.NEAREST).save(f'{out}/dmv_zoom.png')

main(sys.argv[1] if len(sys.argv) > 1 else 'art')
