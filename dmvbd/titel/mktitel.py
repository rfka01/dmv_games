#!/usr/bin/env python3
"""Startschirm von DMVBD "Before Dawn": naechtliche Insel unter Sternen,
ein roter Streifen Morgenrot am Horizont, der Titel in warmen Farben.
Erzeugt titel.png (8 DMV-Farben, geordnet gerastert), mono.png (Helligkeit
gerastert) und vorschau.png. Der Zwinkerstern wird vom Programm gezeichnet
(Lage: STERN unten); an seiner Stelle bleibt der Himmel schwarz.
Eigene Zeichnung, Schrift: DejaVu Serif Bold.
Code: Claude, Prompts: rfka01"""
from PIL import Image, ImageDraw, ImageFont, ImageFilter
import math, random, sys, os

W, H = 640, 400
SY = 400 / 480            # DMV-Punkte sind hoeher als breit
HOR = 262                 # Horizont
STERN = (528, 34)         # Mitte des Zwinkersterns (gerade Koordinaten)
FONT = '/usr/share/fonts/truetype/dejavu/DejaVuSerif-Bold.ttf'

def lerp(a, b, t): return tuple(int(a[i] + (b[i] - a[i]) * t) for i in range(3))

def szene():
    im = Image.new('RGB', (W, H)); px = im.load()
    # Himmel: oben schwarz, zum Horizont tiefblau, darueber ein Hauch Violett,
    # direkt am Horizont der rote Streifen
    for y in range(HOR):
        t = y / HOR
        if t < 0.55: c = lerp((0, 0, 8), (10, 20, 120), t / 0.55)
        elif t < 0.86: c = lerp((10, 20, 120), (60, 40, 170), (t - 0.55) / 0.31)
        elif t < 0.95: c = lerp((60, 40, 170), (200, 30, 90), (t - 0.86) / 0.09)
        else: c = lerp((200, 30, 90), (255, 40, 20), (t - 0.95) / 0.05)
        for x in range(W): px[x, y] = c
    # Meer
    for y in range(HOR, H):
        t = (y - HOR) / (H - HOR)
        c = lerp((30, 20, 110), (0, 0, 30), min(1, t * 1.3))
        for x in range(W): px[x, y] = c
    d = ImageDraw.Draw(im)
    # Wolkenschleier (lange flache Ellipsen)
    wolke = Image.new('L', (W, H)); dw = ImageDraw.Draw(wolke)
    for (x, y, rx, ry) in ((120, 150, 150, 10), (460, 120, 170, 9), (300, 200, 220, 8), (560, 190, 120, 7),
                           (60, 215, 110, 6)):
        dw.ellipse([x - rx, y - ry * SY * 1.2, x + rx, y + ry * SY * 1.2], fill=110)
    wolke = wolke.filter(ImageFilter.GaussianBlur(6))
    im = Image.composite(Image.new('RGB', (W, H), (70, 40, 210)), im, wolke)
    d = ImageDraw.Draw(im)
    # Sterne (nicht im Titelbereich und nicht beim Zwinkerstern)
    rnd = random.Random(7)
    for i in range(90):
        x, y = rnd.randrange(W), rnd.randrange(int(HOR * 0.7))
        if abs(x - STERN[0]) < 20 and abs(y - STERN[1]) < 16: continue
        if 120 < x < 520 and 40 < y < 190: continue
        c = rnd.choice([(255, 255, 255), (255, 255, 160), (160, 220, 255)])
        d.point((x, y), fill=c)
        if rnd.random() < 0.15: d.point([(x - 1, y), (x + 1, y)], fill=c)
    # ferne Berge am Horizont (blauschwarz)
    pts = [(0, HOR)]
    for x in range(0, W + 1, 8):
        h = 10 + 8 * math.sin(x / 47) + 6 * math.sin(x / 19 + 1) + 4 * math.sin(x / 7)
        if 360 < x < 520: h += 14 * math.sin((x - 360) / 160 * math.pi)
        pts.append((x, HOR - h * SY))
    pts.append((W, HOR))
    d.polygon(pts, fill=(20, 20, 80))
    # die Insel: zwei Gipfel, rechts flach auslaufend
    ins = [(150, HOR + 2)]
    for x in range(150, 470, 4):
        u = (x - 150) / 320
        h = 150 * math.exp(-((u - 0.32) / 0.16) ** 2) + 95 * math.exp(-((u - 0.58) / 0.13) ** 2) \
            + 30 * math.exp(-((u - 0.8) / 0.12) ** 2) + 3 * math.sin(x / 5)
        ins.append((x, HOR - h * SY))
    ins.append((470, HOR + 2))
    d.polygon(ins, fill=(8, 8, 40))
    # Mondlicht/Morgenlicht an den Kanten links (heller Rand)
    for (x, y) in ins[1:-1]:
        d.point((x - 1, y + 1), fill=(60, 60, 170))
    # Leuchtturm auf dem kleinen Gipfel rechts
    lx = 150 + int(0.8 * 320); ly = HOR - int(33 * SY)
    d.polygon([(lx - 3, ly), (lx + 3, ly), (lx + 2, ly - 22), (lx - 2, ly - 22)], fill=(230, 230, 230))
    for k in range(3): d.line([(lx - 3, ly - 4 - 7 * k), (lx + 3, ly - 4 - 7 * k)], fill=(220, 30, 30))
    d.rectangle([lx - 3, ly - 27, lx + 3, ly - 23], fill=(255, 255, 120))
    d.polygon([(lx - 4, ly - 27), (lx + 4, ly - 27), (lx, ly - 32)], fill=(200, 30, 30))
    # Dorf am Fuss: Lichter
    for (x, y) in ((205, 4), (214, 2), (226, 5), (238, 3), (250, 6), (262, 4), (230, -6), (246, -4), (220, -10)):
        d.rectangle([x, HOR - 6 + y - 2, x + 1, HOR - 6 + y], fill=(255, 230, 60))
    # Palmen links auf der Insel
    for (px0, h) in ((172, 30), (184, 24)):
        base = HOR - 2
        top = (px0 + 6, base - h)
        d.line([(px0, base), top], fill=(4, 4, 20), width=2)
        for a in (-150, -120, -60, -30, 10, 200):
            r = 12
            d.line([top, (top[0] + r * math.cos(math.radians(a)), top[1] + r * math.sin(math.radians(a)) * SY + 4)],
                   fill=(4, 4, 20), width=2)
    # Spiegelung im Meer: rote Streifen und Lichter
    rnd = random.Random(3)
    for y in range(HOR + 2, H, 3):
        t = (y - HOR) / (H - HOR)
        for k in range(int(10 * (1 - t)) + 2):
            x = rnd.randrange(W); l = rnd.randrange(8, 40)
            c = (220, 40, 40) if t < 0.25 else (60, 60, 170)
            d.line([(x, y), (x + l, y)], fill=c)
    for (x, l) in ((lx, 6), (230, 30)):
        for y in range(HOR + 4, HOR + 60, 4):
            d.line([(x - l // 2 + rnd.randrange(-3, 4), y), (x + l // 2 + rnd.randrange(-3, 4), y)], fill=(230, 210, 80))
    # Schiff links auf dem Wasser
    sx, sy = 70, HOR + 22
    d.polygon([(sx - 16, sy), (sx + 16, sy), (sx + 11, sy + 6), (sx - 12, sy + 6)], fill=(10, 10, 30))
    d.line([(sx, sy), (sx, sy - 30)], fill=(10, 10, 30), width=2)
    d.polygon([(sx + 2, sy - 28), (sx + 14, sy - 10), (sx + 2, sy - 8)], fill=(200, 200, 220))
    d.polygon([(sx - 2, sy - 24), (sx - 13, sy - 8), (sx - 2, sy - 6)], fill=(180, 180, 210))
    return im

def bogentext(im, text, size, cy, bogen, oben, unten, rand=4):
    """Text auf flachem Bogen, Fuellung von oben nach unten oben->unten,
    schwarzer Rand"""
    f = ImageFont.truetype(FONT, size)
    breiten = [f.getlength(c) for c in text]
    ges = sum(breiten) + 2 * (len(text) - 1)
    x = W / 2 - ges / 2
    lay = Image.new('L', (W, H)); dl = ImageDraw.Draw(lay)
    for c, b in zip(text, breiten):
        mx = x + b / 2
        u = (mx - W / 2) / (ges / 2)
        y = cy + bogen * u * u
        # Buchstabe einzeln, leicht gedreht entlang des Bogens
        g = Image.new('L', (size * 2, size * 2)); dg = ImageDraw.Draw(g)
        dg.text((size, size), c, font=f, fill=255, anchor='mm')
        g = g.resize((size * 2, int(size * 2 * SY * 1.15)))
        g = g.rotate(-math.degrees(math.atan(2 * bogen * u / (ges / 2))), resample=Image.BICUBIC)
        lay.paste(255, (int(mx - g.width / 2), int(y - g.height / 2)), g)
        x += b + 2
    lay = lay.point(lambda v: 255 if v > 110 else 0)
    umriss = lay.filter(ImageFilter.MaxFilter(2 * rand + 1))
    bb = lay.getbbox()
    verlauf = Image.new('RGB', (W, H)); dv = ImageDraw.Draw(verlauf)
    for y in range(H):
        t = 0 if y <= bb[1] else 1 if y >= bb[3] else (y - bb[1]) / (bb[3] - bb[1])
        dv.line([(0, y), (W, y)], fill=lerp(oben, unten, t))
    im.paste((0, 0, 0), (0, 0), umriss)
    im.paste(verlauf, (0, 0), lay)
    # Glanzkante oben
    glanz = Image.new('L', (W, H))
    glanz.paste(lay, (0, 0)); glanz2 = Image.new('L', (W, H)); glanz2.paste(lay, (0, 2))
    from PIL import ImageChops
    kante = ImageChops.subtract(glanz, glanz2)
    im.paste((255, 255, 220), (0, 0), kante)

def banner(im, text, cy, band=(255, 255, 0), enden=(255, 0, 0)):
    d = ImageDraw.Draw(im)
    f = ImageFont.truetype(FONT, 15)
    tw = f.getlength(text)
    x0, x1 = W / 2 - tw / 2 - 16, W / 2 + tw / 2 + 16
    h = 20
    # Enden (gefaltet), dann das Band, leicht gebogen
    for s in (-1, 1):
        xe = x0 if s < 0 else x1
        d.polygon([(xe, cy - h / 2 + 6), (xe + s * 30, cy - h / 2 + 6), (xe + s * 22, cy + 6 + 2), (xe + s * 30, cy + h / 2 + 8),
                   (xe, cy + h / 2 + 8)], fill=enden, outline=(0, 0, 0))
    d.polygon([(x0, cy - h / 2), (x1, cy - h / 2), (x1, cy + h / 2), (x0, cy + h / 2)], fill=band, outline=(0, 0, 0))
    d.text((W / 2, cy), text, font=f, fill=(0, 0, 0), anchor='mm')

def raster(im):
    """jede Farbe einzeln gegen eine 4x4-Bayer-Matrix -> 8 DMV-Farben"""
    B = [[0, 8, 2, 10], [12, 4, 14, 6], [3, 11, 1, 9], [15, 7, 13, 5]]
    out = Image.new('RGB', im.size); po = out.load(); pi = im.load()
    for y in range(im.height):
        for x in range(im.width):
            t = (B[y & 3][x & 3] + 0.5) * 16
            po[x, y] = tuple(255 if v > t else 0 for v in pi[x, y])
    return out

def mono(im):
    B = [[0, 8, 2, 10], [12, 4, 14, 6], [3, 11, 1, 9], [15, 7, 13, 5]]
    g = im.convert('L'); pg = g.load()
    out = Image.new('L', im.size); po = out.load()
    for y in range(im.height):
        for x in range(im.width):
            v = int(255 * (pg[x, y] / 255) ** 1.7)
            po[x, y] = 255 if v > (B[y & 3][x & 3] + 0.5) * 16 else 0
    return out

def bild(farbig):
    im = szene()
    if farbig:
        bogentext(im, 'BEFORE', 40, 70, 10, (255, 255, 0), (255, 140, 0))
        bogentext(im, 'DAWN', 84, 132, 16, (255, 255, 0), (255, 30, 0), rand=5)
        banner(im, 'Bildschirmschoner für die NCR DMV', 190)
    else:                     # Mono: helle Schrift, weisses Band
        bogentext(im, 'BEFORE', 40, 70, 10, (255, 255, 255), (230, 230, 230))
        bogentext(im, 'DAWN', 84, 132, 16, (255, 255, 255), (200, 200, 200), rand=5)
        banner(im, 'Bildschirmschoner für die NCR DMV', 190, (255, 255, 255), (170, 170, 170))
    d = ImageDraw.Draw(im)
    f = ImageFont.truetype('/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf', 11)
    d.text((W - 6, H - 4), 'Code: Claude  Prompts: rfka01', font=f,
           fill=(0, 255, 255) if farbig else (255, 255, 255), anchor='rd')
    return im

def main(aus):
    im = bild(True)
    r = raster(im); r.save(f'{aus}/titel.png')
    m = mono(bild(False)); m.save(f'{aus}/mono.png')
    # Hintergrund unter dem Zwinkerstern fuer DMVBD (21 x 21 ab STERN - 10)
    x0, y0 = STERN[0] - 10, STERN[1] - 10
    pr, pm = r.load(), m.load()
    with open(f'{aus}/../src/STERNBG.H', 'w', newline='\r\n') as f:
        f.write('/* STERNBG.H - Hintergrund unter dem Zwinkerstern des Startschirms,\n'
                '   erzeugt von titel/mktitel.py. Bit x = Punkt x des 21x21-Felds. */\n')
        f.write('static const unsigned long bg_farbe[3][21] = {\n')
        for e, k in ((0, 1), (1, 0), (2, 2)):       # Ebenen G, R, B
            f.write('  { ' + ', '.join('0x%06lXL' % sum(1 << x for x in range(21) if pr[x0 + x, y0 + y][k] > 127)
                                      for y in range(21)) + ' },\n')
        f.write('};\nstatic const unsigned long bg_mono[21] = {\n  ' +
                ', '.join('0x%06lXL' % sum(1 << x for x in range(21) if pm[x0 + x, y0 + y] > 127) for y in range(21)) + ' };\n')
    v = r.copy(); dv = ImageDraw.Draw(v)
    x, y = STERN
    dv.line([(x - 8, y), (x + 8, y)], fill=(255, 255, 255)); dv.line([(x, y - 7), (x, y + 7)], fill=(255, 255, 255))
    v.resize((1280, 960), Image.NEAREST).save(f'{aus}/vorschau.png')
    mm = Image.merge('RGB', (Image.new('L', m.size), m, Image.new('L', m.size)))
    mm.resize((1280, 960), Image.NEAREST).save(f'{aus}/vorschau_mono.png')

main(sys.argv[1] if len(sys.argv) > 1 else '.')
