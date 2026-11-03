"""hopbild.py - HOPPLERs Titelstreifen am PC nachbilden (Wasser, Seerosen,
Figuren, Text), um daraus deckende Bilder mit fertigem Hintergrund fuer die
Titel-Animation zu schneiden. Bildschirm als Liste von Zeilen mit Farbe
0-7 je Punkt (1 blau, 2 gruen, 4 rot).

Code: Claude (Anthropic), Prompts: rfka01. Lizenz: MIT"""
import os, struct, sys
HIER = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HIER)
import sprites_def as SD
from font_def import F, cell

PATROW = [(0x55, 0xAA, 0x55, 0xAA), (0xFF, 0xEE, 0x55, 0xBB),
          (0x11, 0x00, 0x44, 0x00), (0x01, 0x00, 0x10, 0x00)]
MAXS, RINGBASE, NIMG = 21, 21, 42
COL = {'K': 0, 'B': 1, 'G': 2, 'C': 3, 'R': 4, 'M': 5, 'Y': 6, 'W': 7}

def ims(n): return n if n <= MAXS else n - RINGBASE
def rxof(s): return (s * 5) // 3

def sizes():
    """ImA, ImW, ImRec je (N, V) wie ImgSizes in HOPPLER.PAS"""
    t = {}
    for n in range(NIMG + 1):
        s = ims(n); l = rxof(s + 1) + 2
        for v in (0, 1):
            al = (l - 8 * v + 15) // 16
            ar = max(0, (l - (15 - 8 * v) + 15) // 16)
            w = al + 1 + ar
            t[n, v] = (al, w, (4 * (2 * s + 5) * w * 2 + 127) // 128)
    return t

def lade_dat(pfad):
    """altes HOPPLER.DAT: je Bild Worte [Ebene M/G/R/B][Zeile][Wort]"""
    d = open(pfad, 'rb').read()
    assert d[:11] == b'HOPPLER 0.6', 'falsche Kennung'
    pos = 128; t = sizes(); bilder = {}
    for n in range(NIMG + 1):
        for v in (0, 1):
            al, w, rec = t[n, v]
            r = 2 * ims(n) + 5
            ws = struct.unpack('<%dH' % (4 * r * w), d[pos:pos + 8 * r * w])
            bilder[n, v] = (al, w, r, ws)
            pos += rec * 128
    return bilder

def _halfw(y, rx, ry):
    if rx <= 0 or ry <= 0 or abs(y) > ry: return -1
    t = 1.0 - (y / (ry + 0.5)) ** 2
    if t < 0: t = 0
    import math
    return int(math.floor(rx * math.sqrt(t) + 0.5))

def _spanw(k, a, b):
    l = a - 16 * k; h = b - 16 * k
    l = max(l, 0); h = min(h, 15)
    if l > h: return 0
    return ((1 << (h + 1)) - 1) & ~((1 << l) - 1) & 0xFFFF

def _span2(k, c, h):
    return _spanw(k, c - h, c + h) if h >= 0 else 0

def mache_bilder():
    """Seerosen und Ringe berechnen wie MakeImages in HOPPLER bis V0.18"""
    t = sizes(); bilder = {}
    for n in range(NIMG + 1):
        s = ims(n); rx = rxof(s); r = 2 * s + 5
        for v in (0, 1):
            al, w, rec = t[n, v]
            c = 16 * al + 8 * v
            ws = [0] * (4 * r * w)
            for y in range(r):
                dy = y - s - 2
                hm = _halfw(dy, rxof(s + 1) + 1, s + 1) if n <= MAXS else _halfw(dy, rx + 1, s)
                wv = PATROW[1][dy & 3]; wv |= wv << 8
                ck = PATROW[0][dy & 3]; ck |= ck << 8
                if n <= MAXS:
                    ho = _halfw(dy, rx, s)
                    hi = _halfw(dy, rx - 2, s - 1) if (s > 2 and ho >= 0) else -1
                else:
                    ho = _halfw(dy, rx, s); hi = _halfw(dy, rx - 2, s - 1)
                    if s // 2 >= 3:
                        ho2 = _halfw(dy, rxof(s // 2), s // 2); hi2 = _halfw(dy, rxof(s // 2) - 2, s // 2 - 1)
                    else:
                        ho2 = hi2 = -1
                for k in range(w):
                    if n <= MAXS:
                        mo = _span2(k, c, ho); mi = _span2(k, c, hi)
                        rim = mo & ~mi; ol = mi & ck
                        g = rim | ol; rd = ol; b = rim | (wv & ~mo)
                    else:
                        rim = (_span2(k, c, ho) & ~_span2(k, c, hi)) | (_span2(k, c, ho2) & ~_span2(k, c, hi2))
                        g = rim; rd = 0; b = rim | wv
                    mk = _span2(k, c, hm)
                    ws[(0 * r + y) * w + k] = mk & 0xFFFF
                    ws[(1 * r + y) * w + k] = g & mk & 0xFFFF
                    ws[(2 * r + y) * w + k] = rd & mk & 0xFFFF
                    ws[(3 * r + y) * w + k] = b & mk & 0xFFFF
            bilder[n, v] = (al, w, r, tuple(ws))
    return bilder

class Schirm:
    def __init__(self, f=0):
        self.p = [[f] * 640 for _ in range(400)]
    def dbox(self, x, y, w, h, ca, cb, pat):
        for yy in range(y, y + h):
            r = PATROW[pat][yy & 3]
            for xx in range(x, x + w):
                self.p[yy][xx] = ca if (r >> (xx & 7)) & 1 else cb
    def blit(self, bilder, n, cx, cy):
        v = (cx >> 3) & 1
        al, w, r, ws = bilder[n, v]
        x0 = ((cx >> 4) - al) * 16; y0 = cy - ims(n) - 2
        for y in range(r):
            for k in range(w):
                m = ws[(0 * r + y) * w + k]
                g = ws[(1 * r + y) * w + k]; rd = ws[(2 * r + y) * w + k]; b = ws[(3 * r + y) * w + k]
                for i in range(16):
                    if (m >> i) & 1:
                        xx = x0 + k * 16 + i
                        if 0 <= xx < 640:
                            self.p[y0 + y][xx] = ((g >> i) & 1) * 2 + ((rd >> i) & 1) * 4 + ((b >> i) & 1)
    def sprite(self, rows, x, y, z):
        k = z + 1
        for sy, row in enumerate(rows):
            for sx, c in enumerate(row):
                if c in '. ': continue
                if c.islower(): c = c.upper() if (sx + sy) % 2 == 0 else 'K'
                for dy in range(k):
                    for dx in range(k):
                        self.p[y + sy * k + dy][x + sx * k + dx] = COL[c]
    def text(self, s, x, y, col):
        for ch in s:
            g = cell(F[ch])
            for yy in range(16):
                for xx in range(8):
                    if (g[yy] >> xx) & 1: self.p[y + yy][x + xx] = col
            x += 8
    def ppm(self, pfad, y0=0, y1=400):
        rgb = {c: ((255 if c & 4 else 0), (255 if c & 2 else 0), (255 if c & 1 else 0)) for c in range(8)}
        with open(pfad, 'wb') as f:
            f.write(b'P6\n640 %d\n255\n' % (y1 - y0))
            for y in range(y0, y1):
                f.write(bytes(v for c in self.p[y] for v in rgb[c]))

COIN = None
def coin_rows():
    import importlib.util
    spec = importlib.util.spec_from_file_location('mks', os.path.join(HIER, 'mksprites.py'))
    src = open(os.path.join(HIER, 'mksprites.py')).read()
    ns = {}
    exec(src.split('def masks')[0].split('from sprites_def import *')[1], ns)
    return ns['COIN']

TY = 144
TS = (17, 19, 21, 13, 9, 5, 0, 11, 16, 17)
def titel(bilder, lib=True, vogel=True, muenze=True):
    """Titelstreifen y 104..175 wie DrawTitle"""
    s = Schirm()
    s.dbox(0, 104, 640, 72, 1, 0, 1)
    for i in range(10):
        if i == 6: s.blit(bilder, RINGBASE + 14, 32 + i * 64, TY)
        elif i == 9: s.blit(bilder, TS[i], 600, TY)
        else: s.blit(bilder, TS[i], 32 + i * 64, TY)
    if lib: s.sprite(SD.LIB_F, 16, TY - 26, 1)
    if vogel: s.sprite(SD.DUMMVOGEL, 144, TY - 26, 1)
    if muenze:
        s.sprite(coin_rows(), 584, TY - 22, 1)
        s.text('1', 596, TY - 14, 0)
    return s
