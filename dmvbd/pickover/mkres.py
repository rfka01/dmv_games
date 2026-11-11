#!/usr/bin/env python3
"""Ressourcen von PICKOVER.AD: Sinustafel (SINTAB.H), Einstellungen
(Typ 1000) und Texte (Typ 2000). Code: Claude, Prompts: rfka01"""
import math, struct, os

os.makedirs('res', exist_ok=True)
with open('SINTAB.H', 'w', newline='\r\n') as f:
    f.write('/* SINTAB.H - Sinus, 1024 Schritte je Umlauf, 1.0 = 16384 (mkres.py) */\n')
    f.write('static const short sintab[1024] = {\n')
    v = [int(round(math.sin(2 * math.pi * i / 1024) * 16384)) for i in range(1024)]
    v = [min(16383, max(-16384, x)) for x in v]
    for i in range(0, 1024, 16): f.write('  ' + ', '.join('%d' % x for x in v[i:i + 16]) + ',\n')
    f.write('};\n')

def schieber(titel, mn, mx, pos):
    b = bytearray(64)
    struct.pack_into('<H', b, 0, 2); b[2:2 + len(titel)] = titel.encode('latin1')
    struct.pack_into('<H', b, 24, pos); struct.pack_into('<HHH', b, 48, mn, mx, 100)
    return bytes(b)

def auswahl(titel, namen, vorgabe):             # Art 3: Liste, Wert = Nummer
    b = bytearray(32 + 16 * len(namen))
    struct.pack_into('<H', b, 0, 3); b[2:2 + len(titel)] = titel.encode('latin1')
    struct.pack_into('<HH', b, 22, len(namen), vorgabe)
    for k, s in enumerate(namen): b[32 + 16 * k:32 + 16 * k + len(s)] = s.encode('latin1')
    return bytes(b)
open('res/CTL3.BIN', 'wb').write(auswahl('Farbe:', ['Weiss', 'Bunt'], 0))
open('res/CTL4.BIN', 'wb').write(bytes(32))
open('res/T10.BIN', 'wb').write(b'Pickover-Attraktor - Code: Claude, Prompts: rfka01.\r\n  MIT-Lizenz.\0')
open('res/T20.BIN', 'wb').write(b'Pickover\0')
open('res/T30.BIN', 'wb').write(
    b'PICKOVER\r\n\r\nSeltsame Attraktoren nach Clifford Pickover: eine immer wieder '
    b'angewandte Abbildung mit Sinus und Kosinus, als Dichte in Grau gezeichnet. '
    b'Jedes Gebilde zieht von unten nach oben durch das Bild.\0')

# Vorspann: drei Zeilen, nach hinten geneigt (oben schmaler und flacher),
# 1 Bit je Punkt, oberste Zeile zuerst -> TITEL.H
from PIL import Image, ImageDraw, ImageFont
TW, TH = 480, 132                                # Breite, Hoehe auf dem Schirm
ZEILEN = ['Clifford', "Pickover's", 'seltsamer Attraktor']
F = ImageFont.truetype('/usr/share/fonts/truetype/dejavu/DejaVuSans-Bold.ttf', 96)
SW, ZH = 1400, 124                               # flacher Block, doppelt fein
flach = Image.new('L', (SW, ZH * len(ZEILEN) + 16), 0)
dr = ImageDraw.Draw(flach)
for i, t in enumerate(ZEILEN):
    w = dr.textlength(t, font=F)
    dr.text(((SW - w) / 2, 8 + i * ZH), t, font=F, fill=255)
SH = flach.size[1]
OBEN = 0.55                                      # Breite oben : unten
breit = [OBEN + (1 - OBEN) * (r + 0.5) / TH for r in range(TH)]
acc = [0.0]                                      # Quellzeile je Schirmzeile: oben enger
for s in breit: acc.append(acc[-1] + 1 / s)
px = flach.load()
k = SW / (TW * 1.0)                              # unten: ganze Blockbreite auf TW
bits = []
for r in range(TH):
    s = breit[r]
    reihe = []
    for x in range(TW):
        sum_ = 0
        for dy in (0.25, 0.75):                  # 2x2 Abtastung
            v = (acc[r] + dy * (acc[r + 1] - acc[r])) / acc[-1] * SH
            for dx in (0.25, 0.75):
                u = SW / 2 + ((x + dx) - TW / 2) * k / s
                if 0 <= u < SW and 0 <= v < SH: sum_ += px[int(u), int(v)]
        reihe.append(1 if sum_ >= 2 * 255 else 0)
    bits.append(reihe)
with open('TITEL.H', 'w', newline='\r\n') as f:
    f.write('/* TITEL.H - Vorspann, %dx%d, 1 Bit, oberste Zeile zuerst (mkres.py) */\n' % (TW, TH))
    f.write('#define TW %d\n#define TH %d\nstatic const BYTE titel[TH][TW / 8] = {\n' % (TW, TH))
    for reihe in bits:
        by = [sum(reihe[i + j] << (7 - j) for j in range(8)) for i in range(0, TW, 8)]
        f.write(' {' + ','.join('%d' % b for b in by) + '},\n')
    f.write('};\n')
Image.frombytes('1', (TW, TH), bytes(sum(([sum(r[i + j] << (7 - j) for j in range(8)) for i in range(0, TW, 8)] for r in bits), []))).save('res/titel.png')
