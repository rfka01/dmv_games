#!/usr/bin/env python3
"""vorschau.py - TIL-Datei als PNG: EGA-Farben, DMV-Farbe, DMV-Mono.

  vorschau.py STEINE.TIL bild.png [--brett brett.png]

Die Umsetzung auf die DMV ist dieselbe wie im Spiel (mahjongg.c, til_lade):
  Farbe:  EGA 0-5 und 9-15 auf die acht DMV-Farben (hell wie dunkel),
          7 Hellgrau -> Weiss, 8 Dunkelgrau -> Raster Blau/Schwarz,
          6 Braun -> Raster Rot/Gelb.
  Mono:   7 und 15 leuchten, 8 Dunkelgrau als Raster, alles andere dunkel.
Das Raster richtet sich nach (x + y) & 1 im Stein.
"""
import sys
from PIL import Image

EGA = [(0, 0, 0), (0, 0, 170), (0, 170, 0), (0, 170, 170), (170, 0, 0),
       (170, 0, 170), (170, 85, 0), (170, 170, 170), (85, 85, 85),
       (85, 85, 255), (85, 255, 85), (85, 255, 255), (255, 85, 85),
       (255, 85, 255), (255, 255, 85), (255, 255, 255)]
# DMV-Farbnummern wie DMVGFX.INC: 1 blau, 2 gruen, 4 rot
DMVRGB = {0: (0, 0, 0), 1: (0, 0, 255), 2: (0, 255, 0), 3: (0, 255, 255),
          4: (255, 0, 0), 5: (255, 0, 255), 6: (255, 255, 0),
          7: (255, 255, 255)}
GRUEN = (40, 255, 90)


def dmv_farbe(v, x, y):
    ck = (x + y) & 1
    t = {0: 0, 1: 1, 2: 2, 3: 3, 4: 4, 5: 5, 6: 4 if ck else 6, 7: 7,
         8: 1 if ck else 0, 9: 1, 10: 2, 11: 3, 12: 4, 13: 5, 14: 6, 15: 7}
    return t[v]


def dmv_mono(v, x, y):
    if v in (7, 15):
        return 1
    if v == 8:
        return (x + y) & 1
    return 0


def pixel(daten, t, x, y):
    b = daten[t * 800 + y * 20 + x // 2]
    return b >> 4 if x % 2 == 0 else b & 15


def til_png(daten, ziel, z=3):
    im = Image.new('RGB', (14 * 44, 9 * 44), (40, 40, 40))
    for t in range(42):
        for row, art in enumerate(('ega', 'farbe', 'mono')):
            ox = (t % 14) * 44 + 2
            oy = (row * 3 + t // 14) * 44 + 2
            for y in range(40):
                for x in range(40):
                    v = pixel(daten, t, x, y)
                    if art == 'ega':
                        c = EGA[v]
                    elif art == 'farbe':
                        c = DMVRGB[dmv_farbe(v, x, y)]
                    else:
                        c = GRUEN if dmv_mono(v, x, y) else (0, 0, 0)
                    im.putpixel((ox + x, oy + y), c)
    im = im.resize((im.width * z, im.height * z), Image.NEAREST)
    im.save(ziel)
    print(ziel)


if __name__ == '__main__':
    d = open(sys.argv[1], 'rb').read()
    til_png(d, sys.argv[2])
