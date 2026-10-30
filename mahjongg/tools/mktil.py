#!/usr/bin/env python3
"""mktil.py - eigenen Steinsatz im TIL-Format erzeugen (MIT).

  mktil.py [ziel.TIL] [--png vorschau.png]

TIL-Format (wie Nels Andersons MAHJONGG.TIL): 42 Steine zu je 800 Byte,
40 x 40 Punkte, 4 Bit EGA-Farbe je Punkt, zwei Punkte je Byte (oberes
Halbbyte links), 20 Byte je Zeile.
"""
import argparse
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import tiles_def as T

W = H = 40
# EGA-Farbnummern
SCHWARZ, BLAU, GRUEN, CYAN, ROT, MAGENTA, BRAUN, HELLGRAU = range(8)
DUNKELGRAU, GELB, WEISS = 8, 14, 15
FARBE = {'K': SCHWARZ, 'B': BLAU, 'G': GRUEN, 'C': CYAN, 'R': ROT,
         'M': MAGENTA, 'Y': GELB, 'D': DUNKELGRAU, 'W': WEISS, 'O': BRAUN,
         'L': 10}                # L hellgruen

# Steinflaeche: x 4..35 (32 Punkte samt Kante, so liegen die Steine im
# Spiel auf 8-Punkt-Grenzen dicht an dicht), y 0..33; Seitenwand 4 Punkte
# nach links unten
FX0, FY0, FX1, FY1 = 4, 0, 35, 33
IX, IY = 5, 1                    # Innenflaeche 30 x 32 ab hier
IW = 30


class Stein:
    def __init__(self):
        self.p = [[SCHWARZ] * W for _ in range(H)]
        self.rahmen()

    def set(self, x, y, c):
        if 0 <= x < W and 0 <= y < H:
            self.p[y][x] = c

    def rahmen(self):
        # Seitenwand: Flaeche um k nach links unten verschoben, k = 1..4
        for k in range(4, 0, -1):
            for y in range(FY0, FY1 + 1):
                for x in range(FX0, FX1 + 1):
                    self.set(x - k, y + k, DUNKELGRAU)
        # Aussenkante der Wand
        for y in range(FY0 + 4, FY1 + 5):
            self.set(FX0 - 4, y, SCHWARZ)
        for x in range(FX0 - 4, FX1 - 3):
            self.set(x, FY1 + 4, SCHWARZ)
        for k in range(1, 4):
            self.set(FX0 - k, FY0 + k, SCHWARZ)          # Schraege oben
            self.set(FX1 - k, FY1 + k, SCHWARZ)          # Schraege unten
        # Flaeche mit schwarzer Kante
        for y in range(FY0, FY1 + 1):
            for x in range(FX0, FX1 + 1):
                rand = x in (FX0, FX1) or y in (FY0, FY1)
                self.set(x, y, SCHWARZ if rand else HELLGRAU)

    def raster(self, bild, x0, y0, z=1, farbe=None):
        for j, zeile in enumerate(bild):
            for i, ch in enumerate(zeile):
                if ch == '.':
                    continue
                c = FARBE[ch] if farbe is None else farbe
                for dy in range(z):
                    for dx in range(z):
                        self.set(x0 + i * z + dx, y0 + j * z + dy, c)

    def mitte(self, bild, y0=None, z=1, farbe=None):
        bw, bh = len(bild[0]) * z, len(bild) * z
        x0 = IX + (IW - bw) // 2
        if y0 is None:
            y0 = IY + (32 - bh) // 2
        self.raster(bild, x0, y0, z, farbe)

    def ecke(self, zeichen, farbe=SCHWARZ):
        self.raster(T.KLEIN[zeichen], FX1 - 4, FY0 + 1, 1, farbe)

    def schrift(self, text, y0, farbe):
        bw = len(text) * 6 - 1
        x0 = IX + (IW - bw) // 2
        for k, ch in enumerate(text):
            self.raster(T.BUCHST[ch], x0 + 6 * k, y0, 1, farbe)

    def scheibe(self, cx, cy, r, c):
        for y in range(cy - r, cy + r + 1):
            for x in range(cx - r, cx + r + 1):
                if (x - cx) ** 2 + (y - cy) ** 2 <= r * r + r:
                    self.set(x, y, c)

    def punkt(self, cx, cy, r, c):
        """Kreis wie auf den Punktsteinen: zwei feine Ringe, Kern."""
        self.ring(cx, cy, r, c)
        self.ring(cx, cy, r - 2, c)
        if r >= 5:
            self.set(cx, cy, c)

    def stab(self, x, y, h, c=GRUEN):
        """Bambusstab 4 Punkte breit, h hoch, Knoten oben, Mitte, unten."""
        for j in range(h):
            for i in range(4):
                self.set(x + i, y + j, c)
            self.set(x + 1, y + j, HELLGRAU if c != ROT else GELB)
        for j in (0, h // 2, h - 1):
            for i in range(4):
                self.set(x + i, y + j, SCHWARZ if c == GRUEN else c)

    # --- feine Grundformen (1 Punkt) ---
    def linie(self, x0, y0, x1, y1, c):
        dx, dy = abs(x1 - x0), -abs(y1 - y0)
        sx, sy = (1 if x0 < x1 else -1), (1 if y0 < y1 else -1)
        e = dx + dy
        while True:
            self.set(x0, y0, c)
            if x0 == x1 and y0 == y1:
                break
            e2 = 2 * e
            if e2 >= dy:
                e += dy
                x0 += sx
            if e2 <= dx:
                e += dx
                y0 += sy

    def ring(self, cx, cy, r, c):
        """Kreislinie, ein Punkt breit (Mittelpunkt auf einem Punkt)."""
        for yy in range(cy - r - 1, cy + r + 2):
            for xx in range(cx - r - 1, cx + r + 2):
                d = ((xx - cx) ** 2 + (yy - cy) ** 2) ** 0.5
                if abs(d - r) < 0.5:
                    self.set(xx, yy, c)

    def flaeche(self, pkt, c):
        """Vieleck fuellen (Mittelpunkte der Punkte innen)."""
        ys = [p[1] for p in pkt]
        for yy in range(int(min(ys)), int(max(ys)) + 1):
            xs = []
            n = len(pkt)
            for i in range(n):
                (xa, ya), (xb, yb) = pkt[i], pkt[(i + 1) % n]
                if (ya <= yy + 0.5 < yb) or (yb <= yy + 0.5 < ya):
                    xs.append(xa + (yy + 0.5 - ya) * (xb - xa) / (yb - ya))
            xs.sort()
            for k in range(0, len(xs) - 1, 2):
                for xx in range(int(round(xs[k])), int(round(xs[k + 1]))):
                    self.set(xx, yy, c)

    def bild1(self, zeilen, x0, y0, farbe):
        """1-Punkt-Zeichen (K = gesetzt) in einer Farbe."""
        for j, z in enumerate(zeilen):
            for i, ch in enumerate(z):
                if ch != '.':
                    self.set(x0 + i, y0 + j, farbe)

    def mitte1(self, zeilen, y0, farbe):
        self.bild1(zeilen, IX + (IW - len(zeilen[0])) // 2, y0, farbe)

    def daten(self):
        out = bytearray()
        for y in range(H):
            for x in range(0, W, 2):
                out.append((self.p[y][x] << 4) | self.p[y][x + 1])
        return bytes(out)


def x(i):      # Innenkoordinaten, entworfen fuer 32 Punkte Breite
    return IX + (i * (IW - 1) + 15) // 31


def y(j):
    return IY + j


def punkte(n):
    s = Stein()
    G, B, R = GRUEN, BLAU, ROT
    if n == 1:
        cx, cy = x(15), y(17)
        for r, c in ((13, B), (11, G), (9, R), (7, B), (5, G), (3, R)):
            s.ring(cx, cy, r, c)
        s.scheibe(cx, cy, 1, R)
        s.ecke('1')
        return s
    L = {
        2: [(16, 8, G), (16, 24, B)],
        3: [(7, 7, B), (16, 16, R), (25, 25, G)],
        4: [(9, 9, B), (23, 9, G), (9, 23, G), (23, 23, B)],
        5: [(8, 11, B), (24, 11, G), (16, 19, R), (8, 27, G), (24, 27, B)],
        6: [(11, 6, G), (21, 6, G), (11, 17, R), (21, 17, R),
            (11, 27, R), (21, 27, R)],
        7: [(6, 5, G), (16, 7, G), (26, 9, G), (10, 19, R), (22, 19, R),
            (10, 27, R), (22, 27, R)],
        8: [(10, 4, B), (22, 4, B), (10, 12, B), (22, 12, B),
            (10, 20, B), (22, 20, B), (10, 28, B), (22, 28, B)],
        9: [(6, 10, B), (16, 10, B), (26, 10, B), (6, 19, R), (16, 19, R),
            (26, 19, R), (6, 28, G), (16, 28, G), (26, 28, G)],
    }[n]
    r = 3 if n in (7, 8, 9) else 4 if n != 2 else 6
    for i, j, c in L:
        s.punkt(x(i), y(j), r, c)
    s.ecke(str(n))
    return s


def bambus(n):
    s = Stein()
    if n == 1:
        s.mitte(T.VOGEL, y0=y(6), z=1)
        s.ecke('1')
        return s
    R = ROT
    if n == 2:
        st = [(14, 2, 13), (14, 17, 13)]
    elif n == 3:
        st = [(14, 2, 13), (8, 17, 13), (20, 17, 13)]
    elif n == 4:
        st = [(8, 2, 13), (20, 2, 13), (8, 17, 13), (20, 17, 13)]
    elif n == 5:
        st = [(5, 2, 13), (22, 2, 13), (14, 10, 13, R), (5, 17, 13),
              (22, 17, 13)]
    elif n == 6:
        st = [(4, 2, 13), (13, 2, 13), (22, 2, 13), (4, 17, 13),
              (13, 17, 13), (22, 17, 13)]
    elif n == 7:
        st = [(14, 0, 9, R), (4, 11, 9), (14, 11, 9), (24, 11, 9),
              (4, 22, 9), (14, 22, 9), (24, 22, 9)]
    elif n == 8:
        st = [(0, 2, 13), (7, 2, 13), (15, 2, 13), (22, 2, 13),
              (0, 17, 13), (7, 17, 13), (15, 17, 13), (22, 17, 13)]
    else:
        st = [(4, 0, 9), (13, 0, 9, R), (22, 0, 9), (4, 11, 9),
              (13, 11, 9, R), (22, 11, 9), (4, 22, 9), (13, 22, 9, R),
              (22, 22, 9)]
    for e in st:
        c = e[3] if len(e) > 3 else GRUEN
        s.stab(x(e[0]), y(e[1]), e[2], c)
    s.ecke(str(n))
    return s


def zeichen(n):
    s = Stein()
    z = T.ZAHL1[n]
    s.mitte1(z, y(1) + (12 - len(z)) // 2, SCHWARZ)
    s.mitte1(T.WAN1, y(14), ROT)
    s.ecke(str(n))
    return s


def wind(w):
    s = Stein()
    g = T.WIND1[w]
    s.mitte1(g, IY + (32 - len(g)) // 2 + 1, SCHWARZ)
    s.ecke(w, BLAU)
    return s


def drache(art):
    s = Stein()
    if art == 'weiss':
        for (a, b, c, d) in ((x(3), y(2), x(28), y(29)),
                             (x(5), y(4), x(26), y(27)),
                             (x(9), y(8), x(22), y(23))):
            for i in range(a, c + 1):
                s.set(i, b, BLAU)
                s.set(i, d, BLAU)
            for j in range(b, d + 1):
                s.set(a, j, BLAU)
                s.set(c, j, BLAU)
    elif art == 'rot':
        g = T.ZHONG1
        s.mitte1(g, IY + (32 - len(g)) // 2, ROT)
        s.ecke('C', ROT)
    else:
        s.mitte(T.GRUENDRACHE, y0=y(1), z=1)
    return s


# --- Jahreszeiten und Blumen, fein gezeichnet (Feld 30 x 22 ab IX, IY) ---
import math


def strahl(s, cx, cy, w, r0, r1, c):
    a = math.radians(w)
    s.linie(cx + round(r0 * math.cos(a)), cy - round(r0 * math.sin(a)),
            cx + round(r1 * math.cos(a)), cy - round(r1 * math.sin(a)), c)


def sommer(s):                  # Sonne
    cx, cy = IX + 14, IY + 11
    s.scheibe(cx, cy, 5, GELB)
    s.ring(cx, cy, 6, ROT)
    for k in range(12):
        strahl(s, cx, cy, k * 30, 8, 11 if k % 2 == 0 else 9, ROT)


def winter(s):                  # Schneeflocke
    cx, cy = IX + 14, IY + 11
    for k in range(6):
        w = 90 + k * 60
        strahl(s, cx, cy, w, 2, 10, BLAU)
        for d in (6,):
            a = math.radians(w)
            px, py = cx + d * math.cos(a), cy - d * math.sin(a)
            for dw in (-45, 45):
                b = math.radians(w + 180 + dw)
                s.linie(round(px), round(py), round(px + 3 * math.cos(b)),
                        round(py - 3 * math.sin(b)), BLAU)
    s.scheibe(cx, cy, 1, BLAU)


def bluete(s, cx, cy, c, kern=GELB):
    for w in (90, 162, 234, 306, 18):
        a = math.radians(w)
        s.scheibe(cx + round(2 * math.cos(a)), cy - round(2 * math.sin(a)),
                  1, c)
    s.set(cx, cy, kern)


def fruehling(s):               # Kirschzweig
    s.linie(IX + 2, IY + 21, IX + 12, IY + 12, BRAUN)
    s.linie(IX + 12, IY + 12, IX + 26, IY + 4, BRAUN)
    s.linie(IX + 12, IY + 12, IX + 14, IY + 4, BRAUN)
    s.linie(IX + 18, IY + 9, IX + 21, IY + 14, BRAUN)
    for (bx, by) in ((14, 3), (25, 4), (21, 15), (7, 15)):
        bluete(s, IX + bx, IY + by, MAGENTA)


def herbst(s):                  # Ahornblatt
    cx, cy = IX + 14, IY + 12
    pkt = [(0, -11), (2, -6), (5, -8), (5, -4), (10, -6), (8, -1), (11, 1),
           (6, 3), (7, 6), (2, 5), (1, 8), (-1, 8), (-2, 5), (-7, 6),
           (-6, 3), (-11, 1), (-8, -1), (-10, -6), (-5, -4), (-5, -8),
           (-2, -6)]
    s.flaeche([(cx + a + 0.5, cy + b) for a, b in pkt], ROT)
    for (a, b) in ((0, -9), (8, -4), (-8, -4), (5, 4), (-5, 4)):
        s.linie(cx, cy + 4, cx + a, cy + b, HELLGRAU)
    s.linie(cx, cy + 4, cx, cy + 10, BRAUN)


def chrysantheme(s):
    cx, cy = IX + 14, IY + 8
    s.linie(cx, cy + 6, cx, IY + 21, GRUEN)
    s.linie(cx, IY + 17, cx - 5, IY + 13, GRUEN)
    s.linie(cx, IY + 19, cx + 5, IY + 15, GRUEN)
    for k in range(20):
        strahl(s, cx, cy, k * 18, 3, 8, GELB)
        strahl(s, cx, cy, k * 18 + 9, 3, 6, ROT)
    s.scheibe(cx, cy, 2, BRAUN)


def pflaume(s):
    cx, cy = IX + 15, IY + 9
    s.linie(IX + 3, IY + 21, cx - 2, cy + 3, BRAUN)
    s.linie(IX + 9, IY + 15, IX + 4, IY + 11, BRAUN)
    for k in range(5):
        a = math.radians(90 + k * 72)
        px, py = cx + round(5 * math.cos(a)), cy - round(5 * math.sin(a))
        s.scheibe(px, py, 3, ROT)
        s.ring(px, py, 3, MAGENTA)
    s.scheibe(cx, cy, 2, GELB)
    for k in range(5):
        strahl(s, cx, cy, 54 + k * 72, 2, 4, ROT)


def orchidee(s):
    for (bx, by) in ((9, 5), (21, 9)):
        cx, cy = IX + bx, IY + by
        for w in (90, 210, 330):
            strahl(s, cx, cy, w, 1, 4, MAGENTA)
            strahl(s, cx, cy, w + 12, 1, 4, MAGENTA)
            strahl(s, cx, cy, w - 12, 1, 4, MAGENTA)
        for w in (30, 150, 270):
            strahl(s, cx, cy, w, 1, 3, MAGENTA)
        s.set(cx, cy, GELB)
    s.linie(IX + 9, IY + 9, IX + 13, IY + 21, GRUEN)
    s.linie(IX + 21, IY + 13, IX + 15, IY + 21, GRUEN)
    s.linie(IX + 13, IY + 21, IX + 3, IY + 14, GRUEN)
    s.linie(IX + 15, IY + 21, IX + 27, IY + 16, GRUEN)
    s.linie(IX + 14, IY + 21, IX + 6, IY + 19, GRUEN)


def bambusblume(s):
    x0 = IX + 8
    for j in range(IY + 1, IY + 22):
        s.set(x0, j, GRUEN)
        s.set(x0 + 1, j, GRUEN)
    for j in (IY + 6, IY + 12, IY + 18):
        s.set(x0 - 1, j, GRUEN)
        s.set(x0 + 2, j, GRUEN)
    for (y0, l, r) in ((IY + 3, 0, 1), (IY + 9, 1, 1), (IY + 15, 1, 0)):
        if r:
            s.linie(x0 + 2, y0 + 3, x0 + 8, y0, GRUEN)
            s.linie(x0 + 8, y0, x0 + 18, y0 - 1, GRUEN)
            s.linie(x0 + 3, y0 + 3, x0 + 16, y0 + 1, GRUEN)
        if l:
            s.linie(x0 - 1, y0 + 3, x0 - 6, y0, GRUEN)
            s.linie(x0 - 6, y0, x0 - 7, y0 - 2, GRUEN)


def bild(maler, text):
    s = Stein()
    maler(s)
    s.schrift(text, y(24), ROT)
    return s


def satz():
    st = [punkte(n) for n in range(1, 10)]
    st.append(drache('weiss'))
    st += [bambus(n) for n in range(1, 10)]
    st.append(drache('gruen'))
    st += [zeichen(n) for n in range(1, 10)]
    st.append(drache('rot'))
    st += [wind(w) for w in 'NSEW']
    st += [bild(b, t) for b, t in ((sommer, 'SUM'), (winter, 'WIN'),
                                   (fruehling, 'SPR'), (herbst, 'AUT'))]
    st += [bild(b, t) for b, t in ((chrysantheme, 'MUM'), (pflaume, 'PLUM'),
                                   (orchidee, 'ORC'), (bambusblume, 'BAM'))]
    assert len(st) == 42
    return st


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('ziel', nargs='?', default='DMVSTEIN.TIL')
    ap.add_argument('--png')
    a = ap.parse_args()
    st = satz()
    daten = b''.join(s.daten() for s in st)
    assert len(daten) == 33600
    with open(a.ziel, 'wb') as f:
        f.write(daten)
    print('%s: %d Byte' % (a.ziel, len(daten)))
    if a.png:
        from vorschau import til_png
        til_png(daten, a.png)


if __name__ == '__main__':
    main()
