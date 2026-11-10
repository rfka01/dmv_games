#!/usr/bin/env python3
"""Schriftzug und Ressourcen von DMVWARP.AD: "DECISION MATE V" in duenner
eigener Schrift (DejaVu Sans ExtraLight), das V gross; dazu Einstellungen
und Texte. Code: Claude, Prompts: rfka01"""
from PIL import Image, ImageDraw, ImageFont
import struct, os
os.makedirs('res', exist_ok=True)
F = '/usr/share/fonts/truetype/dejavu/DejaVuSans-ExtraLight.ttf'

# Eigene Strichschrift: Grossbuchstaben 10 x 16 Punkte, 1 Punkt stark,
# alle Endpunkte auf ganzen Punkten - senkrechte und waagrechte Kanten
# liegen genau auf Punktspalten und -zeilen.
def buchstabe(d, ch, x, y, h=16):
    w = 10; b = y + h - 1; m = y + h // 2
    L = lambda *p: d.line([(x + a, y + c) for a, c in p], fill=255)
    if ch == 'D':
        L((0, 0), (0, h - 1)); L((0, 0), (5, 0)); L((0, h - 1), (5, h - 1))
        d.arc([x + 1, y, x + 9, y + h - 1], 270, 90, fill=255)
    elif ch == 'E':
        L((0, 0), (0, h - 1)); L((0, 0), (8, 0)); L((0, h // 2 - 1), (6, h // 2 - 1)); L((0, h - 1), (8, h - 1))
    elif ch == 'C':
        d.arc([x, y, x + 10, y + h - 1], 45, 315, fill=255)
    elif ch == 'I':
        L((0, 0), (0, h - 1)); w = 1
    elif ch == 'S':
        d.arc([x, y, x + 8, y + h // 2], 90, 360, fill=255)
        d.arc([x, y + h // 2 - 1, x + 8, y + h - 1], 270, 180, fill=255)
        w = 9
    elif ch == 'O':
        d.ellipse([x, y, x + 10, y + h - 1], outline=255); w = 11
    elif ch == 'N':
        L((0, 0), (0, h - 1)); L((9, 0), (9, h - 1)); L((0, 0), (9, h - 1))
    elif ch == 'M':
        L((0, 0), (0, h - 1)); L((11, 0), (11, h - 1)); L((0, 0), (5, 10)); L((6, 10), (11, 0)); w = 12
    elif ch == 'A':
        L((0, h - 1), (5, 0)); L((5, 0), (10, h - 1)); L((2, 10), (8, 10)); w = 11
    elif ch == 'T':
        L((0, 0), (10, 0)); L((5, 0), (5, h - 1)); w = 11
    return w

def logo():
    W, H = 200, 48
    im = Image.new('L', (W, H), 0); d = ImageDraw.Draw(im)
    x = 2
    for c in 'DECISION':
        x += buchstabe(d, c, x, 4) + 5
    xe = x
    x = 2
    for c in 'MATE':
        x += buchstabe(d, c, x, 28) + 5
    # grosses V, Unterkante bei Zeile 43, oben auf Hoehe der ersten Zeile
    vx = xe + 6
    d.line([(vx, 2), (vx + 15, 45)], fill=255); d.line([(vx + 16, 45), (vx + 31, 2)], fill=255)
    bb = im.getbbox(); im = im.crop((0, 0, bb[2] + 2, H))
    return im

def save_bmp(im, path):
    """1-bit DIB: 0 schwarz, 1 weiss."""
    w, h = im.size; px = im.load()
    stride = ((w + 31) // 32) * 4
    rows = []
    for y in range(h - 1, -1, -1):
        r = bytearray(stride)
        for x in range(w):
            if px[x, y]: r[x // 8] |= 0x80 >> (x % 8)
        rows.append(bytes(r))
    bits = b''.join(rows)
    hdr = struct.pack('<IiiHHIIiiII', 40, w, h, 1, 1, 0, len(bits), 0, 0, 2, 2)
    pal = bytes((0, 0, 0, 0, 255, 255, 255, 0))
    off = 14 + 40 + 8
    open(path, 'wb').write(b'BM' + struct.pack('<IHHI', off + len(bits), 0, 0, off) + hdr + pal + bits)

im = logo(); save_bmp(im, 'res/LOGO.BMP')
print('Logo', im.size)
im.convert('RGB').resize((im.size[0] * 4, im.size[1] * 4), Image.NEAREST).save('res/logo.png')
with open('LOGOGR.H', 'w', newline='\r\n') as f:
    f.write('/* LOGOGR.H - Groesse des Schriftzugs (mkres.py) */\n#define LOGOW %d\n#define LOGOH %d\n' % im.size)

def schieber(titel, mn, mx, pos):
    b = bytearray(64); struct.pack_into('<H', b, 0, 2); b[2:2 + len(titel)] = titel.encode('latin1')
    struct.pack_into('<H', b, 24, pos); struct.pack_into('<HHH', b, 48, mn, mx, 100); return bytes(b)
open('res/CTL1.BIN', 'wb').write(schieber('Tempo:', 1, 10, 45))
open('res/CTL2.BIN', 'wb').write(schieber('Schweif:', 20, 90, 50))
open('res/CTL3.BIN', 'wb').write(bytes(32))
open('res/CTL4.BIN', 'wb').write(bytes(32))
open('res/T10.BIN', 'wb').write(b'DMV-Warp - Code: Claude, Prompts: rfka01.\r\n  MIT-Lizenz.\0')
open('res/T20.BIN', 'wb').write(b'DMV-Warp\0')
open('res/T30.BIN', 'wb').write(b'DMV-WARP\r\n\r\nDer Schriftzug DECISION MATE V zieht einen Regenbogen hinter sich her.\0')
