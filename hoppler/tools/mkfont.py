# Erzeugt FONT.INC (eigene Schrift fuer DMVGFX.INC) aus font_def.py
import sys, os
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from font_def import F, UML, cell
rows = []
for c in range(32, 127):
    rows.append(cell(F[chr(c)]))
codes = list(UML.keys())
for k in codes:
    nm, art, cap = UML[k]
    rows.append(cell(art, cap))
out = ["{ FONT.INC - erzeugt von tools/mkfont.py aus tools/font_def.py - nicht von Hand aendern }",
       "{ 8x16-Zeichen, Zeilen von oben nach unten, Bit 0 = linker Punkt.",
       "  Index 0..94 = Zeichen 32..126, 95..101 = Umlaute laut FontUml (CP437).",
       "  Lizenz: MIT }",
       "const",
       "  FontUml: array[0..6] of byte = (" + ", ".join(f"${k:02X}" for k in codes) + ");",
       "  FontG: array[0..101, 0..15] of byte = ("]
lines = []
for i, r in enumerate(rows):
    lab = chr(32 + i) if i < 95 else UML[codes[i - 95]][0]
    if lab in "{}": lab = "Klammer"
    lines.append("    (" + ",".join(f"${v:02X}" for v in r) + ")")
out.append(",\n".join(lines) + ");")
open("FONT.INC", "w", newline="\r\n").write("\n".join(out) + "\n")
