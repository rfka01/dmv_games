{ DMVGFX.INC - Grafik, Text und Ton fuer die NCR Decision Mate V
  (Farbgrafik mit uPD7220, 640x400, Ebenen G 0000h, R 4000h, B 8000h)
  Turbo Pascal 3.01A. Braucht FONT.INC (vorher einbinden).
  Code: Claude (Anthropic), Prompts: rfka01 - 2026. Lizenz: MIT

  Grafikchip:  Cmd, Par, Mode, SetPRAM, Solid, Cursor, GChr, ChrZoom
  Zeichnen:    Box, DBox (gedithert), Sprite16, BlitAsm, Txt, TxtC, TxtB
  Takt:        WaitFrame (Strahlruecklauf), Wait
  Ton:         Tone, SndClear, SndIdle (8741-Befehl 06h, Warteschlange)
  Sonstiges:   GfxInit, GfxExit, IsDMV, Num, NumR, Max, Min }

type
  Spr16 = array[0..2, 0..1, 0..15] of integer;  { Ebene, SET/RESET, Zeile }
  Pat8  = array[0..7] of byte;
  Str80 = string[80];

const
  { Farben wie TurboGraf: 1 blau, 2 gruen, 4 rot }
  cBlack = 0; cBlue = 1; cGreen = 2; cCyan = 3; cRed = 4;
  cMagenta = 5; cYellow = 6; cWhite = 7;

  { Muster fuer Dither, 4 Zeilen, Bit 0 = links }
  patCheck = 0; patWave = 1; pat25 = 2; pat12 = 3;
  PatRow: array[0..3, 0..3] of byte = (($55, $AA, $55, $AA),
                                       ($FF, $EE, $55, $BB),
                                       ($11, $00, $44, $00),
                                       ($01, $00, $10, $00));

  PB: array[0..2] of integer = ($0000, $4000, $8000);  { gruen rot blau }
  PBit: array[0..2] of integer = (2, 4, 1);

var
  CurP     : Pat8;                 { zuletzt geladenes GCHRD-Muster }
  CurOK    : boolean;
  Frames   : integer;              { Bildwechsel seit Start }
  { Tonwarteschlange fuer den 8741 (Befehl 06h) }
  SqT, SqL : array[0..63] of byte;
  SqHead, SqTail, SqPh : integer;
  SoundOn  : boolean;

{ ---------------- GDC-Grundroutinen ---------------- }

procedure Cmd(B: byte);
begin
  repeat until (Port[$A0] and 2) = 0;
  Port[$A1] := B;
end;

procedure Par(B: byte);
begin
  repeat until (Port[$A0] and 2) = 0;
  Port[$A0] := B;
end;

{ ---------------- Ton ueber den Tastatur-Controller 8741 ----------------
  Befehl 06h an Port 41h, dann Ton+20h und Laenge+1Fh an Port 40h.
  Ton n = 110 * 2^((n-1)/12) Hz (1..42), 0 = Pause; Laenge in 20 ms.
  Der 8741 spielt den Ton selbst; der Befehl fuer den naechsten Ton
  wartet in seinem Eingangspuffer, bis der laufende Ton fertig ist.
  SndPump wird bei jedem Bildwechsel aufgerufen und wartet nie lange. }

procedure SndPump;
var I: integer;
  function Free: boolean;      { Eingangspuffer leer, kurz warten }
  var K: integer;
  begin
    K := 0;
    while ((Port[$41] and 2) <> 0) and (K < 200) do K := K + 1;
    Free := (Port[$41] and 2) = 0;
  end;
begin
  for I := 1 to 4 do
  begin
    if SqPh = 0 then
    begin
      if SqHead = SqTail then exit;
      if (Port[$41] and 2) <> 0 then exit;
      Port[$41] := $06;
      SqPh := 1;
    end;
    if SqPh = 1 then
    begin
      if (Port[$41] and 2) <> 0 then exit;   { voriger Ton laeuft noch }
      Port[$40] := SqT[SqHead] + $20;
      SqPh := 2;
    end;
    if SqPh = 2 then
    begin
      if not Free then exit;
      Port[$40] := SqL[SqHead] + $1F;
      SqHead := (SqHead + 1) and 63;
      SqPh := 0;
    end;
  end;
end;

procedure Tone(N, L: integer);         { in die Warteschlange }
begin
  if not SoundOn then exit;
  if ((SqTail + 1) and 63) = SqHead then exit;
  if L < 1 then L := 1;
  SqT[SqTail] := N; SqL[SqTail] := L;
  SqTail := (SqTail + 1) and 63;
end;

procedure SndClear;                    { Wartendes verwerfen }
begin
  SqTail := SqHead;
  if SqPh = 0 then exit;
  SqTail := (SqHead + 1) and 63;       { angefangenen Ton zu Ende senden }
end;

procedure WaitFrame;
begin
  repeat SndPump until (Port[$A0] and $20) = 0;   { Toene auch beim Warten }
  repeat SndPump until (Port[$A0] and $20) <> 0;
  Frames := Frames + 1;
end;

procedure SndIdle;                     { warten, bis alles gesendet ist }
var K: integer;
begin
  K := 0;
  while ((SqHead <> SqTail) or (SqPh <> 0)) and (K < 300) do
  begin
    WaitFrame;
    K := K + 1;
  end;
end;

procedure Wait(N: integer);
var I: integer;
begin
  for I := 1 to N do WaitFrame;
end;

procedure Mode(M: integer);      { 0 ersetzen 1 xor 2 loeschen 3 setzen }
begin
  Cmd($20 + M);
end;

procedure SetPRAM(var B: Pat8);  { GCHRD-Muster, nur wenn geaendert }
var I: integer;
    Same: boolean;
begin
  Same := CurOK;
  if Same then
    for I := 0 to 7 do if B[I] <> CurP[I] then Same := false;
  if not Same then
  begin
    Cmd($78);
    for I := 0 to 7 do Par(B[I]);
    CurP := B;
    CurOK := true;
  end;
end;

procedure Solid;
var B: Pat8;
begin
  FillChar(B, 8, $FF);
  SetPRAM(B);
end;

procedure Cursor(X, Y, P: integer);   { Y von oben, P Ebene }
var A: integer;
begin
  A := Y * 40 + (X shr 4) + PB[P];
  Cmd($49);
  Par(Lo(A));
  Par(Hi(A));
  Par((X and 15) shl 4);
end;

procedure GChr(W, H: integer);   { Flaeche W x H nach rechts und oben }
begin
  Cmd($4C);
  Par($12);
  Par(Lo(H - 1));
  Par(((Hi(H - 1)) and $3F) or $40);
  Par(Lo(W));
  Par(Hi(W) and $3F);
  Cmd($68);
end;

procedure ChrZoom(Z: integer);
begin
  Cmd($46);
  Par(Z and 15);
end;

{ farbechte Box, X/Y = linke obere Ecke }
procedure Box(X, Y, W, H, Col: integer);
var P: integer;
begin
  if (W <= 0) or (H <= 0) then exit;
  Solid;
  for P := 0 to 2 do
  begin
    if (Col and PBit[P]) <> 0 then Mode(3) else Mode(2);
    Cursor(X, Y + H - 1, P);
    GChr(W, H);
  end;
end;

{ geditherte Box: Farbe CA auf den Musterbits, CB sonst.
  Muster am Bildschirm ausgerichtet; W sollte durch 8 teilbar sein }
procedure DBox(X, Y, W, H, CA, CB, Pat: integer);
var P, B, R, S, YB: integer;
    PA, PN: Pat8;
    A1, B1: boolean;
begin
  if (W <= 0) or (H <= 0) then exit;
  YB := Y + H - 1;
  for B := 8 to 15 do
  begin
    R := PatRow[Pat, (YB - 15 + B + 16) and 3];
    if (B and 1) = 0 then S := (X + W) and 7 else S := X and 7;
    R := ((R shr S) or (R shl (8 - S))) and $FF;
    PA[B - 8] := R;
    PN[B - 8] := (not R) and $FF;
  end;
  for P := 0 to 2 do
  begin
    A1 := (CA and PBit[P]) <> 0;
    B1 := (CB and PBit[P]) <> 0;
    if A1 and B1 then begin Solid; Mode(3); end
    else if not (A1 or B1) then begin Solid; Mode(2); end
    else if A1 then begin SetPRAM(PA); Mode(0); end
    else begin SetPRAM(PN); Mode(0); end;
    Cursor(X, YB, P);
    GChr(W, H);
  end;
end;

{ ---- Sprites: je Ebene SET- und RESET-Maske, 8x8-Bloecke per GCHRD ---- }

procedure Block8(X, Y, P: integer; var R; Ofs, Shift, Z: integer);
type IA = array[0..15] of integer;
var I, Any: integer;
    A: IA absolute R;
    B: Pat8;
begin
  Any := 0;
  for I := 0 to 7 do
  begin
    B[I] := Lo(A[Ofs + I] shr Shift);
    Any := Any or B[I];
  end;
  if Any = 0 then exit;
  SetPRAM(B);
  Cursor(X, Y + 8 * (Z + 1) - 1, P);
  GChr(8, 8);
end;

procedure Sprite16(X, Y: integer; var S: Spr16; Z: integer);
{ Z = 0: 16x16, Z = 1: 32x32 (GDC-Zoom) }
var P, O, K: integer;
begin
  K := 8 * (Z + 1);
  if Z <> 0 then ChrZoom(Z);
  for P := 0 to 2 do
    for O := 0 to 1 do
    begin
      if O = 0 then Mode(3) else Mode(2);
      Block8(X,     Y,     P, S[P, O], 0, 0, Z);
      Block8(X + K, Y,     P, S[P, O], 0, 8, Z);
      Block8(X,     Y + K, P, S[P, O], 8, 0, Z);
      Block8(X + K, Y + K, P, S[P, O], 8, 8, Z);
    end;
  if Z <> 0 then ChrZoom(0);
end;

{ ---- Text: eigene Schrift aus FONT.INC (8x16), durchsichtig ----
  Zeichen 32..126 direkt, Umlaute ueber FontUml (Codepage 437). }

function FontIdx(C: char): integer;
var J, K: integer;
begin
  K := -1;
  if (C >= ' ') and (C <= '~') then K := Ord(C) - 32
  else
    for J := 0 to 6 do if FontUml[J] = Ord(C) then K := 95 + J;
  FontIdx := K;
end;

procedure Txt(X, Y: integer; S: Str80; Col, Z: integer);
var I, H, J, P, A, K, Any: integer;
    B: Pat8;
begin
  K := 8 * (Z + 1);
  if Z <> 0 then ChrZoom(Z);
  for I := 1 to Length(S) do
  begin
    A := FontIdx(S[I]);
    if A >= 0 then
      for H := 0 to 1 do        { 0 = obere, 1 = untere Haelfte }
      begin
        Any := 0;
        for J := 0 to 7 do
        begin
          B[J] := FontG[A, H * 8 + J];
          Any := Any or B[J];
        end;
        if Any <> 0 then
        begin
          SetPRAM(B);
          for P := 0 to 2 do
          begin
            if (Col and PBit[P]) <> 0 then Mode(3) else Mode(2);
            Cursor(X, Y + H * K + K - 1, P);
            GChr(8, 8);
          end;
        end;
      end;
    X := X + K;
  end;
  if Z <> 0 then ChrZoom(0);
end;

procedure TxtC(Y: integer; S: Str80; Col, Z: integer);   { zentriert }
begin
  Txt(320 - Length(S) * 4 * (Z + 1), Y, S, Col, Z);
end;

function Num(N, W: integer): Str80;   { mit fuehrenden Nullen }
var S: Str80;
begin
  Str(N, S);
  while Length(S) < W do S := '0' + S;
  Num := S;
end;

function Max(A, B: integer): integer;
begin
  if A > B then Max := A else Max := B;
end;

function Min(A, B: integer): integer;
begin
  if A < B then Min := A else Min := B;
end;


procedure TxtB(X, Y: integer; S: Str80; Col: integer);   { fett }
begin
  Txt(X, Y, S, Col, 0);
  Txt(X + 1, Y, S, Col, 0);
end;

{ Laeuft das Programm auf einer DMV? An A0h muss ein uPD7220 antworten:
  VSYNC (Bit 5) und HBLANK (Bit 6) wechseln, FIFO leer (Bit 2) kommt vor,
  FFh nie. Auf einem AT saesse dort der zweite Interrupt-Controller. }
function IsDMV: boolean;
var I, S: integer;
    V0, V1, H0, H1, Emp, Bad: boolean;
begin
  V0 := false; V1 := false; H0 := false; H1 := false;
  Emp := false; Bad := false;
  for I := 1 to 30000 do
  begin
    S := Port[$A0];
    if S = $FF then Bad := true;
    if (S and $20) <> 0 then V1 := true else V0 := true;
    if (S and $40) <> 0 then H1 := true else H0 := true;
    if (S and $04) <> 0 then Emp := true;
  end;
  IsDMV := V0 and V1 and H0 and H1 and Emp and not Bad;
end;

function NumR(X: real; W: integer): Str80;   { Punkte, fuehrende Nullen }
var S: Str80;
    I: integer;
begin
  Str(X:W:0, S);
  for I := 1 to Length(S) do if S[I] = ' ' then S[I] := '0';
  NumR := S;
end;

{ Schickt NRows Zeilen zu je NWords Worten als WDAT-Strom an den GDC;
  Md = WDAT-Befehl (22h loeschen, 23h setzen).
  Src = erstes Wort, Ead = Wortadresse der ersten Zeile, nach jeder Zeile
  werden Skip Bytes in der Quelle uebersprungen und Ead um 40 erhoeht. }
procedure BlitAsm(var Src; Ead, NRows, NWords, Skip, Md: integer);
begin
  Inline(
    $1E/                    {       push ds              }
    $C5/$76/<Src/           {       lds  si,[bp+Src]     }
    $8B/$56/<Ead/           {       mov  dx,[bp+Ead]     }
    $8B/$5E/<NRows/         {       mov  bx,[bp+NRows]   }
    $FC/                    {       cld                  }
    $E4/$A0/$A8/$02/$75/$FA/{ ROW:  warten FIFO          }
    $B0/$49/$E6/$A1/        {       CURS                 }
    $E4/$A0/$A8/$02/$75/$FA/
    $88/$D0/$E6/$A0/        {       EAD low              }
    $E4/$A0/$A8/$02/$75/$FA/
    $88/$F0/$E6/$A0/        {       EAD high             }
    $E4/$A0/$A8/$02/$75/$FA/
    $8A/$46/<Md/$E6/$A1/    {       WDAT (Modus Md)      }
    $8B/$4E/<NWords/        {       mov  cx,[bp+NWords]  }
    $AD/                    { WRD:  lodsw                }
    $89/$C7/                {       mov  di,ax           }
    $E4/$A0/$A8/$02/$75/$FA/
    $89/$F8/$E6/$A0/        {       low byte             }
    $E4/$A0/$A8/$02/$75/$FA/
    $89/$F8/$88/$E0/$E6/$A0/{       high byte            }
    $E2/$E5/                {       loop WRD             }
    $03/$76/<Skip/          {       add  si,[bp+Skip]    }
    $83/$C2/$28/            {       add  dx,40           }
    $4B/                    {       dec  bx              }
    $75/$B0/                {       jnz  ROW             }
    $1F);                   {       pop  ds              }
end;


{ ---------------- Grafik ein/aus (ersetzt GINIT/GEXIT von TurboGraf) ---------------- }

procedure GfxPRAM(L: byte);           { zwei Bildbereiche ab 0, L = Laenge/Modus }
var Z: integer;
begin
  Cmd($70);
  for Z := 1 to 2 do begin Par(0); Par(0); Par(0); Par(L); end;
end;

procedure GfxInit;                    { Grafikmodus, alle Ebenen loeschen }
var N: integer;
begin
  Cmd($0C);                           { BCTRL: Anzeige aus }
  GfxPRAM($59);                       { Bildmodus (Bit 6), Laenge 19h }
  Cmd($49); Par(0); Par(0); Par(0);   { CURS 0 }
  ChrZoom(0);
  Cmd($4A); Par($FF); Par($FF);       { MASK: alle Punkte }
  for N := 1 to 3 do                  { 3 x 32K Worte loeschen }
  begin
    Cmd($4C); Par(2); Par($FF); Par($7F);
    Cmd($22); Par($FF); Par($FF);
  end;
  CurOK := false;
  Cmd($0D);                           { Anzeige an }
end;

procedure GfxExit;                    { zurueck in den Textmodus }
var Z: integer;
begin
  SndIdle;
  Cmd($0C);
  ChrZoom(0);
  GfxPRAM($19);                       { Zeichenmodus }
  Cmd($78);                           { PRAM 8..15: volle Zeichenmuster }
  for Z := 1 to 8 do Par($FF);
  ClrScr;
  Cmd($0D);
end;

