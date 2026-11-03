{ DMVSPR.INC - DMVGRAF fuer Turbo Pascal 3: Figuren ueber GCHRD
  ("Sprites"). Nach DMVGRAF.INC einbinden, wenn gebraucht.

  Die Muster stehen 8 x 8 im Parameter-RAM des 7220, der sie selbst
  zeichnet und vergroessert. Schneller als Bilder, gut fuer kleine
  bewegte Figuren.

  Das Programm legt vorher fest, wie viele Figuren es braucht:
    const SPMAX = 20;          (Figuren 0..SPMAX)

  gsprdef(nr, b, h, muster)   Figur anlegen; b und h je 8 oder 16;
                              muster: je Ebene (gruen, rot, blau) h Zeilen
                              zu b/8 Byte, Bit 0 links. Maske ist das ODER
                              der Ebenen, Schwarz ist also durchsichtig.
  gsprset(nr, b, h, worte)    Figur im Format 5 wie gbdef GB_SETR: integer
                              [Ebene gruen, rot, blau][SET, RESET][Zeile],
                              ein Wort je Zeile, Bit 0 links. Durchsichtig
                              ist nur, was in keiner Ebene gesetzt oder
                              geloescht wird; Schwarz deckt.
                              Die Daten beider Arten bleiben im Programm
                              und muessen erhalten bleiben.
  gspr(nr, x, y, zoom, art)   zeichnen (Ecke wie gbput), zoom 0-15; art
                              GB_XOR umkehren (zweimal = weg), GB_DURCH
                              nur die Figur, GB_DECK ganzes Feld.
                              Muss ganz auf den Schirm passen (GE_LAGE).

  Code: Claude (Anthropic), Idee und Prompts: rfka01. Lizenz: MIT }

var
  qspd: array[0..SPMAX] of GBZeiger;   { Muster des Programms }
  qspb, qsph, qspf: array[0..SPMAX] of byte;
  qspk: array[0..SPMAX] of integer;    { Kennung: angelegt (TP3 loescht nicht) }
  qspm, qspo, qspq: array[0..7] of byte;
  qsps, qspr: array[0..23] of byte;   { je Ebene 8 Zeilen SET, RESET }

procedure qspan(nr, b, h, f: integer; p: GBZeiger);
begin
  gerr := GE_NR;
  if (nr < 0) or (nr > SPMAX) then exit;
  gerr := GE_FORM;
  if ((b <> 8) and (b <> 16)) or ((h <> 8) and (h <> 16)) then exit;
  qspd[nr] := p;
  qspb[nr] := b; qsph[nr] := h; qspf[nr] := f;
  qspk[nr] := $5A3C + nr;
  gerr := 0;
end;

procedure gsprdef(nr, b, h: integer; var mu);
begin
  qspan(nr, b, h, 0, hwAdr(mu));
end;

procedure gsprset(nr, b, h: integer; var wd);
begin
  qspan(nr, b, h, 1, hwAdr(wd));
end;

{ Ein 8 x 8-Feld auf Ebene e mit WDAT-Befehl w; x, y = untere linke
  Ecke (Bildzeile), dort beginnt der 7220 }
procedure qkach(e, w, x, y: integer; var mu);
const fp: array[0..4] of byte = ($12, 7, $40, 8, 0);
begin
  qplatz(48);
  qcmd(w);
  qcurs(qadr(e, x, y), x and 15);
  qcmd($78);
  qpars(mu, 8);
  qcmd($4C);
  qpars(fp, 5);
  qcmd($68);
end;

function qsleer(var mm): boolean;
var m: array[0..7] of byte absolute mm;
    i: integer;
begin
  qsleer := false;
  for i := 0 to 7 do if m[i] <> 0 then exit;
  qsleer := true;
end;

procedure gspr(nr, x, y, z, art: integer);
const wt: array[0..3] of byte = ($20, $23, 0, $21);  { WDAT je Art }
var nc, z1, sx, sy, c, r, e, i, j, k, o, st, ps, es, xt, yt, h: integer;
    f: boolean;
    d, p: GBZeiger;
begin
  gerr := GE_NR;
  if (nr < 0) or (nr > SPMAX) then exit;
  gerr := GE_LEER;
  if qspk[nr] <> $5A3C + nr then exit;
  gerr := GE_ZOOM;
  if (z < 0) or (z > 15) then exit;
  gerr := GE_FORM;
  if (art < 0) or (art > 3) or (art = GB_SCHAT) then exit;
  gerr := qstart;
  if gerr <> 0 then exit;
  d := qspd[nr];
  f := qspf[nr] <> 0;
  nc := qspb[nr] shr 3;
  h := qsph[nr];
  z1 := z + 1;
  sx := x;
  sy := qsy(y);
  if qorig = 0 then sy := sy - (h * z1 - 1);
  gerr := GE_LAGE;
  if (sx < 0) or (sy < 0) or (sx + qspb[nr] * z1 > 640) or (sy + h * z1 > 400) then exit;
  gerr := 0;
  if f then st := 2 else st := nc;  { Byte je Zeile }
  ps := h * st;                     { Abstand SET -> RESET bzw. Ebene }
  es := ps;
  if f then es := 2 * ps;
  qzm(z);
  for r := 0 to (h shr 3) - 1 do
    for c := 0 to nc - 1 do begin
      xt := sx + c * 8 * z1;
      yt := sy + (r + 1) * 8 * z1 - 1;
      for i := 0 to 7 do begin             { Ebenen holen, Maske bilden }
        o := (r * 8 + i) * st + c;
        qspo[i] := 0; qspm[i] := 0;
        k := i;
        while k < 24 do begin
          j := d^[o]; qsps[k] := j; qspo[i] := qspo[i] or j;
          if f then begin j := d^[o + ps]; qspr[k] := j; qspm[i] := qspm[i] or j; end;
          o := o + es; k := k + 8;
        end;
        qspm[i] := qspm[i] or qspo[i];
      end;
      for e := 0 to qneb - 1 do begin
        p := hwAdr(qsps[e * 8]);
        if qneb = 1 then p := hwAdr(qspo);
        if art = GB_DURCH then begin       { erst RESET, dann SET }
          if f and (qneb > 1) then Move(qspr[e * 8], qspq, 8)
          else for i := 0 to 7 do qspq[i] := qspm[i] and not p^[i];
          if not qsleer(qspq) then qkach(e, $22, xt, yt, qspq);
        end;
        if (art = GB_DECK) or not qsleer(p^) then qkach(e, wt[art], xt, yt, p^);
      end;
    end;
  qflush;
end;
