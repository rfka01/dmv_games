{ DMVHWD.INC - DMVGRAF fuer Turbo Pascal 3: Hardwareschicht MS-DOS
  (8088/V20 der K230/K231/K235).

  Dasselbe wie GHWDOS.C der C-Fassung: 7220 (A0h-A2h), Maus K806
  (Steckplatz 2A, 30h-33h) und Tastatur-8741 (40h/41h) direkt,
  Tastatur, Bildschirmtext und Dateien ueber MsDos, Bildspeicher in
  Bloecken zu 64 KB von DOS (Funktion 48h).

  Reihenfolge im Programm: DMVHWD.INC, DMVFONT.INC, DMVGRAF.INC.

  Code: Claude (Anthropic), Idee und Prompts: rfka01. Lizenz: MIT }

const
  hwPRES: integer = 0;              { nur CP/M-80 (DMVHWC.INC); hier ohne Wirkung }
type
  GBytes  = array[0..32767] of byte;
  GStr    = string[255];
  GBZeiger = ^GBytes;
  GDosReg = record AX, BX, CX, DX, BP, SI, DI, DS, ES, Flags: integer; end;

const
  hwHSH    = 4;                     { MaxAvail in Einheiten von 2^hwHSH Byte }
  GH_FARBE = 1;  GH_MAUS = 2;  GH_SYN = 4;  GH_COVOX = 8;
  hwZEIT   = 30000;                 { Wartegrenze in Abfragen }
  hwGBBLK  = 2;                     { hoechstens so viele 64-KB-Bloecke }
  hwQL     = 1024;                  { GDC-Liste }
  hwQB     = 1280; hwQB1 = 1279;    { Bildzeilenpuffer (mindestens 320) }

var
  hwK806, hwFarbe: integer;
  hwFh: integer;                    { offene Datei, -1 keine }
  hwBn: integer;                    { Bildspeicherbloecke, -1 = noch nicht }
  hwBseg: array[0..1] of integer;
  hwR: GDosReg;
  hwEad: array[0..3] of byte;
  hwWb: array[0..1] of byte;
  hwSrc, hwDst, hwL: array[0..1] of integer;   { fuer die Maschinenbefehle }
  hwQn, hwQe, hwAd, hwSx, hwNb, hwW, hwAa, hwLz, hwX, hwSh, hwRe, hwK2,
  hwMsk, hwFFo, hwOr, hwHl: integer;
  hwWst: boolean;                   { MASK FFFF und FIGS fuer WDAT gesetzt }
  hwRomz, hwSchirm: integer;        { Kennbuchstabe der Firmware, Schirm 1-3 }
  hwT, hwFF: array[0..79] of byte;
  hwXs, hwXo, hwXn, hwXr, hwXw, hwXm, hwXa, hwXt, hwXk, hwXz: integer;
  hwXu: array[0..35] of integer;     { Tabelle fuer ghblit }

{ ---- 7220 ---- }

procedure hwGw;                     { bis das FIFO nicht voll ist }
var n: integer;
begin
  n := 0;
  while ((Port[$A0] and 2) <> 0) and (n < hwZEIT) do n := n + 1;
end;

procedure hwCmd(c: integer);
begin
  hwWst := false;
  hwGw; Port[$A1] := c;
end;

procedure hwPar(c: integer);
begin
  hwGw; Port[$A0] := c;
end;

procedure hwParZ(c: integer);       { ZOOM-Parameter ueber A2h }
begin
  hwGw; Port[$A2] := c;
end;

function hwLies(m: integer; var d): integer;  { m Bytes aus dem FIFO }
var b: GBytes absolute d;
    i, n: integer;
begin
  hwLies := 0;
  for i := 0 to m - 1 do begin
    n := 0;
    while ((Port[$A0] and 1) = 0) and (n < hwZEIT) do n := n + 1;
    if n >= hwZEIT then begin hwLies := 1; exit; end;
    b[i] := Port[$A1];
  end;
end;

{ GDC-Liste wie BIOS 66: Eintraege n, Befehl, n-1 Parameter; 0 Ende }
{ GDC-Liste ausgeben (n, Befehl, n-1 Parameter; 0 = Ende). In
  Maschinensprache wie in DMVHWC.INC. }
procedure ghlist(var l);
begin
  hwSrc[0] := Ofs(l); hwSrc[1] := Seg(l);
  inline(
    $FC/                              { cld }
    $8B/$36/>hwSrc/                   { mov si,[hwSrc] }
    $A1/>hwSrc+2/                     { mov ax,[hwSrc+2] }
    $1E/                              { push ds }
    $8E/$D8/                          { mov ds,ax }
    $AC/                              { satz:   lodsb }
    $08/$C0/                          { or al,al }
    $74/$1E/                          { jz ende }
    $88/$C1/                          { mov cl,al }
    $E4/$A0/                          { w1:     in al,0A0h }
    $A8/$02/                          { test al,2 }
    $75/$FA/                          { jnz w1 }
    $AC/                              { lodsb }
    $E6/$A1/                          { out 0A1h,al }
    $FE/$C9/                          { dec cl }
    $74/$EC/                          { jz satz }
    $E4/$A0/                          { w2:     in al,0A0h }
    $A8/$02/                          { test al,2 }
    $75/$FA/                          { jnz w2 }
    $AC/                              { lodsb }
    $E6/$A0/                          { out 0A0h,al }
    $FE/$C9/                          { dec cl }
    $75/$F3/                          { jnz w2 }
    $EB/$DD/                          { jmp short satz }
    $1F                               { ende:   pop ds }
  );
end;

{ Eintrag fuer eine Bitzeile an die GDC-Liste lst (ab qn) haengen:
  CURS adr, MASK FFFF, FIGS, WDAT w mit den Worten (MASK und FIGS
  entfallen, solange sie vom letzten Eintrag her gelten). nb Bit ab
  Punkt sx aus bb; a: 0 Bits, 1 umgekehrt, 2 lauter Einsen, 3 lauter
  Nullen, 4 mm and not bb, 5 mm and bb (mm: Maske, sonst unbenutzt),
  6 beides nacheinander: w mit mm and not bb, dann w + 1 mit mm and bb.
  Bleibt bei w <> 20h nichts zu tun, faellt der Eintrag wieder weg
  (qe := -1). Platz muss vorher geschaffen sein. In Maschinensprache:
  das ist die meistbenutzte Stelle der Bibliothek. }
procedure hwEin;                    { ein Eintrag nach den hw-Variablen }
begin
  inline(
    $FC/                              { cld }
    $55/                              { push bp }
    $8C/$D8/                          { mov ax,ds }
    $8E/$C0/                          { mov es,ax }
    $8B/$1E/>hwSx/                    { mov bx,[hwSx] }
    $83/$E3/$0F/                      { and bx,15 }
    $A1/>hwNb/                        { mov ax,[hwNb] }
    $01/$D8/                          { add ax,bx }
    $83/$C0/$0F/                      { add ax,15 }
    $B1/$04/                          { mov cl,4 }
    $D3/$E8/                          { shr ax,cl }
    $D1/$E0/                          { shl ax,1 }
    $89/$C5/                          { mov bp,ax }
    $A3/>hwK2/                        { mov [hwK2],ax }
    $8B/$16/>hwNb/                    { mov dx,[hwNb] }
    $8B/$3E/>hwL/                     { mov di,[hwL] }
    $03/$3E/>hwQn/                    { add di,[hwQn] }
    $80/$3E/>hwWst/$00/               { cmp byte [hwWst],0 }
    $74/$0D/                          { jz kl }
    $B8/$03/$49/                      { mov ax,4903h }
    $AB/                              { stosw }
    $A1/>hwAd/                        { mov ax,[hwAd] }
    $AB/                              { stosw }
    $B9/$04/$00/                      { mov cx,4 }
    $EB/$21/                          { jmp short kw }
    $B8/$04/$49/                      { kl:     mov ax,4904h }
    $AB/                              { stosw }
    $A1/>hwAd/                        { mov ax,[hwAd] }
    $AB/                              { stosw }
    $30/$C0/                          { xor al,al }
    $AA/                              { stosb }
    $B8/$03/$4A/                      { mov ax,4A03h }
    $AB/                              { stosw }
    $B8/$FF/$FF/                      { mov ax,0FFFFh }
    $AB/                              { stosw }
    $B8/$04/$4C/                      { mov ax,4C04h }
    $AB/                              { stosw }
    $B8/$02/$00/                      { mov ax,2 }
    $AB/                              { stosw }
    $30/$C0/                          { xor al,al }
    $AA/                              { stosb }
    $B9/$0E/$00/                      { mov cx,14 }
    $89/$0E/>hwHl/                    { kw:     mov [hwHl],cx }
    $89/$E8/                          { mov ax,bp }
    $FE/$C0/                          { inc al }
    $AA/                              { stosb }
    $A0/>hwW/                         { mov al,[hwW] }
    $AA/                              { stosb }
    $A1/>hwQn/                        { mov ax,[hwQn] }
    $01/$C8/                          { add ax,cx }
    $A3/>hwQe/                        { mov [hwQe],ax }
    $83/$C0/$02/                      { add ax,2 }
    $A3/>hwQn/                        { mov [hwQn],ax }
    $09/$DB/                          { or bx,bx }
    $75/$22/                          { jnz lang }
    $80/$3E/>hwAa/$00/                { cmp byte [hwAa],0 }
    $75/$1B/                          { jnz lang }
    $A1/>hwMsk/                       { mov ax,[hwMsk] }
    $3B/$06/>hwFFo/                   { cmp ax,[hwFFo] }
    $75/$12/                          { jnz lang }
    $F6/$C2/$0F/                      { test dl,15 }
    $75/$0D/                          { jnz lang }
    $89/$E9/                          { mov cx,bp }
    $1E/                              { push ds }
    $C5/$36/>hwSrc/                   { lds si,[hwSrc] }
    $F3/$A4/                          { rep movsb }
    $1F/                              { pop ds }
    $E9/$DA/$00/                      { jmp dazu }
    $89/$D0/                          { mov ax,dx }
    $83/$C0/$07/                      { add ax,7 }
    $B1/$03/                          { mov cl,3 }
    $D3/$E8/                          { shr ax,cl }
    $89/$D1/                          { mov cx,dx }
    $80/$E1/$07/                      { and cl,7 }
    $B2/$FF/                          { mov dl,0FFh }
    $74/$06/                          { jz lz }
    $B2/$01/                          { mov dl,1 }
    $D2/$E2/                          { shl dl,cl }
    $FE/$CA/                          { dec dl }
    $88/$16/>hwLz/                    { lz:     mov [hwLz],dl }
    $B6/$00/                          { mov dh,0 }
    $80/$3E/>hwAa/$00/                { cmp byte [hwAa],0 }
    $74/$02/                          { jz ux }
    $B6/$FF/                          { mov dh,0FFh }
    $88/$36/>hwX/                     { ux:     mov [hwX],dh }
    $89/$E9/                          { mov cx,bp }
    $80/$FB/$08/                      { cmp bl,8 }
    $72/$05/                          { jb nv }
    $C6/$05/$00/                      { mov byte [di],0 }
    $47/                              { inc di }
    $49/                              { dec cx }
    $80/$E3/$07/                      { nv:     and bl,7 }
    $88/$1E/>hwSh/                    { mov [hwSh],bl }
    $89/$C6/                          { mov si,ax }
    $39/$F1/                          { cmp cx,si }
    $73/$07/                          { jae vok }
    $89/$CE/                          { mov si,cx }
    $C6/$06/>hwLz/$FF/                { mov byte [hwLz],0FFh }
    $29/$F1/                          { vok:    sub cx,si }
    $89/$0E/>hwRe/                    { mov [hwRe],cx }
    $C6/$06/>hwOr/$00/                { mov byte [hwOr],0 }
    $8A/$16/>hwLz/                    { mov dl,[hwLz] }
    $8A/$36/>hwX/                     { mov dh,[hwX] }
    $8A/$0E/>hwSh/                    { mov cl,[hwSh] }
    $89/$F5/                          { mov bp,si }
    $30/$ED/                          { xor ch,ch }
    $8B/$1E/>hwMsk/                   { mov bx,[hwMsk] }
    $C4/$36/>hwSrc/                   { les si,[hwSrc] }
    $4D/                              { dec bp }
    $74/$19/                          { jz letzt }
    $26/$AC/                          { lp:     es lodsb }
    $30/$F0/                          { xor al,dh }
    $22/$07/                          { and al,[bx] }
    $43/                              { inc bx }
    $30/$E4/                          { xor ah,ah }
    $D3/$E0/                          { shl ax,cl }
    $08/$E8/                          { or al,ch }
    $88/$E5/                          { mov ch,ah }
    $88/$05/                          { mov [di],al }
    $47/                              { inc di }
    $08/$06/>hwOr/                    { or [hwOr],al }
    $4D/                              { dec bp }
    $75/$E7/                          { jnz lp }
    $26/$AC/                          { letzt:  es lodsb }
    $30/$F0/                          { xor al,dh }
    $22/$07/                          { and al,[bx] }
    $20/$D0/                          { and al,dl }
    $30/$E4/                          { xor ah,ah }
    $D3/$E0/                          { shl ax,cl }
    $08/$E8/                          { or al,ch }
    $88/$E5/                          { mov ch,ah }
    $88/$05/                          { mov [di],al }
    $47/                              { inc di }
    $08/$06/>hwOr/                    { or [hwOr],al }
    $88/$E8/                          { mov al,ch }
    $8B/$0E/>hwRe/                    { mov cx,[hwRe] }
    $E3/$11/                          { jcxz rf }
    $88/$05/                          { mov [di],al }
    $47/                              { inc di }
    $08/$06/>hwOr/                    { or [hwOr],al }
    $49/                              { dec cx }
    $E3/$07/                          { jcxz rf }
    $30/$C0/                          { xor al,al }
    $88/$05/                          { rz:     mov [di],al }
    $47/                              { inc di }
    $E2/$FB/                          { loop rz }
    $80/$3E/>hwOr/$00/                { rf:     cmp byte [hwOr],0 }
    $75/$19/                          { jnz dazu }
    $80/$3E/>hwW/$20/                 { cmp byte [hwW],20h }
    $74/$12/                          { jz dazu }
    $A1/>hwQe/                        { mov ax,[hwQe] }
    $2B/$06/>hwHl/                    { sub ax,[hwHl] }
    $A3/>hwQn/                        { mov [hwQn],ax }
    $C7/$06/>hwQe/$FF/$FF/            { mov word [hwQe],0FFFFh }
    $EB/$0A/                          { jmp short ende }
    $A1/>hwQn/                        { dazu:   mov ax,[hwQn] }
    $03/$06/>hwK2/                    { add ax,[hwK2] }
    $A3/>hwQn/                        { mov [hwQn],ax }
    $5D                               { ende:   pop bp }
  );
  if hwQe <> -1 then hwWst := true;
end;

procedure ghrow(var lst; var qn, qe: integer; adr, sx, nb, w, a: integer;
                var bb; var mm);
begin
  hwL[0] := Ofs(lst);                   { GDC-Liste und Maske im Datensegment }
  hwSrc[0] := Ofs(bb); hwSrc[1] := Seg(bb);
  hwFFo := Ofs(hwFF); hwMsk := hwFFo;
  case a of
    2: begin FillChar(hwT, 80, $FF); hwSrc[0] := Ofs(hwT); hwSrc[1] := Seg(hwT); a := 0; end;
    3: begin FillChar(hwT, 80, 0); hwSrc[0] := Ofs(hwT); hwSrc[1] := Seg(hwT); a := 0; end;
    4: begin hwMsk := Ofs(mm); a := 1; end;
    5: begin hwMsk := Ofs(mm); a := 0; end;
  end;
  hwQn := qn; hwQe := qe; hwAd := adr; hwSx := sx; hwNb := nb; hwW := w;
  if a = 6 then begin
    hwMsk := Ofs(mm);
    hwAa := 1; hwEin;               { Maske ohne Punkte: aus }
    hwAa := 0; hwW := w + 1; hwEin; { Maske mit Punkten: an }
  end else begin
    hwAa := a; hwEin;
  end;
  qn := hwQn; qe := hwQe;
end;

{ Eine Bildzeile fuer alle Ebenen ab e0 an die Liste haengen: Ebene e
  aus pl + e * st, Maske mk; Wortadresse ebad[e] + ad. art wie gbput
  (deckend nur fuer ganze Worte). Liefert die naechste Ebene; ist die
  Liste voll, kleiner als ne (dann ausgeben und weiter). }
function ghbrow(var lst; var qn, qe: integer; var pl; var mk;
                e0, ne, st, ad, sx, nb, art: integer; var ebad): integer;
var eb: array[0..2] of integer absolute ebad;
    e: integer;
begin
  hwL[0] := Ofs(lst); hwQn := qn; hwQe := qe;
  hwSx := sx; hwNb := nb;
  hwFFo := Ofs(hwFF);
  hwSrc[0] := Ofs(pl) + e0 * st; hwSrc[1] := Seg(pl);
  e := e0;
  while (e < ne) and (hwQn + 48 + (nb shr 2) < hwQL) do begin
    hwAd := eb[e] + ad;
    if (art = 0) and (((sx or nb) and 15) = 0) then begin
      hwMsk := hwFFo; hwAa := 0; hwW := $20; hwEin;
    end else if art = 3 then begin
      hwMsk := Ofs(mk); hwAa := 0; hwW := $21; hwEin;
    end else begin
      hwMsk := Ofs(mk);
      hwAa := 1; hwW := $22; hwEin;
      hwAa := 0; hwW := $23; hwEin;
    end;
    e := e + 1; hwSrc[0] := hwSrc[0] + st;
  end;
  qn := hwQn; qe := hwQe;
  ghbrow := e;
end;

{ Ganzes Bild direkt an den 7220, wie HOPPLERs BlitAsm: einmal MASK FFFF
  und FIGS, dann je Zeile und je Eintrag der Tabelle tu (nt Eintraege zu
  6 Worten: Ebenenadresse, WDAT-Befehl, Maske 0 = aus der Zeile oder
  FFh, XOR fuer die Ebene, Versatz der Ebene in der Zeile, 1 = lauter
  Nullen) CURS ohne Punkt, WDAT und die nw Worte. Quelle: Block b ab o,
  n Zeilen im Abstand rs; lm maskiert das letzte Wort jeder Zeile; ad =
  Wortadresse der ersten Zeile ohne Ebene. Maschinensprache. }
procedure ghblit(b, o, n, rs, nw, lm, ad: integer; var tu; nt: integer);
begin
  hwXs := hwBseg[b]; hwXo := o; hwXn := n; hwXr := rs; hwXw := nw;
  hwXm := lm; hwXa := ad; hwXt := nt;
  Move(tu, hwXu, nt * 12);
  inline(
    $55/                              { anfang: push    bp }
    $06/                              { push    es }
    $8E/$06/>hwXs/                    { mov     es, [hwXs] }
    $8B/$36/>hwXo/                    { mov     si, [hwXo] }
    $B0/$4A/                          { mov     al, 4Ah }
    $88/$C4/                          { %%w: mov ah, al }
    $E4/$A0/                          { in al, 0A0h }
    $A8/$02/                          { test al, 2 }
    $88/$E0/                          { mov al, ah }
    $75/$F6/                          { jnz %%w }
    $E6/$A1/                          { out 0A1h, al }
    $B0/$FF/                          { mov     al, 0FFh }
    $88/$C4/                          { %%w: mov ah, al }
    $E4/$A0/                          { in al, 0A0h }
    $A8/$02/                          { test al, 2 }
    $88/$E0/                          { mov al, ah }
    $75/$F6/                          { jnz %%w }
    $E6/$A0/                          { out 0A0h, al }
    $88/$C4/                          { %%w: mov ah, al }
    $E4/$A0/                          { in al, 0A0h }
    $A8/$02/                          { test al, 2 }
    $88/$E0/                          { mov al, ah }
    $75/$F6/                          { jnz %%w }
    $E6/$A0/                          { out 0A0h, al }
    $B0/$4C/                          { mov     al, 4Ch }
    $88/$C4/                          { %%w: mov ah, al }
    $E4/$A0/                          { in al, 0A0h }
    $A8/$02/                          { test al, 2 }
    $88/$E0/                          { mov al, ah }
    $75/$F6/                          { jnz %%w }
    $E6/$A1/                          { out 0A1h, al }
    $B0/$02/                          { mov     al, 2 }
    $88/$C4/                          { %%w: mov ah, al }
    $E4/$A0/                          { in al, 0A0h }
    $A8/$02/                          { test al, 2 }
    $88/$E0/                          { mov al, ah }
    $75/$F6/                          { jnz %%w }
    $E6/$A0/                          { out 0A0h, al }
    $30/$C0/                          { xor     al, al }
    $88/$C4/                          { %%w: mov ah, al }
    $E4/$A0/                          { in al, 0A0h }
    $A8/$02/                          { test al, 2 }
    $88/$E0/                          { mov al, ah }
    $75/$F6/                          { jnz %%w }
    $E6/$A0/                          { out 0A0h, al }
    $88/$C4/                          { %%w: mov ah, al }
    $E4/$A0/                          { in al, 0A0h }
    $A8/$02/                          { test al, 2 }
    $88/$E0/                          { mov al, ah }
    $75/$F6/                          { jnz %%w }
    $E6/$A0/                          { out 0A0h, al }
    $A1/>hwXn/                        { mov     ax, [hwXn] }
    $A3/>hwXz/                        { mov     [hwXz], ax }
    $BB/>hwXu/                        { zeile:  mov     bx, hwXu }
    $A1/>hwXt/                        { mov     ax, [hwXt] }
    $A3/>hwXk/                        { mov     [hwXk], ax }
    $B0/$49/                          { tupel:  mov     al, 49h }
    $88/$C4/                          { %%w: mov ah, al }
    $E4/$A0/                          { in al, 0A0h }
    $A8/$02/                          { test al, 2 }
    $88/$E0/                          { mov al, ah }
    $75/$F6/                          { jnz %%w }
    $E6/$A1/                          { out 0A1h, al }
    $8B/$16/>hwXa/                    { mov     dx, [hwXa] }
    $03/$17/                          { add     dx, [bx] }
    $88/$D0/                          { mov     al, dl }
    $88/$C4/                          { %%w: mov ah, al }
    $E4/$A0/                          { in al, 0A0h }
    $A8/$02/                          { test al, 2 }
    $88/$E0/                          { mov al, ah }
    $75/$F6/                          { jnz %%w }
    $E6/$A0/                          { out 0A0h, al }
    $88/$F0/                          { mov     al, dh }
    $88/$C4/                          { %%w: mov ah, al }
    $E4/$A0/                          { in al, 0A0h }
    $A8/$02/                          { test al, 2 }
    $88/$E0/                          { mov al, ah }
    $75/$F6/                          { jnz %%w }
    $E6/$A0/                          { out 0A0h, al }
    $8A/$47/$02/                      { mov     al, [bx+2] }
    $88/$C4/                          { %%w: mov ah, al }
    $E4/$A0/                          { in al, 0A0h }
    $A8/$02/                          { test al, 2 }
    $88/$E0/                          { mov al, ah }
    $75/$F6/                          { jnz %%w }
    $E6/$A1/                          { out 0A1h, al }
    $8B/$0E/>hwXw/                    { mov     cx, [hwXw] }
    $49/                              { dec     cx }
    $53/                              { push    bx }
    $56/                              { push    si }
    $83/$7F/$0A/$00/                  { cmp     word [bx+10], 0 }
    $74/$03/$E9/$91/$00/              { jne     null }
    $8B/$7F/$04/                      { mov     di, [bx+4] }
    $8B/$6F/$06/                      { mov     bp, [bx+6] }
    $8B/$5F/$08/                      { mov     bx, [bx+8] }
    $09/$FF/                          { or      di, di }
    $75/$45/                          { jnz     nurp }
    $E3/$21/                          { jcxz    lz }
    $26/$8B/$10/                      { wortz:  mov     dx, [es:si+bx] }
    $31/$EA/                          { xor     dx, bp }
    $26/$23/$14/                      { and     dx, [es:si] }
    $E4/$A0/                          { %%w: in al, 0A0h }
    $A8/$02/                          { test al, 2 }
    $75/$FA/                          { jnz %%w }
    $88/$D0/                          { mov al, dl }
    $E6/$A0/                          { out 0A0h, al }
    $E4/$A0/                          { %%w: in al, 0A0h }
    $A8/$02/                          { test al, 2 }
    $75/$FA/                          { jnz %%w }
    $88/$F0/                          { mov al, dh }
    $E6/$A0/                          { out 0A0h, al }
    $83/$C6/$02/                      { add     si, 2 }
    $E2/$DF/                          { loop    wortz }
    $26/$8B/$10/                      { lz:     mov     dx, [es:si+bx] }
    $31/$EA/                          { xor     dx, bp }
    $26/$23/$14/                      { and     dx, [es:si] }
    $23/$16/>hwXm/                    { and     dx, [hwXm] }
    $E4/$A0/                          { %%w: in al, 0A0h }
    $A8/$02/                          { test al, 2 }
    $75/$FA/                          { jnz %%w }
    $88/$D0/                          { mov al, dl }
    $E6/$A0/                          { out 0A0h, al }
    $E4/$A0/                          { %%w: in al, 0A0h }
    $A8/$02/                          { test al, 2 }
    $75/$FA/                          { jnz %%w }
    $88/$F0/                          { mov al, dh }
    $E6/$A0/                          { out 0A0h, al }
    $EB/$58/                          { jmp     short fertig }
    $E3/$1E/                          { nurp:   jcxz    lp }
    $26/$8B/$10/                      { wortp:  mov     dx, [es:si+bx] }
    $31/$EA/                          { xor     dx, bp }
    $E4/$A0/                          { %%w: in al, 0A0h }
    $A8/$02/                          { test al, 2 }
    $75/$FA/                          { jnz %%w }
    $88/$D0/                          { mov al, dl }
    $E6/$A0/                          { out 0A0h, al }
    $E4/$A0/                          { %%w: in al, 0A0h }
    $A8/$02/                          { test al, 2 }
    $75/$FA/                          { jnz %%w }
    $88/$F0/                          { mov al, dh }
    $E6/$A0/                          { out 0A0h, al }
    $83/$C6/$02/                      { add     si, 2 }
    $E2/$E2/                          { loop    wortp }
    $26/$8B/$10/                      { lp:     mov     dx, [es:si+bx] }
    $31/$EA/                          { xor     dx, bp }
    $23/$16/>hwXm/                    { and     dx, [hwXm] }
    $E4/$A0/                          { %%w: in al, 0A0h }
    $A8/$02/                          { test al, 2 }
    $75/$FA/                          { jnz %%w }
    $88/$D0/                          { mov al, dl }
    $E6/$A0/                          { out 0A0h, al }
    $E4/$A0/                          { %%w: in al, 0A0h }
    $A8/$02/                          { test al, 2 }
    $75/$FA/                          { jnz %%w }
    $88/$F0/                          { mov al, dh }
    $E6/$A0/                          { out 0A0h, al }
    $EB/$19/                          { jmp     short fertig }
    $41/                              { null:   inc     cx }
    $31/$D2/                          { xor     dx, dx }
    $E4/$A0/                          { %%w: in al, 0A0h }
    $A8/$02/                          { test al, 2 }
    $75/$FA/                          { jnz %%w }
    $88/$D0/                          { mov al, dl }
    $E6/$A0/                          { out 0A0h, al }
    $E4/$A0/                          { %%w: in al, 0A0h }
    $A8/$02/                          { test al, 2 }
    $75/$FA/                          { jnz %%w }
    $88/$F0/                          { mov al, dh }
    $E6/$A0/                          { out 0A0h, al }
    $E2/$EA/                          { loop    n1 }
    $5E/                              { fertig: pop     si }
    $5B/                              { pop     bx }
    $83/$C3/$0C/                      { add     bx, 12 }
    $FF/$0E/>hwXk/                    { dec     word [hwXk] }
    $74/$03/$E9/$F9/$FE/              { jnz     tupel }
    $03/$36/>hwXr/                    { add     si, [hwXr] }
    $83/$06/>hwXa/$28/                { add     word [hwXa], 40 }
    $FF/$0E/>hwXz/                    { dec     word [hwXz] }
    $74/$03/$E9/$DE/$FE/              { jnz     zeile }
    $07/                              { pop     es }
    $5D                               { pop     bp }
  );
  hwWst := false;
end;

{ n Bits ab Bit off von ss nach dd, jedes z1-mal (hoechstens 640 Bit).
  Maschinensprache. }
procedure ghzbits(var dd; var ss; off, n, z1: integer);
var m: integer;
begin
  if (n <= 0) or (z1 <= 0) then exit;
  m := n * z1;
  if m > 640 then m := 640;
  hwDst[0] := Ofs(dd); hwDst[1] := Seg(dd);
  hwSrc[0] := Ofs(ss); hwSrc[1] := Seg(ss);
  hwSx := off; hwNb := n; hwAa := z1; hwW := m;
  inline(
    $FC/                              { cld }
    $55/                              { push bp }
    $1E/                              { push ds }
    $8B/$1E/>hwSx/                    { mov bx,[hwSx] }
    $89/$D9/                          { mov cx,bx }
    $80/$E1/$07/                      { and cl,7 }
    $B2/$01/                          { mov dl,1 }
    $D2/$E2/                          { shl dl,cl }
    $B1/$03/                          { mov cl,3 }
    $D3/$EB/                          { shr bx,cl }
    $8B/$2E/>hwNb/                    { mov bp,[hwNb] }
    $8B/$0E/>hwW/                     { mov cx,[hwW] }
    $8A/$36/>hwAa/                    { mov dh,[hwAa] }
    $C4/$3E/>hwDst/                   { les di,[hwDst] }
    $C5/$36/>hwSrc/                   { lds si,[hwSrc] }
    $01/$DE/                          { add si,bx }
    $AC/                              { lodsb }
    $88/$C7/                          { mov bh,al }
    $30/$DB/                          { xor bl,bl }
    $B4/$01/                          { mov ah,1 }
    $88/$F8/                          { bit:    mov al,bh }
    $20/$D0/                          { and al,dl }
    $74/$02/                          { jz b0 }
    $B0/$FF/                          { mov al,0FFh }
    $52/                              { b0:     push dx }
    $88/$C2/                          { zl:     mov dl,al }
    $20/$E2/                          { and dl,ah }
    $08/$D3/                          { or bl,dl }
    $D0/$C4/                          { rol ah,1 }
    $73/$07/                          { jnc nw }
    $86/$C3/                          { xchg al,bl }
    $AA/                              { stosb }
    $86/$C3/                          { xchg al,bl }
    $30/$DB/                          { xor bl,bl }
    $49/                              { nw:     dec cx }
    $74/$13/                          { jz fertig }
    $FE/$CE/                          { dec dh }
    $75/$E8/                          { jnz zl }
    $5A/                              { pop dx }
    $D0/$C2/                          { rol dl,1 }
    $73/$05/                          { jnc ns }
    $8A/$04/                          { mov al,[si] }
    $46/                              { inc si }
    $88/$C7/                          { mov bh,al }
    $4D/                              { ns:     dec bp }
    $75/$D2/                          { jnz bit }
    $EB/$01/                          { jmp short raus }
    $5A/                              { fertig: pop dx }
    $80/$FC/$01/                      { raus:   cmp ah,1 }
    $74/$03/                          { jz ende }
    $88/$D8/                          { mov al,bl }
    $AA/                              { stosb }
    $1F/                              { ende:   pop ds }
    $5D                               { pop bp }
  );
end;

{ Block wie BIOS 67/2: Wortzahl (hoch, niedrig), Liste, dahinter Platz }
function ghrdat(var bl): integer;
var b: GBytes absolute bl;
    m, p: integer;
begin
  hwWst := false;
  m := b[0] * 256 + b[1];
  p := 2;
  while b[p] <> 0 do p := p + b[p] + 1;
  ghlist(b[2]);
  ghrdat := hwLies(m, b[p + 1]);
end;

function ghstat: integer;
begin
  ghstat := Port[$A0];
end;

procedure ghvsyn;                   { Status Bit 5 von 0 auf 1 }
var n: integer;
begin
  n := 0;
  while ((Port[$A0] and $20) <> 0) and (n < hwZEIT) do n := n + 1;
  n := 0;
  while ((Port[$A0] and $20) = 0) and (n < hwZEIT) do n := n + 1;
end;

function ghvzahl: integer;          { Statusabfragen bis zum Bildwechsel }
var n: integer;
begin
  n := 0;
  while ((Port[$A0] and $20) <> 0) and (n < 20000) do n := n + 1;
  while ((Port[$A0] and $20) = 0) and (n < 30000) do n := n + 1;
  ghvzahl := n;
end;

function hwWLies(a: integer): integer;  { ein Wort aus dem Bildspeicher }
begin
  hwCmd($49); hwPar(Lo(a)); hwPar(Hi(a)); hwPar(0);
  hwCmd($4A); hwPar($FF); hwPar($FF);
  hwCmd($4C); hwPar($02); hwPar(1); hwPar(0);
  hwCmd($A0);
  hwWb[0] := 0; hwWb[1] := 0;
  if hwLies(2, hwWb) = 0 then ;
  hwWLies := hwWb[0] + 256 * hwWb[1];
end;

procedure hwWSchr(a, w: integer);   { ein Wort schreiben }
begin
  hwCmd($49); hwPar(Lo(a)); hwPar(Hi(a)); hwPar(0);
  hwCmd($4A); hwPar($FF); hwPar($FF);
  hwCmd($4C); hwPar($02); hwPar(0); hwPar(0);
  hwCmd($20); hwPar(Lo(w)); hwPar(Hi(w));
end;

{ Kennbuchstabe der Firmware (0FF9h, 'C' Farbe, 'M' Mono): OUT 11h legt
  das ROM ueber 0-1FFFh, OUT 10h zurueck, dazwischen keine Unterbrechung.
  Der Code muss ueber 2000h liegen. }
procedure hwRom;
begin
  hwRomz := 0;
  inline(
    $9C/                              { pushf }
    $FA/                              { cli }
    $1E/                              { push ds }
    $31/$C0/                          { xor ax,ax }
    $8E/$D8/                          { mov ds,ax }
    $E6/$11/                          { out 11h,al }
    $8A/$1E/$F9/$0F/                  { mov bl,[0FF9h] }
    $E6/$10/                          { out 10h,al }
    $1F/                              { pop ds }
    $9D/                              { popf }
    $88/$1E/>hwRomz                   { mov [hwRomz],bl }
  );
end;

{ Bildschirm nach HWA 3: die Farbkarte hat drei Ebenen zu 16 K Worten,
  die Monokarte eine (sie spiegelt 4000h auf 0000h). Alte Worte sichern,
  A5C3h nach 3FF0h und 3C5Ah nach 7FF0h, in der Reihenfolge 7FF0h,
  3FF0h zuruecklesen, alte Worte zurueck. Bleiben beide stehen: Farbe.
  Mit dem Kennbuchstaben verglichen; hwSchirm: 1 Mono, 2 Farbe, 3
  widerspruechlich. }
function hwFarbtest: integer;
var a3, a7, v3, v7, r: integer;
begin
  a3 := hwWLies($3FF0); a7 := hwWLies($7FF0);
  hwWSchr($3FF0, $A5C3); hwWSchr($7FF0, $3C5A);
  v7 := hwWLies($7FF0); v3 := hwWLies($3FF0);
  hwWSchr($7FF0, a7); hwWSchr($3FF0, a3);
  if (v3 = $A5C3) and (v7 = $3C5A) then r := 2
  else if (v3 = $3C5A) and (v7 = $3C5A) then r := 1
  else r := 0;
  hwRom;
  if r = 0 then begin               { Probe unklar: nach der Firmware }
    hwSchirm := 3;
    if hwRomz = Ord('C') then r := 2 else r := 1;
  end else if ((hwRomz = Ord('C')) and (r = 1)) or
              ((hwRomz = Ord('M')) and (r = 2)) then hwSchirm := 3
  else hwSchirm := r;
  if r = 2 then hwFarbtest := 1 else hwFarbtest := 0;
end;

{ Ist das eine DMV? Statusbits des 7220 an A0h: Bildwechsel (Bit 5) und
  Zeilenruecklauf (Bit 6) wechseln, FIFO leer (Bit 2) kommt vor, nie
  FFh. Liest nur. }
function hwDMV: boolean;
var i, s, m: integer;
begin
  hwDMV := false;
  m := 0; i := 0;
  while (i < 30000) and (m <> $1F) do begin
    s := Port[$A0];
    if s = $FF then exit;
    if (s and $20) <> 0 then m := m or 1 else m := m or 2;
    if (s and $40) <> 0 then m := m or 4 else m := m or 8;
    if (s and 4) <> 0 then m := m or $10;
    i := i + 1;
  end;
  hwDMV := m = $1F;
end;

{ K806 auf 2A (30h-33h) nach HWA 5. Erst nur lesen: ein leerer Platz
  liefert das untere Adressbyte zurueck (MAME: FFh). Dann Befehl 00h an
  31h: Statusbit 3 (F1) zeigt Adressbit 0 des letzten Schreibens, also
  1, und es kommen genau fuenf Datenbytes. Nichts sonst wird beschrieben. }
function hwMaustest: integer;
const MW = 20000;
var v, i, n, t: integer;
begin
  hwMaustest := 0;
  n := 0;
  for i := 0 to 3 do begin
    v := Port[$30 + i];
    if (v = $30 + i) or (v = $FF) then n := n + 1;
  end;
  if n = 4 then exit;
  i := 0;
  while (i < 64) and ((Port[$31] and 1) <> 0) do begin   { alte Daten }
    v := Port[$30]; i := i + 1;
  end;
  t := 0;
  while (t < MW) and ((Port[$31] and 2) <> 0) do t := t + 1;
  if t >= MW then exit;
  Port[$31] := 0;
  if (Port[$31] and 8) = 0 then exit;                   { F1 }
  n := 0;
  repeat
    t := 0;
    if n < 5 then i := MW else i := MW div 8;
    while (t < i) and ((Port[$31] and 1) = 0) do t := t + 1;
    if (Port[$31] and 1) <> 0 then begin v := Port[$30]; n := n + 1; end
    else t := -1;
  until (t < 0) or (n > 5);
  if n = 5 then hwMaustest := $30;
end;

function ghinit: integer;
var a, n: integer;
begin
  ghinit := -1;
  if not hwDMV then exit;             { keine DMV: nichts anfassen }
  hwFh := -1;
  FillChar(hwFF, 80, $FF);
  hwWst := false;
  hwBn := -1;
  { Die DOS-Konsole schreibt nach dem Farbtest an der alten Stelle
    weiter: Adresse (EAD) vorher mit CURD lesen, hinterher mit CURS
    zurueck, MASK FFFFh }
  n := 0;
  while (n < 16) and ((Port[$A0] and 1) <> 0) do begin
    a := Port[$A1]; n := n + 1;
  end;
  hwCmd($E0);
  hwEad[0] := 0; hwEad[1] := 0; hwEad[2] := 0;
  if hwLies(3, hwEad) = 0 then ;
  n := 0;
  while (n < 16) and ((Port[$A0] and 1) <> 0) do begin
    a := Port[$A1]; n := n + 1;            { der echte 7220 schickt noch MASK }
  end;
  hwFarbe := hwFarbtest;
  hwCmd($49); hwPar(hwEad[0]); hwPar(hwEad[1]); hwPar(hwEad[2] and 3);
  hwCmd($4A); hwPar($FF); hwPar($FF);
  hwK806 := hwMaustest;
  a := 0;
  if hwFarbe <> 0 then a := a or GH_FARBE;
  if hwK806 <> 0 then a := a or GH_MAUS;
  ghinit := a;
end;

{ Grafik ein und aus wie GINIT/GEXIT von NCRs GRAPH-Bibliothek unter DOS }
procedure ghgraf(an: boolean);
var i, e: integer;
begin
  if an then begin
    hwCmd($0C);                             { Anzeige aus }
    hwCmd($4B); hwPar(0);                   { CCHAR: kein Cursor }
    hwCmd($70);                             { PRAM: zwei Bildbereiche }
    for i := 1 to 2 do begin
      hwPar(0); hwPar(0); hwPar(0); hwPar($59);
    end;
    hwCmd($47); hwPar(80);                  { PITCH }
    hwCmd($49); hwPar(0); hwPar(0); hwPar(0);
    hwCmd($46); hwParZ(0);                  { ZOOM 0 }
    hwCmd($4A); hwPar($FF); hwPar($FF);     { MASK }
    if hwFarbe <> 0 then e := 3 else e := 1;
    for i := 1 to e do begin
      hwCmd($4C); hwPar($02); hwPar($FF); hwPar($7F);
      hwCmd($22); hwPar($FF); hwPar($FF);
    end;
    hwCmd($6B);                             { START }
    hwCmd($0D);                             { Anzeige an }
  end else begin
    hwCmd($0C);
    hwCmd($46); hwParZ(0);
    hwCmd($4B); hwPar($8F);                 { CCHAR wie NCRs GEXIT }
    hwCmd($70);
    hwPar(0); hwPar(0); hwPar(0); hwPar($19);
    hwPar(0); hwPar(0); hwPar(0); hwPar($19);
    for i := 1 to 8 do hwPar($FF);
    hwCmd($47); hwPar(80);
    hwCmd($49); hwPar(0); hwPar(0); hwPar(0);
    hwCmd($4A); hwPar($FF); hwPar($FF);
    hwCmd($6B);
    hwCmd($0D);
    hwR.AX := $0200; hwR.DX := $1A;         { die Konsole loescht selbst }
    MsDos(hwR);
  end;
end;

function ghdset(an: integer): integer;     { unter DOS nur der direkte Weg }
begin
  ghdset := -1;
end;

{ ---- Maus K806 (wie BIOS 68) ---- }

function hwMw(bit, soll: integer): integer;  { Status-Bit abwarten }
var n: integer;
begin
  hwMw := 0;
  for n := 1 to hwZEIT do
    if ((Port[hwK806 + 1] and bit) <> 0) = (soll <> 0) then exit;
  hwMw := 1;
end;

function ghmaus(c: integer; var bl): integer;
var b: GBytes absolute bl;
    i, p, a, k: integer;
begin
  ghmaus := 1;
  if hwK806 = 0 then exit;
  ghmaus := 2;
  i := 0;
  while (i < 64) and ((Port[hwK806 + 1] and 1) <> 0) do begin
    a := Port[hwK806]; i := i + 1;
  end;
  p := b[0];
  k := 1;
  if hwMw(2, 0) <> 0 then exit;
  Port[hwK806 + 1] := c;
  while p > 0 do begin
    if hwMw(2, 0) <> 0 then exit;
    Port[hwK806] := b[k];
    k := k + 1; p := p - 1;
  end;
  a := b[k];
  k := k + 1;
  while a > 0 do begin
    if hwMw(1, 1) <> 0 then exit;
    b[k] := Port[hwK806];
    k := k + 1; a := a - 1;
  end;
  ghmaus := 0;
end;

{ ---- Ton ueber den 8741 (wie BIOS 69, DMVTRAK) ---- }

procedure hwKw;
var n: integer;
begin
  n := 0;
  while ((Port[$41] and 2) <> 0) and (n < hwZEIT) do n := n + 1;
end;

{ Tastatur-8741 fuer die Tonwarteschlange: Eingangspuffer frei (IBF 0)?
  Und ein Byte an 41h (Befehl) oder 40h (Daten), ohne zu warten. }
function ghkfrei: boolean;
begin
  ghkfrei := (Port[$41] and 2) = 0;
end;

procedure ghkaus(befehl: boolean; v: integer);
begin
  if befehl then Port[$41] := v else Port[$40] := v;
end;

function ghton(h, l: integer): integer;
begin
  ghton := 1;
  if (Port[$41] and 2) <> 0 then exit;
  Port[$41] := $06;
  hwKw;
  if h > 42 then h := 42;
  Port[$40] := h + $20;
  hwKw;
  if l < 1 then l := 1;
  if l > 224 then l := 224;
  Port[$40] := l + $1F;
  ghton := 0;
end;

{ ---- Tastatur, Text, Dateien ueber DOS ---- }

function ghtast: integer;           { 06h mit DL = FFh: holen ohne Warten }
begin
  hwR.AX := $0600; hwR.DX := $FF;
  MsDos(hwR);
  if (hwR.Flags and $40) <> 0 then ghtast := 0
  else ghtast := Lo(hwR.AX);
end;

procedure ghtext(s: GStr);
var i: integer;
begin
  for i := 1 to Length(s) do begin
    if s[i] = #10 then begin
      hwR.AX := $0200; hwR.DX := 13; MsDos(hwR);
    end;
    hwR.AX := $0200; hwR.DX := Ord(s[i]); MsDos(hwR);
  end;
end;

function ghopen(name: GStr): integer;
var z: array[0..80] of byte;
    i: integer;
begin
  for i := 1 to Length(name) do z[i - 1] := Ord(name[i]);
  z[Length(name)] := 0;
  hwR.AX := $3D00;                  { nur lesen }
  hwR.DS := Seg(z); hwR.DX := Ofs(z);
  MsDos(hwR);
  if (hwR.Flags and 1) <> 0 then ghopen := 1
  else begin hwFh := hwR.AX; ghopen := 0; end;
end;

function ghread(var bl): integer;   { 128 Byte; ein kurzer Rest mit ^Z }
var b: GBytes absolute bl;
    i: integer;
begin
  hwR.AX := $3F00; hwR.BX := hwFh; hwR.CX := 128;
  hwR.DS := Seg(b); hwR.DX := Ofs(b);
  MsDos(hwR);
  if ((hwR.Flags and 1) <> 0) or (hwR.AX = 0) then ghread := 0
  else begin
    for i := hwR.AX to 127 do b[i] := $1A;
    ghread := 1;
  end;
end;

procedure ghclos;
begin
  if hwFh >= 0 then begin
    hwR.AX := $3E00; hwR.BX := hwFh; MsDos(hwR);
    hwFh := -1;
  end;
end;

{ ---- Bildspeicher: Bloecke zu 64 KB von DOS ---- }

function ghsnum: integer;
begin
  if hwBn < 0 then begin
    hwBn := 0;
    while hwBn < hwGBBLK do begin
      hwR.AX := $4800; hwR.BX := $1000;     { 4096 Absaetze = 64 KB }
      MsDos(hwR);
      if (hwR.Flags and 1) <> 0 then begin
        ghsnum := hwBn; exit;
      end;
      hwBseg[hwBn] := hwR.AX;
      hwBn := hwBn + 1;
    end;
  end;
  ghsnum := hwBn;
end;

function ghsgro(b: integer): real;
begin
  ghsgro := 65536.0;
end;

procedure ghsput(b, o: integer; var q; n: integer);
begin
  Move(q, Mem[hwBseg[b]:o], n);
end;

procedure ghsget(var z; b, o, n: integer);
begin
  Move(Mem[hwBseg[b]:o], z, n);
end;

procedure ghsmov(b, z, q, n: integer);     { z < q, vorwaerts }
begin
  Move(Mem[hwBseg[b]:q], Mem[hwBseg[b]:z], n);
end;

function hwAdrI(var x): integer;   { Adresse im Datensegment als Zahl }
begin
  hwAdrI := Ofs(x);
end;

function hwAdr(var x): GBZeiger;   { Zeiger auf eine Variable }
begin
  hwAdr := Ptr(Seg(x), Ofs(x));
end;
