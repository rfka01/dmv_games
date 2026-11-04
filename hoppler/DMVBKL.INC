{ DMVBKL.INC - DMVGRAF fuer Turbo Pascal 3: Bilder, kleine Fassung (fuer
  CP/M-80, wo die ganze Bibliothek nicht in 64 KB passt). Nach
  DMVGRAF.INC einbinden, statt DMVBILD.INC.

  Enthalten: gbdef, gbload, gbldv (Bildsammlung, Formate 1, 2, 4),
  gbput, gbfree, gbinfo mit den Formaten 1, 2, 4 und 5 (5 nur gbdef). Wie viele Bilder es gibt, legt das Programm vorher fest:
  const BKMAX = 16;  (Bilder 0 bis BKMAX - 1). Nicht enthalten (Fehler GE_FORM bzw. GE_LAGE): Format 3
  (gbmap), gbget, glayer/glput/gldraw, gzeig.

  Code: Claude (Anthropic), Idee und Prompts: rfka01. Lizenz: MIT }

var
  bw, bh, bbpr, bnp, bblk, boff, bx0, by0, bx1, by1: array[0..BKMAX] of integer;
  btop: array[0..7] of real;
  bnblk: integer;
  bpuf: array[0..hwQB1] of byte;
  bm, bt, bnul: array[0..79] of byte;
  bp: array[0..2, 0..79] of byte;
  hnr, hn, hpos: integer;
  rmem: GBZeiger;
  rmpos: integer;
  rdat: boolean;
  rbuf: array[0..127] of byte;
  rpos: integer;
  rraw: array[0..319] of byte;
  brest: integer;
  bzr: array[0..319] of byte;
  gbanz: integer;                   { gbldv: geladene Bilder }



{ ------------------------------------------------------------------ }
{ Bilder (GBILD.C)                                                   }

function bnrok(nr: integer): boolean;
begin
  bnrok := (nr >= 0) and (nr < BKMAX);
end;

{ ---- Bildspeicher ---- }

function balloc(nr: integer; n: real): integer;
var k: integer;
begin
  if bnblk < 0 then begin
    bnblk := ghsnum;
    if bnblk > 8 then bnblk := 8;
    for k := 0 to bnblk - 1 do btop[k] := 0.0;
  end;
  balloc := GE_PLATZ;
  for k := 0 to bnblk - 1 do
    if btop[k] + n <= ghsgro(k) then begin
      bblk[nr] := k;
      boff[nr] := rint(btop[k]);
      btop[k] := btop[k] + n;
      balloc := 0;
      exit;
    end;
end;

function bgr(nr: integer): real;    { Groesse eines Bildes }
begin
  bgr := 1.0 * bh[nr] * bnp[nr] * bbpr[nr];
end;

procedure gbfree(nr: integer);
var k, b: integer;
    n, o, rest: real;
begin
  gerr := GE_NR;
  if not bnrok(nr) then exit;
  gerr := 0;
  if bnp[nr] = 0 then exit;
  b := bblk[nr];
  n := bgr(nr);
  o := uwr(boff[nr]);
  rest := btop[b] - o - n;
  if rest > 0 then                  { dahinter liegendes nachruecken }
    ghsmov(b, rint(o), rint(o + n), rint(rest));
  for k := 0 to BKMAX - 1 do
    if (bnp[k] <> 0) and (k <> nr) and (bblk[k] = b) and (uwr(boff[k]) > o) then
      boff[k] := rint(uwr(boff[k]) - n);
  btop[b] := btop[b] - n;
  bnp[nr] := 0;
  gerr := 0;
end;

procedure hflush;
begin
  if hn <> 0 then begin
    ghsput(bblk[hnr], boff[hnr] + hpos, bpuf, hn);
    hpos := hpos + hn;
    hn := 0;
  end;
end;

procedure hput(var s; n: integer);
begin
  if hn + n > QBUF then hflush;
  Move(s, bpuf[hn], n);
  hn := hn + n;
end;

procedure bhol(nr, y, n: integer; var d);   { n Zeilen ab y holen }
var rs: integer;
begin
  rs := bnp[nr] * bbpr[nr];
  ghsget(d, bblk[nr], boff[nr] + y * rs, n * rs);
end;

{ ---- Quelle: Speicher oder Datei ---- }

function brd(var dd; n: integer): boolean;  { n Byte lesen; true = Ende }
var d: GBytes absolute dd;
    i, di: integer;
begin
  brd := false;
  if not rdat then begin
    Move(rmem^[rmpos], d, n);
    rmpos := rmpos + n;
    exit;
  end;
  di := 0;
  while n > 0 do begin
    if rpos >= 128 then begin
      if ghread(rbuf) = 0 then begin brd := true; exit; end;
      rpos := 0;
    end;
    i := 128 - rpos;
    if i > n then i := n;
    Move(rbuf[rpos], d[di], i);
    di := di + i;
    rpos := rpos + i;
    n := n - i;
  end;
end;

{ ---- Bild anlegen ---- }

{ erster und letzter gesetzter Punkt von n Byte, -1 = keiner }
procedure bspan(var zz; n: integer; var lo, hi: integer);
var z: GBytes absolute zz;
    k, v: integer;
begin
  lo := -1; hi := -1;
  k := 0;
  while (k < n) and (lo < 0) do begin
    v := z[k];
    if v <> 0 then
      if (v and 15) <> 0 then lo := k * 8 + bnlo[v and 15]
      else lo := k * 8 + 4 + bnlo[v shr 4];
    k := k + 1;
  end;
  if lo < 0 then exit;
  k := n - 1;
  while (k >= 0) and (hi < 0) do begin
    v := z[k];
    if v <> 0 then
      if (v shr 4) <> 0 then hi := k * 8 + 4 + bnhi[v shr 4]
      else hi := k * 8 + bnhi[v and 15];
    k := k - 1;
  end;
end;

procedure bbox(nr: integer; var z; y: integer);
var lo, hi: integer;
begin
  bspan(z, bbpr[nr], lo, hi);
  if hi < 0 then exit;
  if lo < bx0[nr] then bx0[nr] := lo;
  if hi > bx1[nr] then bx1[nr] := hi;
  if y < by0[nr] then by0[nr] := y;
  if y > by1[nr] then by1[nr] := y;
end;

function brev(v: integer): integer;  { Bitfolge eines Bytes umdrehen }
var r, j: integer;
begin
  r := 0;
  for j := 0 to 7 do
    if (v and (1 shl j)) <> 0 then r := r or ($80 shr j);
  brev := r;
end;

function bfmt12(nr, fmt: integer): integer;
var y, k, n: integer;
begin
  bfmt12 := GE_KURZ;
  n := bbpr[nr];
  for y := 0 to bh[nr] - 1 do begin
    if brd(rraw, n) then exit;
    for k := 0 to n - 1 do begin
      if fmt = GB_BIT7 then rraw[k] := brev(rraw[k]);
      if k = n - 1 then rraw[k] := rraw[k] and brest;
    end;
    bbox(nr, rraw, y);
    hput(rraw, n);
  end;
  bfmt12 := 0;
end;

function bfmt4(nr: integer): integer;
var y, k, n, e: integer;
begin
  bfmt4 := GE_KURZ;
  n := bbpr[nr];
  for y := 0 to bh[nr] - 1 do begin
    if brd(rraw, 4 * n) then exit;
    rraw[n - 1] := rraw[n - 1] and brest;
    bbox(nr, rraw, y);
    hput(rraw, n);
    if qneb = 3 then
      for e := 0 to 2 do begin
        for k := 0 to n - 1 do bt[k] := rraw[(e + 1) * n + k] and rraw[k];
        hput(bt, n);
      end
    else begin
      for k := 0 to n - 1 do
        bt[k] := (rraw[n + k] or rraw[2 * n + k] or rraw[3 * n + k]) and rraw[k];
      hput(bt, n);
    end;
  end;
  bfmt4 := 0;
end;

{ Format 5: Worte, [Ebene gruen/rot/blau][SET, RESET][Zeile][Wort] }
function bfmt5(nr: integer): integer;
var y, k, n, e, wpr, s, r, i, ws, wr, pe: integer;
begin
  n := bbpr[nr];
  wpr := (bw[nr] + 15) shr 4;
  for y := 0 to bh[nr] - 1 do begin
    for k := 0 to n - 1 do begin
      bm[k] := 0; bp[0, k] := 0; bp[1, k] := 0; bp[2, k] := 0;
    end;
    for e := 0 to 2 do
      for k := 0 to wpr - 1 do begin
        i := ((e * 2) * bh[nr] + y) * wpr + k;
        ws := rmem^[rmpos + 2 * i] + 256 * rmem^[rmpos + 2 * i + 1];
        i := i + bh[nr] * wpr;
        wr := rmem^[rmpos + 2 * i] + 256 * rmem^[rmpos + 2 * i + 1];
        if qneb = 3 then pe := e else pe := 0;
        s := 0;
        while (s < 2) and (2 * k + s < n) do begin
          if s <> 0 then r := Hi(ws) else r := Lo(ws);
          if s <> 0 then bm[2 * k + s] := bm[2 * k + s] or r or Hi(wr)
          else bm[2 * k + s] := bm[2 * k + s] or r or Lo(wr);
          bp[pe, 2 * k + s] := bp[pe, 2 * k + s] or r;
          s := s + 1;
        end;
      end;
    bm[n - 1] := bm[n - 1] and brest;
    bbox(nr, bm, y);
    hput(bm, n);
    for e := 0 to qneb - 1 do hput(bp[e, 0], n);
  end;
  bfmt5 := 0;
end;

{ Bild nr anlegen; die Daten kommen ueber brd() oder bei Format 5 aus
  rmem (Worte niederwertiges Byte zuerst, wie Turbo Pascal sie ablegt) }
function bdef(nr, b, h, fmt: integer): integer;
var r: integer;
begin
  if not bnrok(nr) then begin bdef := GE_NR; exit; end;
  if (b < 1) or (b > 640) or (h < 1) or (h > 400) or (fmt < 1) or (fmt > 5) then begin
    bdef := GE_FORM; exit;
  end;
  gbfree(nr);
  bw[nr] := b;
  bh[nr] := h;
  bbpr[nr] := (b + 7) shr 3;
  if (fmt = GB_BIT7) or (fmt = GB_BIT0) then bnp[nr] := 1 else bnp[nr] := qneb + 1;
  if (b and 7) <> 0 then brest := (1 shl (b and 7)) - 1 else brest := $FF;
  if balloc(nr, bgr(nr)) <> 0 then begin
    bnp[nr] := 0; bdef := GE_PLATZ; exit;
  end;
  bx0[nr] := 9999; by0[nr] := 9999;
  bx1[nr] := -1; by1[nr] := -1;
  hnr := nr; hn := 0; hpos := 0;
  case fmt of
    GB_NIB:  r := GE_FORM;              { Format 3 nur in DMVBILD.INC }
    GB_EBN:  r := bfmt4(nr);
    GB_SETR: r := bfmt5(nr);
  else r := bfmt12(nr, fmt);
  end;
  hflush;
  if r <> 0 then gbfree(nr);
  bdef := r;
end;

procedure gbdef(nr, b, h, fmt: integer; var daten);
begin
  gerr := GE_HW;
  if qstart <> 0 then exit;
  rdat := false;
  rmem := hwAdr(daten);
  rmpos := 0;
  gerr := bdef(nr, b, h, fmt);
end;

{ Bilder aus einer Datei. b = 0: Bildsammlung (gbldv), jedes Bild mit
  eigenem Kopf: Breite, Hoehe (je 2 Byte), Format, 0; dann die Daten wie
  bei gbdef. anz <= 0: bis zum Ende der Datei. gbanz: geladene Bilder. }
procedure gbload(nr0: integer; name: GStr; b, h, anz, fmt: integer; kopf: real);
var i, r, n: integer;
begin
  gbanz := 0;
  gerr := GE_HW;
  if qstart <> 0 then exit;
  gerr := GE_DATEI;
  if ghopen(name) <> 0 then exit;
  rdat := true;
  rpos := 128;
  r := 0;
  while (kopf >= 0.5) and (r = 0) do begin   { Kopf ueberspringen, auch > 32 KB }
    if kopf > 128 then n := 128 else n := Round(kopf);
    if brd(bzr, n) then r := GE_KURZ;
    kopf := kopf - n;
  end;
  i := 0;
  while (r = 0) and ((i < anz) or ((anz <= 0) and (b = 0))) do begin
    n := b;
    if b = 0 then begin                     { Kopf des Bildes }
      n := 0;
      if not brd(bzr, 6) then begin
        n := bzr[0] + 256 * bzr[1]; h := bzr[2] + 256 * bzr[3]; fmt := bzr[4];
      end;
      if (n < 1) or (n > 640) then r := -1; { Ende (auch ^Z am Satzende) }
    end;
    if fmt = GB_SETR then r := GE_FORM;
    if r = 0 then r := bdef(nr0 + i, n, h, fmt);
    if r = 0 then gbanz := gbanz + 1;
    i := i + 1;
  end;
  if r = -1 then if anz > 0 then r := GE_KURZ else r := 0;
  ghclos;
  rdat := false;
  gerr := r;
end;

procedure gbldv(nr0: integer; name: GStr; anz: integer; kopf: real);
begin
  gbload(nr0, name, 0, 0, anz, 0, kopf);
end;

procedure gbinfo(nr: integer; var r: GBInfoArr);
begin
  gerr := GE_NR;
  if not bnrok(nr) then exit;
  gerr := GE_LEER;
  if bnp[nr] = 0 then exit;
  r[0] := bw[nr]; r[1] := bh[nr];
  r[2] := bx0[nr]; r[3] := by0[nr]; r[4] := bx1[nr]; r[5] := by1[nr];
  gerr := 0;
end;

{ ---- Zeichnen ---- }

{ Geholte Zeile z fuer Art art als zusammenhaengende Zeile in bzr:
  Maske, im Abstand n die Ebenen }
procedure bzei(nr: integer; var zz; art, sx, sy: integer);
var z: GBytes absolute zz;
    k, n, e, pat, s: integer;
begin
  n := bbpr[nr];
  if bnp[nr] = 1 then begin
    Move(z, bzr, n);
    for e := 0 to qneb - 1 do
      if qebon(e, qcol) then Move(z, bzr[(e + 1) * n], n)
      else Move(bnul, bzr[(e + 1) * n], n);
  end else Move(z, bzr, (qneb + 1) * n);
  if art = GB_DECK then begin
    for k := 0 to n - 1 do bzr[k] := $FF;
    if (bw[nr] and 7) <> 0 then bzr[n - 1] := (1 shl (bw[nr] and 7)) - 1
    else bzr[n - 1] := $FF;
  end;
  if art = GB_SCHAT then begin
    if ((sx + sy) and 1) <> 0 then pat := $55 else pat := $AA;
    for k := 0 to n - 1 do
      if qneb = 3 then begin
        s := bzr[n + k] and bzr[2 * n + k] and bzr[3 * n + k] and pat;
        bzr[n + k] := bzr[n + k] and (not s) and $FF;
        bzr[2 * n + k] := bzr[2 * n + k] and (not s) and $FF;
      end else bzr[n + k] := bzr[n + k] and (not pat) and $FF;
  end;
end;

{ bzei liefert in C die Zeile selbst, wo keine Kopie noetig ist
  (Ebenenbild, Art 1, 3); hier: true = z unveraendert benutzen }
function bdirekt(nr, art: integer): boolean;
begin
  bdirekt := (bnp[nr] <> 1) and ((art = GB_DURCH) or (art = GB_XOR));
end;

procedure bkopz(var zz; n: integer);   { Zeile nach bm, bp (fuer gbput) }
var z: GBytes absolute zz;
    e: integer;
begin
  Move(z, bm, n);
  for e := 0 to qneb - 1 do Move(z[(e + 1) * n], bp[e, 0], n);
end;

procedure bcut(var dd; var ss; off, n: integer);   { Bits off.. von s }
begin
  ghzbits(dd, ss, off, n, 1);
end;

procedure bzoom(var dd; var ss; n, z1: integer);   { jedes Bit z1-mal }
begin
  ghzbits(dd, ss, 0, n, z1);
end;

{ Zeile (bm, bp; nb Bit ab Punkt sx) auf den Schirm }
procedure bzeig(sy, sx, nb, art: integer);
var e, ad: integer;
begin
  if (nb <= 0) or (sy < 0) or (sy > 399) then exit;
  qplatz(48 + (nb shr 2));          { auch: Mauszeiger weg }
  ad := qadr(0, sx, sy) - qebad[0];
  e := 0;
  repeat                            { alle Ebenen, wenn noetig in Stuecken }
    e := ghbrow(ql, qn, qe, bp, bm, e, qneb, 80, ad, sx, nb, art, qebad);
    if e < qneb then qflush;
  until e >= qneb;
end;

function bvor(nr, art: integer): integer;
begin
  if not bnrok(nr) then bvor := GE_NR
  else if bnp[nr] = 0 then bvor := GE_LEER
  else if (art < 0) or (art > 3) then bvor := GE_FORM
  else begin
    FillChar(bnul, 80, 0);
    bvor := 0;
  end;
end;

procedure gbput(nr, art: integer);
var z1, ww, hh, sx, sy, y, n, zr, rs, k, e, c0, c1, ys, nb, zo: integer;
    fertig: boolean;
begin
  gerr := bvor(nr, art);
  if gerr <> 0 then exit;
  z1 := qzoom + 1;
  ww := bw[nr] * z1;
  hh := bh[nr] * z1;
  sx := qx;
  sy := qsy(qy);
  if qorig = 0 then sy := sy - (hh - 1);
  if sx < 0 then c0 := -sx else c0 := 0;
  if sx + ww > 640 then c1 := 640 - sx else c1 := ww;
  if c0 >= c1 then exit;
  rs := bnp[nr] * bbpr[nr];
  if qbsch and (z1 = 1) and ((sx and 15) = 0) and (c0 = 0) and (c1 = ww) and
     (sy >= 0) and (sy + hh <= 400) and (art <> GB_SCHAT) and
     ((art <> GB_DECK) or ((bw[nr] and 15) = 0)) and
     ((bnp[nr] = 1) or (bnp[nr] = qneb + 1)) then begin
    { schneller Weg wie HOPPLERs BlitAsm: ganze Worte, ohne Zoom und
      Beschnitt, direkt aus dem Bildspeicher an den 7220 }
    qplatz(0);                      { Mauszeiger weg }
    qflush;
    qbtupel(bnp[nr], bbpr[nr], art);
    k := bw[nr] and 15;
    if k = 0 then k := -1 else k := (1 shl k) - 1;
    ghblit(bblk[nr], boff[nr], bh[nr], rs, (bbpr[nr] + 1) shr 1, k,
           qadr(0, sx, sy) - qebad[0], qbtu, qbtn);
    exit;
  end;
  y := 0;
  while y < bh[nr] do begin
    n := QBUF div rs;
    if n > bh[nr] - y then n := bh[nr] - y;
    bhol(nr, y, n, bpuf);
    for k := 0 to n - 1 do begin
      zo := k * rs;
      fertig := false;
      for zr := 0 to z1 - 1 do begin
        ys := sy + (y + k) * z1 + zr;
        if (ys >= 0) and (ys <= 399) then begin
          { die Zeile nur einmal aufbereiten, ausser fuer den Schatten,
            dessen Raster von ys abhaengt }
          if fertig and (art <> GB_SCHAT) then
          else begin
          fertig := true;
          if bdirekt(nr, art) then bkopz(bpuf[zo], bbpr[nr])
          else begin
            bzei(nr, bpuf[zo], art, sx, ys);
            bkopz(bzr, bbpr[nr]);
          end;
          nb := bw[nr];
          if z1 > 1 then begin
            bzoom(bt, bm, nb, z1);
            Move(bt, bm, 80);
            for e := 0 to qneb - 1 do begin
              bzoom(bt, bp[e, 0], nb, z1);
              Move(bt, bp[e, 0], 80);
            end;
            if ww > 640 then nb := 640 else nb := ww;
          end;
          if (c0 > 0) or (c1 < nb) then begin
            bcut(bt, bm, c0, c1 - c0);
            Move(bt, bm, 80);
            for e := 0 to qneb - 1 do begin
              bcut(bt, bp[e, 0], c0, c1 - c0);
              Move(bt, bp[e, 0], 80);
            end;
          end;
          end;
          bzeig(ys, sx + c0, c1 - c0, art);
        end;
      end;
    end;
    y := y + n;
  end;
  qflush;
end;
procedure gbmap(f, a, b, m: integer);
begin
  gerr := GE_FORM;
end;

procedure gbget(nr, b, h: integer);
begin
  gerr := GE_FORM;
end;

procedure glayer(x0, y0, x1, y1: integer);
begin
  gerr := GE_LAGE;
end;

procedure glput(nr, x, y, art: integer);
begin
  gerr := GE_LAGE;
end;

procedure gldraw;
begin
  gerr := GE_LAGE;
end;

procedure qzhid;
begin
  qzeig := false;
end;

procedure qzwied;
begin
end;

procedure gzeig;
begin
  if nr < 0 then gerr := 0 else gerr := GE_FORM;
end;

procedure gbstart;                  { Anfangswerte (aus gstart) }
begin
  bnblk := -1;
  FillChar(bnp, SizeOf(bnp), 0);
  rdat := false; rmpos := 0;
end;
