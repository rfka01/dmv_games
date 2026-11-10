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
