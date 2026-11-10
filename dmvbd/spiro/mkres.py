#!/usr/bin/env python3
"""Einstellungen (Typ 1000) und Texte (Typ 2000) von SPIRO.AD.
Code: Claude, Prompts: rfka01"""
import struct, os
os.makedirs('res', exist_ok=True)
def schieber(titel, mn, mx, pos):
    b = bytearray(64); struct.pack_into('<H', b, 0, 2); b[2:2 + len(titel)] = titel.encode('latin1')
    struct.pack_into('<H', b, 24, pos); struct.pack_into('<HHH', b, 48, mn, mx, 100); return bytes(b)
def kaestchen(titel, an):
    b = bytearray(32); struct.pack_into('<H', b, 0, 5); b[2:2 + len(titel)] = titel.encode('latin1')
    struct.pack_into('<H', b, 24, an); return bytes(b)
def liste(titel, namen, vorgabe):           # Art 3: Auswahl, Vorgabe = Nummer ab 0
    b = bytearray(32 + 16 * len(namen)); struct.pack_into('<H', b, 0, 3); b[2:2 + len(titel)] = titel.encode('latin1')
    struct.pack_into('<HH', b, 22, len(namen), vorgabe)
    for k, n in enumerate(namen): b[32 + 16 * k:32 + 16 * k + len(n)] = n.encode('latin1')
    return bytes(b)
open('res/CTL1.BIN', 'wb').write(schieber('Tempo:', 1, 10, 45))
open('res/CTL2.BIN', 'wb').write(schieber('Figuren:', 1, 8, 14))      # 2 Figuren, dann leeren
open('res/CTL3.BIN', 'wb').write(kaestchen('Zahnkranz', 1))
open('res/CTL4.BIN', 'wb').write(liste('Raeder:', ['Gemischt', 'Rad innen', 'Rad aussen', 'Rad im Rad', 'Oval', 'Dreieck'], 0))
open('res/T10.BIN', 'wb').write(b'Spirograph - Code: Claude, Prompts: rfka01.\r\n  MIT-Lizenz.\0')
open('res/T20.BIN', 'wb').write(b'Spirograph\0')
open('res/T30.BIN', 'wb').write(b'SPIROGRAPH\r\n\r\nEin Rad rollt im Zahnkranz oder um ein Mittelrad, auch Rad im Rad '
    b'und unrunde Raeder; ein Stift im Loch zeichnet die Bahn. Nach einigen Figuren wird der Innenraum geleert.\0')
