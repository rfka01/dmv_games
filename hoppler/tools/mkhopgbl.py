"""mkhopgbl.py - schreibt HOPPLER.GBL (Bildsammlung fuer DMVGRAF gbldv).

  mkhopgbl.py HOPPLER.GBL

Bild 0..85:  Seerosen und Ringe (berechnet wie frueher MakeImages), Bild N*2+Lage,
             Format 4 (je Zeile Maske, Gruen, Rot, Blau, Bit 0 links).
Ab 86:       deckende Ausschnitte des Titelstreifens mit fertigem
             Hintergrund fuer die Titel-Animation (Muenze, Libelle,
             Dummvogel mit Ei, Kroete) - sie werden in einem Zug
             gezeichnet, ohne dass dazwischen der Hintergrund aufblitzt.
Die Nummern stehen in GBLNUM.INC (fuer HOPPLER.PAS).

Code: Claude (Anthropic), Prompts: rfka01. Lizenz: MIT"""
import os, struct, sys
HIER = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HIER)
import hopbild as H
import sprites_def as SD

EI = ["..KK..",               # Ei, doppelt gross gezeichnet (Zoom 1)
      ".KWWK.",
      "KWWWWK",
      "KWCWWK",
      "KWWWCK",
      "KWWWWK",
      ".KWWK.",
      "..KK.."]
EIX, EIY = 148, 134                 # Ei vor den Fuessen des Dummvogels

def fmt4(m, g, r, b):
    """Zeilen aus Bitlisten -> Bytes Format 4"""
    out = bytearray()
    for plane in (m, g, r, b):
        v = 0; bs = bytearray()
        for i, bit in enumerate(plane):
            if bit: v |= 1 << (i & 7)
            if (i & 7) == 7: bs.append(v); v = 0
        if len(plane) & 7: bs.append(v)
        out += bs
    return bytes(out)

def eintrag(b, h, daten):
    return struct.pack('<HHBB', b, h, 4, 0) + daten

def pad_bild(al, w, r, ws):
    d = bytearray()
    for y in range(r):
        planes = []
        for p in range(4):
            bits = []
            for k in range(w):
                v = ws[(p * r + y) * w + k]
                bits += [(v >> i) & 1 for i in range(16)]
            planes.append(bits)
        d += fmt4(*planes)
    return eintrag(w * 16, r, bytes(d))

def ausschnitt(s, x, y, b, h):
    d = bytearray()
    for yy in range(y, y + h):
        z = s.p[yy][x:x + b]
        d += fmt4([1] * b, [(c >> 1) & 1 for c in z], [(c >> 2) & 1 for c in z], [c & 1 for c in z])
    return eintrag(b, h, bytes(d))

def main():
    bilder = H.mache_bilder()
    out = bytearray(); num = []
    for n in range(H.NIMG + 1):
        for v in (0, 1):
            out += pad_bild(*bilder[n, v])
    nr = 2 * (H.NIMG + 1)
    TY = H.TY
    # Muenze: x 576..623, y 120..155
    num.append(('imCoin', nr))
    for k in range(3):
        s = H.titel(bilder, muenze=(k == 0))
        if k == 1: s.sprite(SD.COIN_N, 584, TY - 22, 1)
        if k == 2: s.sprite(SD.COIN_E, 584, TY - 22, 1)
        out += ausschnitt(s, 576, 120, 48, 36); nr += 1
    # Libelle hebt ab: x 16..47, y 104..151
    num.append(('imLift', nr))
    for dy, fly in ((2, 1), (4, 1), (6, 1), (8, 1), (10, 1), (11, 1), (12, 1), (1, 0), (0, 0)):
        s = H.titel(bilder, lib=False)
        s.sprite(SD.LIB_FLY if fly else SD.LIB_F, 16, TY - 26 - dy, 1)
        out += ausschnitt(s, 16, 104, 32, 48); nr += 1
    # Dummvogel legt ein Ei: x 144..207, y 116..151
    num.append(('imBird', nr))
    folge = ((SD.DUMMVOGEL, 0, 0), (SD.VOGEL_GEB, 0, 0), (SD.VOGEL_R, 12, 1),
             (SD.VOGEL_R, 24, 1), (SD.DUMMVOGEL, 24, 1), (SD.VOGEL_L, 12, 1))
    for spr, dx, ei in folge:
        s = H.titel(bilder, vogel=False)
        if ei: s.sprite(EI, EIX, EIY, 1)
        s.sprite(spr, 144 + dx, TY - 26, 1)
        out += ausschnitt(s, 144, 116, 64, 36); nr += 1
    # Kroete: x 400..431, y 112..151
    num.append(('imToad', nr))
    for spr in (None, SD.KROETE_K1, SD.KROETE_K2, SD.KROETE_K3):
        s = H.titel(bilder)
        if spr: s.sprite(spr, 400, TY - 27, 1)
        out += ausschnitt(s, 400, 112, 32, 40); nr += 1
    num.append(('NBild', nr))
    open(sys.argv[1], 'wb').write(out)
    inc = ["{ GBLNUM.INC - erzeugt von tools/mkhopgbl.py - Bildnummern in HOPPLER.GBL }",
           "const"] + ["  %s = %d;" % (a, b) for a, b in num]
    open(os.path.join(os.path.dirname(os.path.abspath(sys.argv[1])), 'GBLNUM.INC'), 'w',
         newline='\r\n').write('\n'.join(inc) + '\n')
    print('%s: %d Bilder, %d Byte' % (sys.argv[1], nr, len(out)))

main()
