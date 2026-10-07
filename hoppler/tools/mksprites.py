# Erzeugt SPRITES.INC (Turbo Pascal 3 typed constants) fuer HOPPLER
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
                 ("SprCoin", COIN), ("SprSplash", SPLASH),
                 ("SprLibFly", LIB_FLY), ("SprBirdZu", VOGEL_ZU),
                 ("SprCoinN", COIN_N), ("SprCoinE", COIN_E),
                 ("SprToad1", KROETE_K1), ("SprToad2", KROETE_K2),
                 ("SprToad3", KROETE_K3)):
    out.append(pas_arr(nm, [r.replace(' ', '.') for r in rows]))
open("SPRITES.INC","w",newline="\r\n").write("\n".join(out)+"\n")
print("ok")
