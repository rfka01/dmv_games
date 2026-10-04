# Erzeugt SPRITES.INC (Turbo Pascal 3 typed constants) fuer TOPPLER
# Farben: K=schwarz 0, B=blau 1, G=gruen 2, C=cyan 3, R=rot 4, M=magenta 5, Y=gelb 6, W=weiss 7
# '.' = durchsichtig
import sys, os
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from sprites_def import *
# Pro Ebene (0=gruen,1=rot,2=blau) eine SET- und eine RESET-Maske, Bit 0 = linker Punkt
COL = {'K':0,'B':1,'G':2,'C':3,'R':4,'M':5,'Y':6,'W':7}
PBIT = [2,4,1]   # Ebene 0 gruen, 1 rot, 2 blau

FROG = [
"..BB........BB..",
".BWWB......BWWB.",
".BWKB......BKWB.",
"..BBGKKKKKKGBB..",
"...KGGGGGGGGK...",
"..KGGYGGGGYGGK..",
".KGGGGYGGYGGGGK.",
"KGGKGGGGGGGGKGGK",
"KGK.KGGGGGGK.KGK",
".K..KGGGGGGK..K.",
"...KGGGRRGGGK...",
"..KGGKGGGGKGGK..",
".KGGK.KGGK.KGGK.",
"KGGK...KK...KGGK",
"KGK..........KGK",
".K............K.",
]
COIN = [
".....KKKKKK.....",
"...KKYYYYYYKK...",
"..KYYWWYYYYYYK..",
".KYWWYYYYYYYYYK.",
".KYWYYYYYYYYYYK.",
"KYYYYYYYYYYYYYYK",
"KYYYYYYYYYYYYYYK",
"KYYYYYYYYYYYYYYK",
"KYYYYYYYYYYYYYYK",
"KYYYYYYYYYYYYYYK",
"KYYYYYYYYYYYYYYK",
".KYYYYYYYYYYYYK.",
".KYYYYYYYYYYYYK.",
"..KYYYYYYYYYYK..",
"...KKYYYYYYKK...",
".....KKKKKK.....",
]
SPLASH = [
"......W..W......",
"..W...CWWC...W..",
"...C.C....C.C...",
"....C......C....",
".WC..........CW.",
"..C...BBBB...C..",
"W....B....B....W",
".C..B......B..C.",
".C..B......B..C.",
"W....B....B....W",
"..C...BBBB...C..",
".WC..........CW.",
"....C......C....",
"...C.C....C.C...",
"..W...CWWC...W..",
"......W..W......",
]
DOT = [
"..KKKK..",
".KXXXXK.",
"KXXWXXXK",
"KXWXXXXK",
"KXXXXXXK",
"KXXXXXXK",
".KXXXXK.",
"..KKKK..",
]
def masks(rows):
    w = len(rows[0]); assert all(len(r)==w for r in rows), rows
    out = []
    for p in range(3):
        s = []; r_ = []
        for row in rows:
            sm = rm = 0
            y = len(s)
            for x,ch in enumerate(row):
                if ch == '.': continue
                if ch.islower():          # Raster mit Schwarz (Schattierung)
                    ch = ch.upper() if (x + y) % 2 == 0 else 'K'
                c = COL[ch]
                if c & PBIT[p]: sm |= 1<<x
                else: rm |= 1<<x
            s.append(sm); r_.append(rm)
        out.append((s, r_))
    return out
def pas_arr(name, rows):
    m = masks(rows); n = len(rows)
    lines = [f"  {name}: Spr{n} = ("]
    pl = []
    for p in range(3):
        ops = []
        for o in range(2):
            ops.append("(" + ",".join(f"${v:04X}" for v in m[p][o]) + ")")
        pl.append("    (" + ",\n     ".join(ops) + ")")
    lines.append(",\n".join(pl) + ");")
    return "\n".join(lines)
out = ["{ SPRITES.INC - erzeugt von tools/mksprites.py - nicht von Hand aendern }",
       "{ je Ebene (0=gruen 1=rot 2=blau): SET-Maske, RESET-Maske; Bit 0 = links }",
       "const"]
for nm, rows in (("SprLibF", LIB_F), ("SprLibB", LIB_B),
                 ("SprLibL", LIB_L), ("SprLibR", LIB_R),
                 ("SprBird", DUMMVOGEL), ("SprBirdC", VOGEL_GEB),
                 ("SprBirdL", VOGEL_L), ("SprBirdR", VOGEL_R),
                 ("SprKaefer", KAEFER), ("SprKroete", KROETE),
                 ("SprBiene", BIENE), ("SprBluete", BLUETE),
                 ("SprCoin", COIN), ("SprSplash", SPLASH)):
    out.append(pas_arr(nm, [r.replace(' ', '.') for r in rows]))
cg = open(os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "CHARGEN0.OVR"), "rb").read()
def glyph(c):
    g = cg[c*16:c*16+16]; return list(g[8:16]) + list(g[0:8])
def dots(rows, r):
    rows = rows[:]; rows[r] |= 0x24; return rows
SZ = [0,0,0,0x1C,0x22,0x22,0x12,0x0A,0x12,0x22,0x22,0x1A,0x02,0,0,0]
UM = [("ae", dots(glyph(ord('a')), 4)), ("oe", dots(glyph(ord('o')), 4)),
      ("ue", dots(glyph(ord('u')), 4)), ("Ae", dots(glyph(ord('A')), 1)),
      ("Oe", dots(glyph(ord('O')), 1)), ("Ue", dots(glyph(ord('U')), 1)),
      ("ss", SZ)]
lines = ["  { Umlaute in CP437: ae 84h oe 94h ue 81h Ae 8Eh Oe 99h Ue 9Ah ss E1h;",
         "    Aufbau wie GGGCHG: Bytes 1-8 untere, 9-16 obere Haelfte }",
         "  UmlCode: array[0..6] of byte = ($84, $94, $81, $8E, $99, $9A, $E1);",
         "  UmlGlyph: array[0..6, 1..16] of byte = ("]
gl = []
for nm, rows in UM:
    b = rows[8:16] + rows[0:8]
    gl.append("    (" + ",".join(f"${v:02X}" for v in b) + ")")
lines.append(",\n".join(gl) + ");")
out.append("\n".join(lines))
open("SPRITES.INC","w",newline="\r\n").write("\n".join(out)+"\n")
print("ok")
