{ DMVGRAF.INC - Grafik, Bilder, Maus und Ton fuer die NCR Decision Mate V,
  Fassung fuer Turbo Pascal 3.

  Dieselbe Bibliothek wie die C-Fassung (src/GRAF.C, GBILD.C, GEIN.C,
  GKERN.C), Zeile fuer Zeile uebersetzt; Beschreibung der Funktionen in
  README.md. Vorher einbinden: die Hardwareschicht (DMVHWD.INC fuer
  MS-DOS) und DMVFONT.INC.

  Turbo Pascal 3 kann eine Funktion nicht als Anweisung aufrufen. Was in
  C einen Fehlercode liefert, ist darum hier eine Prozedur; der Code steht
  danach in GERR (0 = gut). Funktionen sind nur gkey, grnd, mpos und
  gdirekt.

  Code: Claude (Anthropic), Idee und Prompts: rfka01. Lizenz: MIT }

const
  { Formate fuer gbdef/gbload }
  GB_BIT7 = 1;  GB_BIT0 = 2;  GB_NIB = 3;  GB_EBN = 4;  GB_SETR = 5;
  { Arten fuer gbput/glput }
  GB_DECK = 0;  GB_DURCH = 1;  GB_SCHAT = 2;  GB_XOR = 3;
  { Fehlercodes }
  GE_ZOOM = 10;  GE_RAD = 20;  GE_RICHT = 30;  GE_MUST = 40;
  GE_MODUS = 50; GE_TRICHT = 60; GE_SEITE = 80;
  GE_NR = 90;  GE_PLATZ = 91;  GE_DATEI = 92;  GE_KURZ = 93;
  GE_FORM = 94;  GE_LEER = 95;  GE_LAGE = 96;  GE_HW = 99;

  QLMAX = hwQL;   GBMAX = 64;  GLMAX = 200;  BANDH = 8;  QBUF = hwQB;
  EB = 640;                         { BANDH * 80: Abstand der Ebenen im Band }
  ZHMAX = 32;  ZKH = 48;  ZKB = 12;    { Mauszeiger }

  qdx: array[0..7] of integer = (0, 1, 1, 1, 0, -1, -1, -1);
  qdy: array[0..7] of integer = (1, 1, 0, -1, -1, -1, 0, 1);

  { 16 Muster, je 8 Zeilen von oben, Bit 0 = links; die ersten zwei Zeilen
    sind zugleich das Linienmuster }
  qmust: array[0..15, 0..7] of byte = (
    ($FF,$FF,$FF,$FF,$FF,$FF,$FF,$FF), ($55,$AA,$55,$AA,$55,$AA,$55,$AA),
    ($11,$44,$11,$44,$11,$44,$11,$44), ($EE,$BB,$EE,$BB,$EE,$BB,$EE,$BB),
    ($FF,$00,$00,$00,$FF,$00,$00,$00), ($11,$11,$11,$11,$11,$11,$11,$11),
    ($80,$40,$20,$10,$08,$04,$02,$01), ($01,$02,$04,$08,$10,$20,$40,$80),
    ($FF,$11,$11,$11,$FF,$11,$11,$11), ($81,$42,$24,$18,$18,$24,$42,$81),
    ($FF,$01,$01,$01,$FF,$10,$10,$10), ($11,$00,$44,$00,$11,$00,$44,$00),
    ($FF,$FF,$00,$00,$FF,$FF,$00,$00), ($33,$33,$33,$33,$33,$33,$33,$33),
    ($0F,$0F,$0F,$0F,$F0,$F0,$F0,$F0), ($00,$00,$00,$00,$00,$00,$00,$00));

  bfa: array[0..15] of integer = (0, 1, 2, 3, 4, 5, 6, 7, 0, 1, 2, 3, 4, 5, 6, 7);
  bfm: array[0..15] of integer = (0, 0, 0, 0, 0, 0, 0, 7, 0, 0, 0, 0, 0, 0, 0, 7);
  bnlo: array[0..15] of byte = (0, 0, 1, 0, 2, 0, 1, 0, 3, 0, 1, 0, 2, 0, 1, 0);
  bnhi: array[0..15] of byte = (0, 0, 1, 1, 2, 2, 2, 2, 3, 3, 3, 3, 3, 3, 3, 3);

type
  GBInfoArr = array[0..5] of integer;

var
  gerr: integer;                    { Ergebnis des letzten Aufrufs }
  qneb, qhw, qx, qy, qorig, qan, qcol, qmode, qpatt, qzoom, qzs: integer;
  qbcol: integer;                   { Hintergrund der Muster, -1 keiner }
  qebad: array[0..2] of integer;
  qebit: array[0..2] of integer;
  ql: array[0..QLMAX] of byte;
  qn, qe: integer;
  qzeig: boolean;

  grz: integer;                     { grnd }
  gmb: array[0..7] of byte;         { Mausblock }


var
  qth, qtl: array[0..31] of byte;   { Tonwarteschlange }
  qtk, qtt, qtp: integer;           { Kopf, Ende, Stufe }
  qbtu: array[0..35] of integer;    { Tabelle fuer ghblit }
  qbtn: integer;
  qbsch: boolean;                   { schneller Weg fuer Bilder an }

procedure qzhid; forward;
procedure qzwied; forward;
procedure gzeig(nr, x, y: integer); forward;
procedure gbstart; forward;

{ ------------------------------------------------------------------ }
{ Hilfen fuer 16 Bit ohne Vorzeichen                                 }

function uwr(i: integer): real;     { Bitmuster als 0..65535 }
begin
  if i < 0 then uwr := i + 65536.0 else uwr := i;
end;

function rint(r: real): integer;    { 0..65535 -> Bitmuster }
begin
  if r >= 32768.0 then rint := Trunc(r - 65536.0) else rint := Trunc(r);
end;

{ ------------------------------------------------------------------ }
{ Kern (GKERN.C)                                                     }

procedure gnib(var zz; var qq; var tt; n: integer);
var z: GBytes absolute zz;
    q: GBytes absolute qq;
    tb: GBytes absolute tt;
    k, j, w, e0, e1, e2, f, qi: integer;
begin
  qi := 0;
  for k := 0 to n - 1 do begin
    e0 := 0; e1 := 0; e2 := 0; f := 0;
    j := 0;
    while j < 8 do begin
      w := tb[q[qi]];
      qi := qi + 1;
      e0 := e0 or ((w and 3) shl j);
      e1 := e1 or (((w shr 2) and 3) shl j);
      e2 := e2 or (((w shr 4) and 3) shl j);
      f := f or (((w shr 6) and 3) shl j);
      j := j + 2;
    end;
    z[k] := f;
    z[80 + k] := e0;
    z[160 + k] := e1;
    z[240 + k] := e2;
  end;
end;

procedure gzeile(var dd; var zz; n, sh, cnt, ns, c01, c23, eb: integer);
var d: GBytes absolute dd;
    z: GBytes absolute zz;
    j, w, mm, cm, c0, c1, c2, v: integer;
begin
  cm := Hi(c01); c0 := Lo(c01); c1 := Hi(c23); c2 := Lo(c23);
  for j := 0 to cnt - 1 do begin
    if j < ns then v := z[j] else v := 0;
    w := v shl sh;
    mm := (w or cm) and $FF;
    cm := (w shr 8) and $FF;
    if j < ns then v := z[j + n] else v := 0;
    w := v shl sh;
    v := (w or c0) and $FF;
    c0 := (w shr 8) and $FF;
    d[j] := (d[j] and not mm) or (v and mm);
    if j < ns then v := z[j + 2 * n] else v := 0;
    w := v shl sh;
    v := (w or c1) and $FF;
    c1 := (w shr 8) and $FF;
    d[j + eb] := (d[j + eb] and not mm) or (v and mm);
    if j < ns then v := z[j + 3 * n] else v := 0;
    w := v shl sh;
    v := (w or c2) and $FF;
    c2 := (w shr 8) and $FF;
    d[j + 2 * eb] := (d[j + 2 * eb] and not mm) or (v and mm);
  end;
end;

{ ------------------------------------------------------------------ }
{ GDC-Liste (GRAF.C)                                                 }

procedure qflush;
begin
  if qn <> 0 then begin
    ql[qn] := 0;
    if qan <> 0 then ghlist(ql) else hwWst := false;
    qn := 0;
  end;
  qe := -1;
end;

procedure qplatz(n: integer);
begin
  if qzeig then qzhid;              { Mauszeiger weg, bevor anderes kommt }
  if qn + n + 2 >= QLMAX then qflush;
end;

procedure qcmd(c: integer);
begin
  qplatz(24);
  hwWst := false;                   { anderer Befehl: MASK/FIGS neu }
  qe := qn;
  ql[qn] := 1; ql[qn + 1] := c;
  qn := qn + 2;
end;

procedure qpar(b: integer);
begin
  ql[qn] := Lo(b);
  qn := qn + 1;
  ql[qe] := ql[qe] + 1;
end;

procedure qwrd(w: integer);
begin
  qpar(Lo(w)); qpar(Hi(w));
end;

procedure qpars(var ss; n: integer);   { n Parameter auf einmal }
begin
  Move(ss, ql[qn], n);
  qn := qn + n;
  ql[qe] := ql[qe] + n;
end;

procedure qpfill(v, n: integer);       { n gleiche Parameter }
begin
  FillChar(ql[qn], n, v);
  qn := qn + n;
  ql[qe] := ql[qe] + n;
end;

{ Block von n Zeilen zu nc Worten ab Bildzeile sy, Wortspalte c auf den
  Ebenen 0..ne-1 ersetzen; Quelle s, Zeilenabstand k, Ebenenabstand es
  Byte. Zeile fuer Zeile alle Ebenen }
procedure qblk(ne, sy, c, nc, n: integer; var ss; k, es: integer);
var s: GBytes absolute ss;
    adr, m, e, si: integer;
begin
  m := 2 * nc;
  si := 0;
  while n > 0 do begin
    for e := 0 to ne - 1 do begin
      if qn + m + 20 >= QLMAX then qflush;
      adr := qebad[e] + sy * 40 + c;
      ql[qn] := 4; ql[qn + 1] := $49; ql[qn + 2] := Lo(adr);
      ql[qn + 3] := Hi(adr); ql[qn + 4] := 0;
      ql[qn + 5] := 3; ql[qn + 6] := $4A; ql[qn + 7] := $FF; ql[qn + 8] := $FF;
      ql[qn + 9] := 4; ql[qn + 10] := $4C; ql[qn + 11] := 2; ql[qn + 12] := 0;
      ql[qn + 13] := 0;
      ql[qn + 14] := 1 + m; ql[qn + 15] := $20;
      Move(s[si + e * es], ql[qn + 16], m);
      qn := qn + 16 + m;
    end;
    n := n - 1; si := si + k; sy := sy + 1;
  end;
  qe := -1;
end;

{ CURS: Wortadresse, Punkt 0-15 im Wort (setzt MASK auf diesen Punkt) }
procedure qcurs(adr, p: integer);
begin
  qcmd($49);
  ql[qn] := Lo(adr); ql[qn + 1] := Hi(adr);
  ql[qn + 2] := (p and 15) shl 4;
  qn := qn + 3;
  ql[qe] := 4;
end;

procedure qzm(z: integer);          { GCHRD-Zoom }
begin
  if z <> qzs then begin
    qcmd($46);
    qpar(z and 15);
    qzs := z;
  end;
end;

function qsy(y: integer): integer;
begin
  if qorig <> 0 then qsy := y else qsy := 399 - y;
end;

function qebon(e, c: integer): boolean;
begin
  qebon := (c and qebit[e]) <> 0;
end;

{ Zeichenmodus einer Ebene fuer Figuren: 0 auslassen, 1 mit Muster,
  2 mit vollem Muster; w = WDAT-Befehl }
function qfmod(e: integer; var w: integer): integer;
var an: boolean;
begin
  an := qebon(e, qcol);
  case qmode of
    0: if an then begin w := $20; qfmod := 1; end
       else begin w := $22; qfmod := 2; end;
    1: begin w := $21; if an then qfmod := 1 else qfmod := 0; end;
    2: begin w := $22; qfmod := 1; end;
  else begin
    if an then w := $23 else w := $22;
    qfmod := 1;
  end;
  end;
end;

function qdir(d: integer): integer; { Richtung 0-7 des Programms -> 7220 }
begin
  if qorig <> 0 then qdir := (10 - d) and 7 else qdir := (2 + d) and 7;
end;

{ Ecke, an der der 7220 beginnt }
procedure qank(sx, sy, d, l: integer; var ox, oy: integer);
var k: integer;
begin
  ox := sx; oy := sy;
  if qorig <> 0 then begin
    k := (8 - d) and 7;
    ox := ox + l * qdx[k];
    oy := oy + l * qdy[k];
  end;
end;

function qadr(e, sx, sy: integer): integer;
begin
  sy := sy + sy + sy + sy + sy;      { sy * 40 ohne Multiplikation }
  sy := sy + sy;
  sy := sy + sy;
  qadr := qebad[e] + sy + sy + (sx shr 4);
end;

{ Figur auf allen Ebenen: Startpunkt, Muster (nm Byte ab PRAM 8),
  FIGS-Parameter, Abschlussbefehl (FIGD 6Ch oder GCHRD 68h) }
procedure qfig(sx, sy: integer; var mm; nm: integer; var ff; nf, ende: integer);
var mu: GBytes absolute mm;
    fp: GBytes absolute ff;
    e, k, i, w: integer;
begin
  for e := 0 to qneb - 1 do begin
    k := qfmod(e, w);
    if k <> 0 then begin
      qplatz(48);
      qcmd(w);
      qcurs(qadr(e, sx, sy), sx and 15);
      qcmd($78);
      if k = 2 then qpfill($FF, nm) else qpars(mu, nm);
      qcmd($4C);
      qpars(fp, nf);
      qcmd(ende);
    end;
  end;
end;

procedure qput(var ff; i, w: integer);   { Wort in FIGS-Parameter, 14 Bit }
var p: GBytes absolute ff;
begin
  p[i] := Lo(w);
  p[i + 1] := Hi(w) and $3F;
end;

{ ------------------------------------------------------------------ }
{ Ein- und Ausschalten                                               }

function qstart: integer;
begin
  if qhw = -2 then begin
    qneb := 3;
    qebad[0] := $0000; qebad[1] := $4000; qebad[2] := $8000;
    qebit[0] := 2; qebit[1] := 4; qebit[2] := 1;
    qhw := ghinit;
    if (qhw >= 0) and ((qhw and GH_FARBE) = 0) then begin
      qneb := 1;
      qebit[0] := 7;
    end;
  end;
  if qhw < 0 then qstart := GE_HW else qstart := 0;
end;

procedure ginit;
begin
  gerr := GE_HW;
  if qstart <> 0 then exit;
  qflush;
  ghgraf(true);
  qan := 1;
  qzs := 0; qpatt := 0; qzoom := 0; qmode := 1; qcol := 7; qbcol := -1;
  qx := 0; qy := 0;
  gerr := 0;
end;

procedure gexit;
begin
  gzeig(-1, 0, 0);
  qflush;
  if qan <> 0 then ghgraf(false);
  qan := 0;
  gerr := 0;
end;

procedure gclear;
const fp: array[0..4] of byte = ($12, $8F, $41, $80, $02);   { 400 x 640 }
var e, i: integer;
begin
  qzm(0);
  for e := 0 to qneb - 1 do begin
    qplatz(40);
    qcmd($22);
    qcurs(qadr(e, 0, 399), 0);
    qcmd($78);
    qpfill($FF, 8);
    qcmd($4C);
    qpars(fp, 5);
    qcmd($68);
  end;
  qflush;
  gerr := 0;
end;

procedure gpoint(x, y: integer);
begin
  if x < 0 then gerr := 1 else if x > 639 then gerr := 2
  else if y < 0 then gerr := 3 else if y > 399 then gerr := 4
  else begin qx := x; qy := y; gerr := 0; end;
end;

procedure where(var x, y: integer);
begin
  x := qx; y := qy; gerr := 0;
end;

procedure gorig(o: integer);
begin
  if o <> 0 then qorig := 1 else qorig := 0;
  gerr := 0;
end;

procedure gcolor(c: integer);
begin
  qcol := c and 7; gerr := 0;
end;

procedure gmode(m: integer);
begin
  if (m < 0) or (m > 3) then gerr := GE_MODUS
  else begin qmode := m; gerr := 0; end;
end;

procedure gpatt(p: integer);
begin
  if (p < 0) or (p > 15) then gerr := GE_MUST
  else begin qpatt := p; gerr := 0; end;
end;

{ Eigenes Muster p (0-15) aus 8 Zeilen von oben, Bit 0 = links }
procedure gpdef(p: integer; var mm);
var m: array[0..7] of byte absolute mm;
    i: integer;
begin
  if (p < 0) or (p > 15) then gerr := GE_MUST
  else begin
    for i := 0 to 7 do qmust[p, i] := m[i];
    gerr := 0;
  end;
end;

{ Hintergrundfarbe fuer Muster in Flaechen (gbox): 0-7, dann zweifarbig
  und deckend; -1 (Vorgabe) nur die Musterpunkte }
procedure gbcol(c: integer);
begin
  if (c < -1) or (c > 7) then gerr := GE_MODUS
  else begin qbcol := c; gerr := 0; end;
end;

procedure gzoom(z: integer);
begin
  if (z < 0) or (z > 15) then gerr := GE_ZOOM
  else begin qzoom := z; gerr := 0; end;
end;

{ ------------------------------------------------------------------ }
{ Linien, Rechtecke, Kreise                                          }

{ Muster am Schirm verankern: der 7220 beginnt GCHRD an der unteren
  linken Ecke (sx, sb) mit PRAM 15 und zeichnet die Zeilen abwechselnd
  von links und von rechts. Fuer Richtung 0 ohne Zoom die Musterzeilen
  so drehen, dass Punkt (x, y) Bit x and 7 von Zeile y and 7 bekommt. }
procedure qmanker(var mm; p, sx, sb, w: integer);
var mu: array[0..7] of byte absolute mm;
    r, c, v: integer;
begin
  for r := 0 to 7 do begin
    v := qmust[p, (sb - r) and 7];
    if (r and 1) <> 0 then c := (sx + w) and 7 else c := sx and 7;
    mu[7 - r] := ((v shr c) or (v shl (8 - c))) and $FF;
  end;
end;

procedure gbox(m, n, d: integer);
var fp: array[0..4] of byte;
    mu, mz: array[0..7] of byte;
    sx, sy, z, e, i: integer;
    f, h: boolean;
begin
  z := qzoom + 1;
  if (d < 0) or (d > 7) then begin gerr := GE_RICHT; exit; end;
  if m < 1 then begin gerr := 80; exit; end;
  if 1.0 * m * z > 640 then begin gerr := 81; exit; end;
  if n < 1 then begin gerr := 82; exit; end;
  if 1.0 * n * z > 640 then begin gerr := 83; exit; end;
  qank(qx, qsy(qy), d, n * z - 1, sx, sy);
  qzm(qzoom);
  fp[0] := $10 or qdir(d);
  qput(fp, 1, n - 1);
  fp[2] := fp[2] or $40;
  qput(fp, 3, m);
  if (d = 0) and (qzoom = 0) then qmanker(mu, qpatt, sx, sy, m)
  else Move(qmust[qpatt, 0], mu, 8);
  if qbcol < 0 then qfig(sx, sy, mu, 8, fp, 5, $68)
  else
    for e := 0 to qneb - 1 do begin { zweifarbig: je Ebene ersetzen }
      f := qebon(e, qcol);
      h := qebon(e, qbcol);
      for i := 0 to 7 do begin
        mz[i] := 0;
        if f then mz[i] := mu[i];
        if h then mz[i] := mz[i] or ((not mu[i]) and $FF);
      end;
      qplatz(48);
      qcmd($20);
      qcurs(qadr(e, sx, sy), sx and 15);
      qcmd($78);
      qpars(mz, 8);
      qcmd($4C);
      qpars(fp, 5);
      qcmd($68);
    end;
  qflush;
  gerr := 0;
end;

{ Tabelle fuer ghblit: je Ebene, was aus einer gespeicherten Bildzeile
  (np = 1: nur Punkte in der Farbe, sonst Maske und Ebenen zu je bb Byte)
  zu schreiben ist. art: deckend, durchsichtig, umkehren. }
procedure qbe(e, w, m, x, v, z: integer);
begin
  qbtu[qbtn * 6] := qebad[e]; qbtu[qbtn * 6 + 1] := w;
  qbtu[qbtn * 6 + 2] := m; qbtu[qbtn * 6 + 3] := x;
  qbtu[qbtn * 6 + 4] := v; qbtu[qbtn * 6 + 5] := z;
  qbtn := qbtn + 1;
end;

procedure qbtupel(np, bb, art: integer);
var e, v, ff: integer;
    an: boolean;
begin
  qbtn := 0;
  ff := hwAdrI(hwFF);
  for e := 0 to qneb - 1 do begin
    an := qebon(e, qcol);
    if np = 1 then begin            { Punkte in der Farbe }
      case art of
        0: if an then qbe(e, $20, ff, 0, 0, 0) else qbe(e, $20, 0, 0, 0, 1);
        1: if an then qbe(e, $23, 0, 0, 0, 0) else qbe(e, $22, 0, 0, 0, 0);
        3: if an then qbe(e, $21, 0, 0, 0, 0);
      end;
    end else begin                  { Maske und Ebenen }
      v := (e + 1) * bb;
      case art of
        0: qbe(e, $20, ff, 0, v, 0);
        1: begin qbe(e, $22, 0, -1, v, 0); qbe(e, $23, 0, 0, v, 0); end;
        3: qbe(e, $21, 0, 0, v, 0);
      end;
    end;
  end;
end;

{ ------------------------------------------------------------------ }
{ Zeilen wortweise                                                   }

{ Eine Bitzeile (Bit 0 = Punkt sx) in Ebene e mit WDAT-Befehl w.
  a: 0 Bits, 1 Bits umgekehrt, 2 lauter Einsen, 3 lauter Nullen }
procedure qrow(sy, sx: integer; var bb; nb, e, w, a: integer);
begin
  if (nb <= 0) or (sy < 0) or (sy > 399) then exit;
  qplatz(50 + (nb shr 3));          { kein Ausgeben mittendrin }
  ghrow(ql, qn, qe, qadr(e, sx, sy), sx, nb, w, a, bb, bb);
end;

{ Wie qrow mit Maske mm: a = 4 zeichnet mm and not pp, a = 5 mm and pp,
  a = 6 erst mm and not pp mit w, dann mm and pp mit w + 1 }
procedure qrowm(sy, sx: integer; var pp; var mm; nb, e, w, a: integer);
begin
  if (nb <= 0) or (sy < 0) or (sy > 399) then exit;
  if a = 6 then qplatz(48 + (nb shr 2)) else qplatz(50 + (nb shr 3));
  ghrow(ql, qn, qe, qadr(e, sx, sy), sx, nb, w, a, pp, mm);
end;

{ Bitzeile in der aktuellen Farbe und Betriebsart auf alle Ebenen }
procedure qbits(sy, sx: integer; var b; nb: integer);
var e: integer;
    an, ganz: boolean;
begin
  ganz := ((sx and 15) = 0) and ((nb and 15) = 0);
  for e := 0 to qneb - 1 do begin
    an := qebon(e, qcol);
    case qmode of
      0: if ganz then begin
           if an then qrow(sy, sx, b, nb, e, $20, 0)
           else qrow(sy, sx, b, nb, e, $20, 3);
         end else if an then begin
           qrow(sy, sx, b, nb, e, $22, 1);
           qrow(sy, sx, b, nb, e, $23, 0);
         end else qrow(sy, sx, b, nb, e, $22, 2);
      1: if an then qrow(sy, sx, b, nb, e, $21, 0);
      2: qrow(sy, sx, b, nb, e, $22, 0);
    else
      if an then qrow(sy, sx, b, nb, e, $23, 0)
      else qrow(sy, sx, b, nb, e, $22, 0);
    end;
  end;
end;

{ ------------------------------------------------------------------ }
{ Text                                                               }

function qglyph(c: integer): integer;   { Zeichen -> Index in fntbit }
var k: integer;
begin
  qglyph := 0;
  if (c >= 32) and (c < 127) then qglyph := c - 32
  else
    for k := 0 to 6 do
      if fntuml[k] = c then qglyph := 95 + k;
end;

var qtz, qtz2: array[0..79] of byte;
    qgi: array[0..79] of byte;

procedure qtxt0(var s: GStr; n: integer);   { waagrecht, wortweise }
var z1, gr, zr, i, sy, nb: integer;
begin
  z1 := qzoom + 1;
  nb := n * 8 * z1;
  sy := qsy(qy);
  if qorig = 0 then sy := sy - (16 * z1 - 1);
  for i := 0 to n - 1 do qgi[i] := qglyph(Ord(s[i + 1]));
  for gr := 0 to 15 do begin
    for i := 0 to n - 1 do qtz[i] := fntbit[qgi[i] * 16 + gr];
    if z1 = 1 then
      qbits(sy + gr, qx, qtz, nb)
    else begin
      ghzbits(qtz2, qtz, 0, n * 8, z1);
      for zr := 0 to z1 - 1 do qbits(sy + gr * z1 + zr, qx, qtz2, nb);
    end;
  end;
end;

procedure qtxtd(var s: GStr; n, d: integer);  { je Zeichen zwei 8x8-Muster }
var fp: array[0..4] of byte;
    mu: array[0..7] of byte;
    z1, td, ud, sx, sy, i, g, st, dxi, dyi, dxh, dyh: integer;
begin
  z1 := qzoom + 1;
  td := qdir(d);
  ud := (td + 2) and 7;
  qank(qx, qsy(qy), d, 16 * z1 - 1, sx, sy);
  qzm(qzoom);
  fp[0] := $10 or td; fp[1] := 7; fp[2] := $40; fp[3] := 8; fp[4] := 0;
  st := 8 * z1;                     { Schritt je Zeichen und Haelfte }
  dxi := st * qdx[td]; dyi := st * qdy[td];
  dxh := st * qdx[ud]; dyh := st * qdy[ud];
  for i := 0 to n - 1 do begin
    g := qglyph(Ord(s[i + 1])) * 16;
    { PRAM 8 = oberste Zeile, 15 = unterste: untere Haelfte zuerst }
    Move(fntbit[g + 8], mu, 8);
    qfig(sx, sy, mu, 8, fp, 5, $68);
    Move(fntbit[g], mu, 8);
    qfig(sx + dxh, sy + dyh, mu, 8, fp, 5, $68);
    sx := sx + dxi; sy := sy + dyi;
  end;
end;

procedure gtext(s: GStr; d: integer);
var n, l, sx, sy: integer;
begin
  if (d < 0) or (d > 7) then begin gerr := GE_TRICHT; exit; end;
  gerr := 0;
  n := Length(s);
  if n * 8 * (qzoom + 1) > 640 then n := 640 div (8 * (qzoom + 1));
  if n = 0 then exit;
  if d = 0 then qtxt0(s, n) else qtxtd(s, n, d);
  qflush;
  { Zeichenpunkt hinter das letzte Zeichen }
  l := n * 8 * (qzoom + 1);
  sx := qx + l * qdx[qdir(d)];
  sy := qsy(qy) + l * qdy[qdir(d)];
  if (sx >= 0) and (sx < 640) and (sy >= 0) and (sy < 400) then begin
    qx := sx;
    if qorig <> 0 then qy := sy else qy := 399 - sy;
  end;
end;

{ ------------------------------------------------------------------ }
{ Maus, Tastatur, Zeit, Auskunft, Ton (GEIN.C)                       }

{ Ist das eine DMV? Liest nur die Statusbits des 7220 (vor ginit). }
function gdmv: boolean;
begin
  gdmv := hwDMV;
end;

procedure ginfo(var f, m, s, c: integer);
var r: integer;
begin
  r := qstart;
  f := 0; m := 0; s := 0; c := 0;
  if r = 0 then begin
    if (qhw and GH_FARBE) <> 0 then f := 1;
    if (qhw and GH_MAUS) <> 0 then m := 1;
    if (qhw and GH_SYN) <> 0 then s := 1;
    if (qhw and GH_COVOX) <> 0 then c := 1;
  end;
  gerr := r;
end;

{ ---- Ton: Warteschlange fuer den Tastatur-8741 (Befehl 06h, Ton + 20h,
  Laenge + 1Fh). Der 8741 spielt einen Ton selbst; der naechste Befehl
  wartet in seinem Eingangspuffer, bis der laufende fertig ist. tpump
  schickt, was geht, und wartet nie; gwait und gkey rufen es auf. }

procedure tpump;
var i, k: integer;
begin
  for i := 1 to 4 do begin
    if qtp = 0 then begin
      if qtk = qtt then exit;
      if not ghkfrei then exit;
      ghkaus(true, 6);
      qtp := 1;
    end;
    if qtp = 1 then begin
      if not ghkfrei then exit;     { voriger Ton laeuft noch }
      ghkaus(false, qth[qtk] + $20);
      qtp := 2;
    end;
    k := 0;
    while (not ghkfrei) and (k < 200) do k := k + 1;
    if not ghkfrei then exit;
    ghkaus(false, qtl[qtk] + $1F);
    qtk := (qtk + 1) and 31;
    qtp := 0;
  end;
end;

{ Ton h (1-42, 0 Pause) fuer l * 20 ms in die Warteschlange; voll: GE_PLATZ }
procedure tonq(h, l: integer);
begin
  gerr := GE_FORM;
  if (h < 0) or (h > 42) or (l < 1) or (l > 224) then exit;
  gerr := GE_PLATZ;
  if ((qtt + 1) and 31) = qtk then exit;
  qth[qtt] := h; qtl[qtt] := l;
  qtt := (qtt + 1) and 31;
  gerr := 0;
  tpump;
end;

{ Wie tonq (frueher: sofort spielen und dabei warten) }
procedure ton(h, l: integer);
begin
  tonq(h, l);
end;

procedure tonclr;                   { Wartendes verwerfen }
begin
  if qtp = 0 then qtt := qtk
  else qtt := (qtk + 1) and 31;     { angefangenen Ton zu Ende senden }
end;

{ Alles an den 8741 geschickt? (Ob der letzte Ton noch klingt, sieht man
  ihm nicht an.) }
function tonidle: boolean;
begin
  tpump;
  tonidle := (qtk = qtt) and (qtp = 0);
end;

function gkey: integer;
begin
  qzwied;
  tpump;
  gkey := ghtast;
end;

procedure gwait(n: integer);
var k: integer;
begin
  qzwied;
  qflush;
  while n > 0 do begin              { Bildwechsel, dabei Toene schicken }
    k := 0;
    while ((ghstat and $20) <> 0) and (k < 30000) do begin tpump; k := k + 1; end;
    k := 0;
    while ((ghstat and $20) = 0) and (k < 30000) do begin tpump; k := k + 1; end;
    n := n - 1;
  end;
  gerr := 0;
end;

function gdirekt(an: integer): integer;
begin
  if qstart <> 0 then gdirekt := -1
  else begin
    qflush;
    gdirekt := ghdset(an);
  end;
end;

procedure gputs(s: GStr);
begin
  ghtext(s);
  gerr := 0;
end;

{ Zufallszahl fuer den Start: Lage des Bildaufbaus, dazu die Maus }
function grnd: integer;
var n: integer;
begin
  if qstart = 0 then begin
    n := ghvzahl;
    grz := grz * 31421 + n + 6927;
    if (qhw and GH_MAUS) <> 0 then begin
      gmb[0] := 0; gmb[1] := 5;
      if ghmaus(0, gmb) = 0 then
        grz := grz * 75 + (gmb[2] shl 7) + gmb[4] * 13 + (gmb[6] and 7);
    end;
  end;
  grnd := grz and $7FFF;
end;

{ Anfangswerte: Turbo Pascal 3 kennt keine vorbelegten Variablen; das
  erste DMVGRAF-Element im Programm muss darum gstart sein }
procedure gstart;
begin
  qhw := -2; qneb := 3; qcol := 7; qmode := 1; qpatt := 0; qzoom := 0;
  qbcol := -1; qtk := 0; qtt := 0; qtp := 0; qbsch := true;
  qorig := 0; qan := 0; qx := 0; qy := 0; qzs := 0;
  qn := 0; qe := -1; qzeig := false;
  gbstart;
  grz := 1;
  gerr := 0;
end;
