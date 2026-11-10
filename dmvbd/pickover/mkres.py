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

def liste(titel, namen, werte, vorgabe):
    n = len(namen)
    b = bytearray(26 + 2 * n + 16 * n + 2 * n)
    struct.pack_into('<H', b, 0, 1); b[2:2 + len(titel)] = titel.encode('latin1')
    struct.pack_into('<HH', b, 22, n, vorgabe)
    for k, s in enumerate(namen): b[26 + 2 * n + 16 * k:26 + 2 * n + 16 * k + len(s)] = s.encode('latin1')
    for k, w in enumerate(werte): struct.pack_into('<H', b, 26 + 18 * n + 2 * k, w)
    return bytes(b)

open('res/CTL1.BIN', 'wb').write(schieber('Tempo:', 1, 4, 34))
open('res/CTL2.BIN', 'wb').write(schieber('Dichte:', 10, 80, 43))       # Tausend Schritte
# Liste: Eintrag 0 -> Wert 0, Eintrag k -> k-ter Wert der Tafel (wie After Dark)
open('res/CTL3.BIN', 'wb').write(liste('Farbe:', ['Weiss', 'Bunt'], [1], 0))
open('res/CTL4.BIN', 'wb').write(bytes(32))
open('res/T10.BIN', 'wb').write(b'Pickover-Attraktor - Code: Claude, Prompts: rfka01.\r\n  MIT-Lizenz.\0')
open('res/T20.BIN', 'wb').write(b'Pickover\0')
open('res/T30.BIN', 'wb').write(
    b'PICKOVER\r\n\r\nSeltsame Attraktoren nach Clifford Pickover: eine immer wieder '
    b'angewandte Abbildung mit Sinus und Kosinus, als Dichte in Grau gezeichnet. '
    b'Jedes Gebilde zieht von unten nach oben durch das Bild.\0')
