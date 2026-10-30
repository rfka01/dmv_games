#!/usr/bin/env python3
"""glyphen.py - Schriftzeichen als 1-Punkt-Linien (ASCII) fuer tiles_def.py.

Einmal benutzt, um die Zeichen der Zeichen-, Wind- und Drachensteine
vorzuzeichnen: Noto Sans CJK (SIL Open Font License) wird geglaettet
gerastert, ab einer Deckung als Flaeche genommen und auf Linien von
einem Punkt Breite verduennt (Skelett). Das Ergebnis steht in
tiles_def.py; zum Erzeugen des Steinsatzes braucht man die Schrift nicht.

  glyphen.py ZEICHEN GROESSE [SCHWELLE] [skelett|ohne]
"""
import sys
import numpy as np
from PIL import Image, ImageDraw, ImageFont
from skimage.morphology import skeletonize

FONT = '/usr/share/fonts/opentype/noto/NotoSansCJK-Regular.ttc'


def ecken(a):
    """Vom Skelett abgerundete Ecken wieder spitz: ein leerer Punkt mit
    je zwei gesetzten Punkten geradeaus in einer waagrechten und einer
    senkrechten Richtung wird gesetzt."""
    h, w = a.shape
    g = lambda y, x: 0 <= y < h and 0 <= x < w and a[y, x]
    b = a.copy()
    for y in range(h):
        for x in range(w):
            if a[y, x]:
                continue
            for dy in (-1, 1):
                for dx in (-1, 1):
                    if (g(y, x + dx) and g(y, x + 2 * dx)
                            and g(y + dy, x) and g(y + 2 * dy, x)
                            and not g(y + dy, x + dx)):
                        b[y, x] = True
    return b


def glyph(c, size, th=90, art='skelett'):
    f = ImageFont.truetype(FONT, size, index=0)
    im = Image.new('L', (48, 48), 0)
    d = ImageDraw.Draw(im)
    if art == 'ohne':
        im = Image.new('1', (48, 48), 0)
        d = ImageDraw.Draw(im)
        d.fontmode = '1'
        d.text((4, 2), c, font=f, fill=1)
        a = np.array(im).astype(bool)
    else:
        d.text((4, 2), c, font=f, fill=255)
        a = np.array(im) >= th
        a = ecken(skeletonize(a))
    ys, xs = np.nonzero(a)
    a = a[ys.min():ys.max() + 1, xs.min():xs.max() + 1]
    return [''.join('K' if v else '.' for v in row) for row in a]


if __name__ == '__main__':
    c, size = sys.argv[1], int(sys.argv[2])
    th = int(sys.argv[3]) if len(sys.argv) > 3 else 90
    art = sys.argv[4] if len(sys.argv) > 4 else 'skelett'
    for z in glyph(c, size, th, art):
        print(z)
