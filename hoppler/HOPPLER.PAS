program Hoppler;

{ HOPPLER - frei nach Perestroika/Toppler fuer die NCR Decision Mate V
  Turbo Pascal 3.01A + TurboGraf 3.2 (CGRAF.LIB), Farbgrafik 640x400
  Code: Claude (Anthropic), Prompts: rfka01 - 2026
  Lizenz: MIT

  Laufzeit: CHARGEN0.OVR (TurboGraf-Zeichensatz) im aktuellen Verzeichnis.
  Beim ersten Start werden die Seerosenbilder berechnet und in HOPPLER.DAT
  gespeichert; danach nur noch geladen.
  Die Quelle ist in Codepage 437 geschrieben (Umlaute in den Texten).

  Haeufigkeiten und Punkte der Tiere folgen dem Original (PERESTR.EXE,
  Locis 1990), aus dem Maschinencode abgelesen. }

{$C-}
{$I CGRAF.LIB}

type
  Spr16 = array[0..2, 0..1, 0..15] of integer;  { Ebene, SET/RESET, Zeile }
  Pat8  = array[0..7] of byte;
  ImgA  = array[0..4095] of integer;  { (Ebene*Zeilen+Zeile)*Worte+Wort }
  ImgP  = ^ImgA;
  Str80 = string[80];

{$I SPRITES.INC}

const
  MaxC    = 14;          { groesstes Raster 14 x 8 }
  MaxR    = 8;
  PondTop = 40;          { Statuszeile 0..31, Glitzerstreifen 32..39 }
  MaxS    = 21;          { groesste Seerose (Halbachse senkrecht) }
  RingBase = 21;         { Bild 21+r: Ringe mit Halbachse r }
  NImg    = 42;
  LastLevel = 25;
  Version = 'V0.16';
  MaxBirds  = 2;

  { Raster je Level 1..8 (ab 8 gleich), wie im Original }
  GridC: array[1..8] of integer = (7, 8, 9, 10, 11, 12, 13, 14);
  GridR: array[1..8] of integer = (4, 4, 5, 5, 6, 7, 7, 8);

  { Farben wie TurboGraf: 1 blau, 2 gruen, 4 rot }
  cBlack = 0; cBlue = 1; cGreen = 2; cCyan = 3; cRed = 4;
  cMagenta = 5; cYellow = 6; cWhite = 7;

  { Muster fuer Dither, 4 Zeilen, Bit 0 = links }
  patCheck = 0; patWave = 1; pat25 = 2; pat12 = 3;
  PatRow: array[0..3, 0..3] of byte = (($55, $AA, $55, $AA),
                                       ($FF, $EE, $55, $BB),
                                       ($11, $00, $44, $00),
                                       ($01, $00, $10, $00));
  TitleVar = 2;   { Hintergrund des Titelbilds: dunkelblau }

  { Zustaende einer Zelle }
  stEmpty = 0; stLive = 1; stRipple = 2; stFixed = 3; stShrink = 4;

  { Tiere auf den Seerosen }
  iNone = 0; iKaefer = 1; iBiene = 2; iKroete = 3; iBluete = 4;

  PB: array[0..2] of integer = ($0000, $4000, $8000);  { gruen rot blau }
  PBit: array[0..2] of integer = (2, 4, 1);
  UpTo: array[0..15] of integer = ($0001, $0003, $0007, $000F, $001F, $003F,
    $007F, $00FF, $01FF, $03FF, $07FF, $0FFF, $1FFF, $3FFF, $7FFF, $FFFF);

type
  DosRegs = record AX, BX, CX, DX, BP, SI, DI, DS, ES, Flags: integer; end;
  HiRec = record
            Name  : string[12];
            Score : real;
            Lvl   : integer;
          end;
  CellRec = record
              Size  : integer;    { Halbachse der Seerose, 0 = keine }
              State : integer;
              Timer : integer;
              Item  : integer;    { Tier; bleibt an der Zelle wie im Original }
              RF    : integer;    { Ringbild }
            end;

var
  Cell     : array[0..13, 0..7] of CellRec;
  Img      : array[0..NImg, 0..1] of ImgP;
  ImW      : array[0..NImg, 0..1] of integer;   { Worte je Zeile }
  ImA      : array[0..NImg, 0..1] of integer;   { Worte links der Mitte }
  ImRec    : array[0..NImg, 0..1] of integer;   { 128-Byte-Saetze }
  CurP     : Pat8;
  CurOK    : boolean;
  Cols, Rows, PitchX, PitchY, CX0, CY0, HX, MaxRY : integer;
  LibC, LibR, CoinC, CoinR : integer;
  LibDir   : integer;              { 0 vorn 1 hinten 2 links 3 rechts }
  BirdD    : array[1..MaxBirds] of integer;
  BirdC, BirdR, BirdT : array[1..MaxBirds] of integer;
  BirdOn   : array[1..MaxBirds] of boolean;
  BirdWait : array[1..MaxBirds] of integer;   { >0: kommt spaeter }
  NBirds   : integer;
  Level, Lives : integer;
  Score, Best, NextLife, LifeStep : real;
  StepT, MinS, EmptyMin, EmptyMax, BirdHop, StableMin, StableRng : integer;
  DrownS, BirdS, ShowS : integer;
  Frames   : integer;
  Quit, Dead, Won, Frozen : boolean;
  StartLevel, VErr : integer;
  Ch       : char;
  SpX, SpY, SpT : array[0..7] of integer;
  { Statistik: 1 Kaefer, 2 Biene, 3 Kroete, 4 Bluete }
  Given, Got : array[1..4] of integer;
  FlLife, FlDeath, FlBonus, FlBirdGo, FlBirdCome, Deaths : integer;
  PlayTicks : real;                { Bildwechsel (50 je Sekunde), Ersatz }
  StartSec  : real;                { DOS-Uhr beim Spielstart }
  HiTab    : array[1..10] of HiRec;
  HF       : file of HiRec;
  { Tonwarteschlange fuer den 8741 (Befehl 06h) }
  SqT, SqL : array[0..63] of byte;
  SqHead, SqTail, SqPh : integer;
  SoundOn  : boolean;
  BlubT    : integer;

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
  if BlubT > 0 then BlubT := BlubT - 1;
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

{ ---- Text: Zeichensatz CHARGEN0.OVR (von TurboGraf geladen), durchsichtig ---- }

procedure Txt(X, Y: integer; S: Str80; Col, Z: integer);
var I, H, J, P, A, K, Any, U: integer;
    B: Pat8;
begin
  K := 8 * (Z + 1);
  if Z <> 0 then ChrZoom(Z);
  for I := 1 to Length(S) do
  begin
    A := Ord(S[I]) + 1;
    U := -1;
    if A > 128 then
      for J := 0 to 6 do if UmlCode[J] = A - 1 then U := J;
    if (A <= 128) or (U >= 0) then
      for H := 0 to 1 do        { 0 = obere Haelfte (Bytes 9..16) }
      begin
        Any := 0;
        for J := 0 to 7 do
        begin
          if U >= 0 then
          begin
            if H = 0 then B[J] := UmlGlyph[U, 9 + J] else B[J] := UmlGlyph[U, 1 + J];
          end
          else if H = 0 then B[J] := GGGCHG[A, 9 + J] else B[J] := GGGCHG[A, 1 + J];
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

{ ---------------- Klangeffekte (nach dem Original, als Notenfolgen) ---------------- }

procedure Run(A, B, St, L: integer);   { Lauf von Ton A nach B }
begin
  if A <= B then
    while A <= B do begin Tone(A, L); A := A + St; end
  else
    while A >= B do begin Tone(A, L); A := A - St; end;
end;

procedure SndLevel;                    { 500, 800, 1300 Hz je 100 ms }
begin
  SndClear; Tone(27, 5); Tone(35, 5); Tone(42, 5);
end;

procedure SndLife;                     { 200, 1200, 500 Hz kurz }
begin
  Tone(11, 2); Tone(42, 2); Tone(27, 2);
end;

procedure SndBlub;                     { Seerose versinkt: Blubbern }
var I: integer;
begin
  if (SqHead <> SqTail) or (SqPh <> 0) or (BlubT > 0) then exit;
  BlubT := 40;
  for I := 1 to 3 do Tone(30 + Random(13), 1);
end;

procedure SndDrown;                    { Gleiten 50..2000 Hz, dann Zufall }
var I: integer;
begin
  SndClear;
  Run(1, 41, 4, 1);
  for I := 1 to 4 do
  begin
    Tone(0, 2);
    Run(18 + Random(12), 42 - Random(6), 3 + Random(3), 1);
  end;
end;

procedure SndCaught;                   { Rauschen, dann tief }
var I: integer;
begin
  SndClear;
  for I := 1 to 6 do begin Tone(1 + Random(10), 1); Tone(30 + Random(12), 1); end;
  Tone(0, 2); Tone(3, 6); Tone(1, 10);
end;

procedure SndBirdCome;
begin
  Run(1, 40, 3, 1);
end;

procedure SndBirdGo;                   { geht oder geht baden }
begin
  Run(40, 1, 3, 1);
end;

procedure SndBonus;                    { schneller Gleitton aufwaerts }
begin
  Run(13, 42, 2, 1);
end;

procedure SndKaefer;
begin
  SndClear; Tone(30, 2); Tone(37, 3);
end;

procedure SndBiene;                    { Trillern }
var I: integer;
begin
  SndClear;
  for I := 1 to 5 do begin Tone(34, 2); Tone(37, 2); end;
end;

procedure SndKroete;                   { quak - quak }
begin
  SndClear; Tone(10, 4); Tone(0, 3); Tone(8, 5);
end;

procedure SndBluete;                   { kleine Dur-Melodie }
begin
  SndClear;
  Tone(28, 2); Tone(32, 2); Tone(35, 2); Tone(40, 3); Tone(0, 3);
end;

procedure Note(N, L: integer);         { Note mit kurzer Pause danach }
begin
  Tone(N, L - 1); Tone(0, 1);
end;

procedure SndWon;                      { "Shave and a haircut - two bits" }
begin
  SndClear;
  Note(28, 15); Note(23, 10); Note(23, 5);      { Shuffle: lang - kurz }
  Note(25, 15); Note(23, 15);
  Tone(0, 15);
  Note(27, 15); Note(28, 20);
end;

procedure SndOver;
begin
  SndClear;
  Tone(35, 6); Tone(32, 6); Tone(28, 6); Tone(23, 14);
end;

procedure ToggleSound;
begin
  if SoundOn then begin SndClear; SoundOn := false; end
  else begin SoundOn := true; Tone(35, 2); Tone(40, 3); end;
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

{ ---------------- Seerosenbilder ---------------- }

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

function ImS(N: integer): integer;        { Halbachse senkrecht von Bild N }
begin
  if N <= MaxS then ImS := N else ImS := N - RingBase;
end;

function RXof(S: integer): integer;       { Halbachse waagrecht }
begin
  RXof := (S * 5) div 3;
end;

{ Bild N mit Mittelpunkt CX (durch 8 teilbar), CY (durch 4 teilbar).
  Je Ebene: erst innerhalb der Maske loeschen, dann den Inhalt setzen -
  ausserhalb der Ellipse bleibt der Bildschirm unberuehrt. }
procedure Blit(N, CX, CY: integer);
var P, V, S, R, W, X0, E: integer;
begin
  V := (CX shr 3) and 1;
  S := ImS(N);
  R := 2 * S + 5;
  W := ImW[N, V];
  X0 := (CX shr 4) - ImA[N, V];
  Cmd($4A); Par($FF); Par($FF);      { MASK ganz }
  Cmd($4C); Par($02);                { FIGS: Richtung rechts }
  for P := 0 to 2 do
  begin
    E := (CY - S - 2) * 40 + X0 + PB[P];
    BlitAsm(Img[N, V]^[0], E, R, W, 0, $22);
    BlitAsm(Img[N, V]^[(P + 1) * R * W], E, R, W, 0, $23);
  end;
  CurOK := false;
end;

function SpanW(K, A, B: integer): integer;
var L, H: integer;
begin
  L := A - 16 * K; H := B - 16 * K;
  if L < 0 then L := 0;
  if H > 15 then H := 15;
  if L > H then SpanW := 0
  else if L = 0 then SpanW := UpTo[H]
  else SpanW := UpTo[H] and not UpTo[L - 1];
end;

function HalfW(Y, RX, RY: integer): integer;  { -1 = Zeile ausserhalb }
var T: real;
begin
  if (RX <= 0) or (RY <= 0) or (Abs(Y) > RY) then HalfW := -1
  else
  begin
    T := 1.0 - Sqr(Y / (RY + 0.5));
    if T < 0 then T := 0;
    HalfW := Round(RX * Sqrt(T));
  end;
end;

{ Groessen der Bilder festlegen und Speicher holen.
  Maske = Ellipse eine Stufe groesser (deckt die vorige Seerose ab). }
procedure AllocImages;
var N, V, S, L, AL, AR, Bytes: integer;
begin
  for N := 0 to NImg do
  begin
    S := ImS(N);
    L := RXof(S + 1) + 2;
    for V := 0 to 1 do
    begin
      AL := (L - 8 * V + 15) div 16;
      AR := (L - (15 - 8 * V) + 15) div 16;
      if AR < 0 then AR := 0;
      ImA[N, V] := AL;
      ImW[N, V] := AL + 1 + AR;
      Bytes := 4 * (2 * S + 5) * ImW[N, V] * 2;
      ImRec[N, V] := (Bytes + 127) div 128;
      GetMem(Img[N, V], ImRec[N, V] * 128);
    end;
  end;
end;

function Span2(K, C, H: integer): integer;  { Zeile mit halber Breite H }
begin
  if H >= 0 then Span2 := SpanW(K, C - H, C + H) else Span2 := 0;
end;

procedure MakeImages;
var N, V, S, Y, K, DY, RX, R, W, C, Wv, Ck, Mo, Mi, Rim, Ol, G, B, Rd,
    HO, HI, HO2, HI2, HM, Mk: integer;
begin
  for N := 0 to NImg do
  begin
    S := ImS(N); RX := RXof(S); R := 2 * S + 5;
    for V := 0 to 1 do
    begin
      W := ImW[N, V];
      C := 16 * ImA[N, V] + 8 * V;        { Mittelpunkt im Bild }
      for Y := 0 to R - 1 do
      begin
        DY := Y - S - 2;
        if N <= MaxS then HM := HalfW(DY, RXof(S + 1) + 1, S + 1)
        else HM := HalfW(DY, RX + 1, S);
        Wv := PatRow[patWave, DY and 3]; Wv := Wv or (Wv shl 8);
        Ck := PatRow[patCheck, DY and 3]; Ck := Ck or (Ck shl 8);
        if N <= MaxS then
        begin                              { Seerose, oliv mit Cyan-Rand }
          HO := HalfW(DY, RX, S);
          if (S > 2) and (HO >= 0) then HI := HalfW(DY, RX - 2, S - 1)
          else HI := -1;
        end
        else
        begin                              { zwei Ringe }
          HO := HalfW(DY, RX, S);
          HI := HalfW(DY, RX - 2, S - 1);
          if S div 2 >= 3 then
          begin
            HO2 := HalfW(DY, RXof(S div 2), S div 2);
            HI2 := HalfW(DY, RXof(S div 2) - 2, S div 2 - 1);
          end
          else begin HO2 := -1; HI2 := -1; end;
        end;
        for K := 0 to W - 1 do
        begin
          if N <= MaxS then
          begin
            Mo := Span2(K, C, HO);
            Mi := Span2(K, C, HI);
            Rim := Mo and not Mi;
            Ol := Mi and Ck;
            G := Rim or Ol; Rd := Ol; B := Rim or (Wv and not Mo);
          end
          else
          begin
            Rim := (Span2(K, C, HO) and not Span2(K, C, HI)) or
                   (Span2(K, C, HO2) and not Span2(K, C, HI2));
            G := Rim; Rd := 0; B := Rim or Wv;
          end;
          Mk := Span2(K, C, HM);
          Img[N, V]^[(0 * R + Y) * W + K] := Mk;
          Img[N, V]^[(1 * R + Y) * W + K] := G and Mk;
          Img[N, V]^[(2 * R + Y) * W + K] := Rd and Mk;
          Img[N, V]^[(3 * R + Y) * W + K] := B and Mk;
        end;
      end;
    end;
  end;
end;

{ Bilder aus HOPPLER.DAT laden oder berechnen und speichern }
procedure LoadImages;
const
  Sig: string[12] = 'HOPPLER 0.6';
var F: file;
    Hdr: array[0..127] of byte;
    N, V, I, Total: integer;
    Ok: boolean;
begin
  AllocImages;
  Total := 1;
  for N := 0 to NImg do for V := 0 to 1 do Total := Total + ImRec[N, V];
  Ok := false;
  Assign(F, 'HOPPLER.DAT');
  {$I-} Reset(F); {$I+}
  if IOResult = 0 then
  begin
    if FileSize(F) = Total then
    begin
      BlockRead(F, Hdr, 1);
      Ok := true;
      for I := 1 to Length(Sig) do
        if Hdr[I - 1] <> Ord(Sig[I]) then Ok := false;
      if Ok then
        for N := 0 to NImg do
          for V := 0 to 1 do BlockRead(F, Img[N, V]^, ImRec[N, V]);
    end;
    Close(F);
  end;
  if not Ok then
  begin
    TxtC(184, 'Erster Start: Die Seerosen werden berechnet.', cWhite, 0);
    TxtC(204, 'Das dauert einmalig etwas ...', cWhite, 0);
    MakeImages;
    FillChar(Hdr, 128, 0);
    for I := 1 to Length(Sig) do Hdr[I - 1] := Ord(Sig[I]);
    {$I-}
    Rewrite(F);
    if IOResult = 0 then
    begin
      BlockWrite(F, Hdr, 1);
      for N := 0 to NImg do
        for V := 0 to 1 do BlockWrite(F, Img[N, V]^, ImRec[N, V]);
      Close(F);
    end;
    I := IOResult;
    {$I+}
  end;
end;

{ ---------------- Raster ---------------- }

procedure SetGrid;
var G: integer;
begin
  G := Min(Level, 8);
  Cols := GridC[G]; Rows := GridR[G];
  PitchX := (640 div Cols) and $FFF8;
  PitchY := (360 div Rows) and $FFFC;
  CX0 := ((640 - (Cols - 1) * PitchX) div 2) and $FFF8;
  CY0 := (PondTop + (360 - (Rows - 1) * PitchY) div 2) and $FFFC;
  MaxRY := Min(MaxS, (PitchY * 24) div 100);
  HX := ((RXof(MaxRY) + 2 + 7) div 8) * 8;
  if HX < 16 then HX := 16;
  DrownS := (MaxRY * 35) div 100;
  BirdS := (MaxRY * 45) div 100;
  ShowS := (MaxRY * 50) div 100;
end;

function CXof(C: integer): integer;
begin
  CXof := CX0 + C * PitchX;
end;

function CYof(R: integer): integer;
begin
  CYof := CY0 + R * PitchY;
end;

procedure Water(X, Y, W, H: integer);
begin
  DBox(X, Y, W, H, cBlue, cBlack, patWave);
end;

{ ---------------- Figuren ---------------- }

procedure ItemSprite(I, X, Y: integer);    { X/Y = Mitte }
begin
  case I of
    iKaefer: Sprite16(X - 8, Y - 9, SprKaefer, 0);
    iBiene:  Sprite16(X - 8, Y - 9, SprBiene, 0);
    iKroete: Sprite16(X - 8, Y - 9, SprKroete, 0);
    iBluete: Sprite16(X - 8, Y - 9, SprBluete, 0);
  end;
end;

procedure Coin(X, Y, N: integer);         { X/Y = Mitte der Seerose }
begin
  Sprite16(X - 16, Y - 22, SprCoin, 1);
  if N < 10 then Txt(X - 4, Y - 14, Num(N, 1), cBlack, 0)
  else Txt(X - 8, Y - 14, Num(N, 2), cBlack, 0);
end;

procedure Libelle(X, Y: integer);         { X/Y = Mitte der Seerose }
begin
  case LibDir of
    0: Sprite16(X - 16, Y - 26, SprLibF, 1);
    1: Sprite16(X - 16, Y - 26, SprLibB, 1);
    2: Sprite16(X - 16, Y - 26, SprLibL, 1);
    3: Sprite16(X - 16, Y - 26, SprLibR, 1);
  end;
end;

procedure Vogel(X, Y, I: integer);        { X/Y = Mitte der Seerose }
begin
  if BirdT[I] <= 10 then Sprite16(X - 16, Y - 26, SprBirdC, 1)
  else
    case BirdD[I] of
      2: Sprite16(X - 16, Y - 26, SprBirdL, 1);
      3: Sprite16(X - 16, Y - 26, SprBirdR, 1);
    else Sprite16(X - 16, Y - 26, SprBird, 1);
    end;
end;

function BirdAt(C, R: integer): integer;
var I: integer;
begin
  BirdAt := 0;
  for I := 1 to MaxBirds do
    if BirdOn[I] and (BirdC[I] = C) and (BirdR[I] = R) then BirdAt := I;
end;

procedure Overlays(C, R: integer);
var CX, CY: integer;
begin
  CX := CXof(C); CY := CYof(R);
  with Cell[C, R] do
  begin
    if (Size >= ShowS) and (Item <> iNone) then ItemSprite(Item, CX, CY);
    if (Size >= ShowS) and (C = CoinC) and (R = CoinR) then
      Coin(CX, CY, Level);
  end;
  if (C = LibC) and (R = LibR) and not Dead then Libelle(CX, CY);
  if BirdAt(C, R) <> 0 then Vogel(CX, CY, BirdAt(C, R));
end;

procedure DrawCell(C, R: integer);
var CX, CY: integer;
begin
  CX := CXof(C); CY := CYof(R);
  Water(CX - HX, CY - 28, 2 * HX, 31 + MaxRY);
  with Cell[C, R] do
    if State = stRipple then
      Blit(RingBase + Max(3, (MaxRY * (RF + 2)) div 5), CX, CY)
    else if Size > 0 then Blit(Size, CX, CY);
  if Cell[C, R].State <> stRipple then Overlays(C, R);
end;

procedure Shrunk(C, R: integer);           { eine Zeile kleiner }
begin
  Blit(Cell[C, R].Size, CXof(C), CYof(R));
  Overlays(C, R);
end;

{ ---------------- Statuszeile ---------------- }

procedure Sky(X, Y, W, H: integer);
begin
  DBox(X, Y, W, H, cWhite, cBlue, patCheck);
end;

procedure UpdLevel;
begin
  Sky(64, 8, 16, 16);
  Txt(64, 8, Num(Level, 2), cRed, 0);
end;

procedure UpdLives;
begin
  Sky(232, 8, 8, 16);
  Txt(232, 8, Num(Lives, 1), cRed, 0);
end;

procedure AddScore(D: real);
begin
  Score := Score + D;
  if Score < 0 then Score := 0;
  { Extraleben wie im Original: Schwellen 10000, 30000, 70000, ... }
  if Score >= NextLife then
  begin
    NextLife := NextLife + LifeStep;
    LifeStep := LifeStep * 2;
    if Lives < 6 then begin Lives := Lives + 1; UpdLives; end;
    SndLife;
  end;
  if Score > Best then Best := Score;
  Sky(384, 8, 56, 16);
  Txt(384, 8, NumR(Score, 7), cRed, 0);
  Sky(560, 8, 56, 16);
  Txt(560, 8, NumR(Best, 7), cRed, 0);
end;

procedure DrawStatus;
var I: integer;
begin
  Sky(0, 0, 640, 32);
  Box(0, 32, 640, 8, cBlue);
  TxtB(16, 8, 'LEVEL', cBlack);
  TxtB(184, 8, 'LEBEN', cBlack);
  TxtB(328, 8, 'PUNKTE', cBlack);
  TxtB(504, 8, 'REKORD', cBlack);
  UpdLevel;
  UpdLives;
  AddScore(0);
  for I := 0 to 7 do SpT[I] := -1;
end;

procedure Sparkle;            { Glitzern im dunkelblauen Streifen }
var I: integer;
begin
  for I := 0 to 7 do
  begin
    SpT[I] := SpT[I] - 1;
    if SpT[I] <= 0 then
    begin
      if SpT[I] = 0 then Box(SpX[I], SpY[I], 6, 1, cBlue);
      SpX[I] := Random(634);
      SpY[I] := 33 + Random(6);
      if Random(2) = 0 then Box(SpX[I], SpY[I], 6, 1, cCyan)
      else Box(SpX[I], SpY[I], 4, 1, cWhite);
      SpT[I] := 10 + Random(40);
      exit;                   { hoechstens eins je Bild }
    end;
  end;
end;

{ ---------------- Level ---------------- }

procedure SetLevelParams;
var L: integer;
begin
  L := Level - 1;                          { 0..24 }
  SetGrid;
  StepT    := Max(2, (130 - 3 * L) div MaxRY);   { Bilder je Zeile Schrumpfen }
  MinS     := (MaxRY * 7) div 10;           { kleinste neue Seerose }
  StableMin := Max(30, 150 - 5 * L);        { so lange bleibt sie ruhig }
  StableRng := 150;
  EmptyMin := 100;
  EmptyMax := Max(220, 450 - 6 * L);
  BirdHop  := Max(22, 70 - 2 * L);
end;

procedure NewPad(C, R: integer);
begin
  with Cell[C, R] do
  begin
    State := stLive;
    Size := MinS + Random(MaxRY - MinS + 1);
    Timer := StableMin + Random(StableRng);
  end;
end;

procedure SetEmpty(C, R: integer);
begin
  with Cell[C, R] do
  begin
    State := stEmpty; Size := 0;
    Timer := EmptyMin + Random(EmptyMax - EmptyMin);
  end;
end;

procedure PlaceBird(I: integer);
var C, R, N: integer;
    Ok: boolean;
begin
  N := 0;
  repeat
    C := Cols div 2 + Random(Cols - Cols div 2); R := Random(Rows);
    Ok := (Cell[C, R].Size >= BirdS + 2) and (BirdAt(C, R) = 0) and
          (Abs(C - LibC) + Abs(R - LibR) >= 4) and
          not ((C = CoinC) and (R = CoinR));
    N := N + 1;
  until Ok or (N > 60);
  BirdOn[I] := Ok;
  BirdC[I] := C; BirdR[I] := R;
  BirdT[I] := BirdHop + Random(BirdHop);
  BirdD[I] := 0;
  if Ok then begin Overlays(C, R); SndBirdCome; end;
end;

procedure InitLevel;
var C, R, I, K: integer;
begin
  SetLevelParams;
  LibC := 0; LibR := Rows - 1;               { Start unten links }
  if Level <= 12 then K := 0 else K := Random(3);
  case K of                                  { Muenze; bis Level 12 oben rechts }
    0: begin CoinC := Cols - 1; CoinR := 0; end;
    1: begin CoinC := Cols - 1; CoinR := Rows - 1; end;
    2: begin CoinC := 0; CoinR := 0; end;
  end;
  for C := 0 to Cols - 1 do
    for R := 0 to Rows - 1 do
    begin
      with Cell[C, R] do
      begin
        Item := iNone; RF := 0;
        { Tiere wie im Original beim Aufbau: Kaefer L/50, Biene L/190 }
        if not ((C = LibC) and (R = LibR)) and
           not ((C = CoinC) and (R = CoinR)) then
        begin
          { Kaefer: Level 1 2/50, dann (Level+1)/50 - Mittelweg zwischen
            dem Code des Originals (L/50 ab Level 2) und der Beobachtung }
          if Random(50) < Max(2, Level + 1) then Item := iKaefer
          else if (Level >= 2) and (Random(190) < Level) then Item := iBiene;
          if Item <> iNone then Given[Item] := Given[Item] + 1;
        end;
      end;
      if Random(100) < 45 then
      begin
        NewPad(C, R);
        Cell[C, R].Timer := Random(StableMin + StableRng);
      end
      else
      begin
        SetEmpty(C, R);
        Cell[C, R].Timer := Random(EmptyMax);
      end;
    end;
  { Bluete ab Level 5 mit Wahrscheinlichkeit 1/3 }
  if (Level >= 5) and (Random(3) = 0) then
  begin
    K := 0;
    repeat
      C := Random(Cols); R := Random(Rows); K := K + 1;
    until ((Cell[C, R].Item = iNone) and not ((C = LibC) and (R = LibR)) and
           not ((C = CoinC) and (R = CoinR))) or (K > 50);
    if K <= 50 then
    begin
      Cell[C, R].Item := iBluete;
      Given[4] := Given[4] + 1;
    end;
  end;
  { Start- und Muenzfeld fest; das Startfeld nur bis zum ersten Sprung }
  with Cell[LibC, LibR] do begin State := stFixed; Size := MaxRY; end;
  with Cell[CoinC, CoinR] do begin State := stFixed; Size := MaxRY; end;
  Frozen := true;
  LibDir := 0;
  Dead := false; Won := false;
  { Dummvoegel: L7-14 meist einer (kommt spaeter), L15 einer, ab L16 zwei }
  for I := 1 to MaxBirds do begin BirdOn[I] := false; BirdWait[I] := 0; end;
  if Level >= 16 then NBirds := 2
  else if Level >= 7 then NBirds := 1
  else NBirds := 0;
  if (Level >= 7) and (Level <= 14) then
  begin
    if Random(4) = 0 then NBirds := 0
    else BirdWait[1] := 100 + Random(400);
  end;
end;

procedure StartBirds;
var I: integer;
begin
  for I := 1 to NBirds do
    if BirdWait[I] = 0 then PlaceBird(I);
end;

procedure DrawPond;
var C, R: integer;
begin
  Water(0, PondTop, 640, 400 - PondTop);
  for R := 0 to Rows - 1 do
    for C := 0 to Cols - 1 do
      if (Cell[C, R].Size > 0) or ((C = LibC) and (R = LibR)) then
        DrawCell(C, R);
end;

procedure RedrawArea(X0, Y0, X1, Y1: integer);   { Teich unter Popup }
var C, R: integer;
begin
  Water(X0, Y0, X1 - X0 + 1, Y1 - Y0 + 1);
  for R := 0 to Rows - 1 do
    for C := 0 to Cols - 1 do
      if (CXof(C) + HX > X0) and (CXof(C) - HX <= X1) and
         (CYof(R) + MaxRY + 3 > Y0) and (CYof(R) - 28 <= Y1) then
        DrawCell(C, R);
end;

{ ---------------- Popups ---------------- }

const
  PopX = 160; PopY = 136; PopW = 320; PopH = 128;

procedure PopBox;
begin
  Box(PopX - 4, PopY - 4, PopW + 8, PopH + 8, cBlack);
  DBox(PopX, PopY, PopW, PopH, cRed, cYellow, patCheck);   { orange }
end;

procedure PopClose;
begin
  RedrawArea(PopX - 8, PopY - 4, PopX + PopW + 7, PopY + PopH + 3);
end;

procedure Popup(S1, S2: Str80; Col: integer);
begin
  PopBox;
  TxtC(PopY + 20, S1, Col, 1);
  TxtB(320 - Length(S2) * 4, PopY + 76, S2, cBlack);
end;

procedure FlushKeys;              { Tastaturpuffer leeren }
var C: char;
begin
  while KeyPressed do Read(Kbd, C);
end;

procedure RawKey;                 { ein Zeichen, unveraendert }
begin
  Ch := #0;
  if KeyPressed then Read(Kbd, Ch);
end;

{ Taste lesen. Sondertasten kommen als ESC bzw. #0 plus Zeichen: Pfeile
  (IBM H/P/K/M, ANSI [A..D) werden zu 8/2/4/6, andere verworfen. ESC
  allein bleibt ESC. ADM-3A-Pfeile ^K ^J ^H ^L ebenfalls zu 8/2/4/6. }
procedure ReadKey;
var First: char;
    I: integer;
begin
  Ch := #0;
  if not KeyPressed then exit;
  Read(Kbd, Ch);
  if (Ch = #27) or (Ch = #0) then
  begin
    First := Ch;
    I := 0;
    while (I < 3) and not KeyPressed do
    begin
      WaitFrame;
      I := I + 1;
    end;
    if KeyPressed then
    begin
      Read(Kbd, Ch);
      if (Ch = '[') and KeyPressed then Read(Kbd, Ch);
      case Ch of
        'H', 'A': Ch := '8';
        'P', 'B': Ch := '2';
        'K', 'D': Ch := '4';
        'M', 'C': Ch := '6';
      else Ch := #255;                  { andere Sondertaste: verwerfen }
      end;
      FlushKeys;
    end
    else if First = #0 then Ch := #255;
  end
  else
    case Ch of
      #11: Ch := '8';
      #10: Ch := '2';
      #8:  Ch := '4';
      #12: Ch := '6';
    end;
  if Ch = #255 then Ch := #0;
end;

procedure WaitKey;                { mit leerem Puffer davor und danach }
begin
  FlushKeys;
  repeat
    WaitFrame;
    ReadKey;
  until Ch <> #0;
  FlushKeys;
end;

procedure Condolence;        { Beileid; fuenf Dummvoegel tanzen }
var T, I, X, Y: integer;
begin
  PopBox;
  TxtC(PopY + 8, 'HERZLICHES', cRed, 1);
  TxtC(PopY + 40, 'BEILEID', cRed, 1);
  Y := PopY + 84;
  for T := 0 to 79 do
  begin
    WaitFrame;
    if (T mod 10) = 0 then
      for I := 0 to 4 do
      begin
        X := 320 - 100 + I * 40 + 4;
        DBox(X, Y, 32, 32, cRed, cYellow, patCheck);
        if ((T div 10 + I) and 1) = 0 then Sprite16(X, Y, SprBird, 1)
        else Sprite16(X, Y, SprBirdC, 1);
      end;
  end;
  PopClose;
  FlushKeys;
end;

{ ---------------- Spielablauf ---------------- }

procedure Die(Splash: boolean);
var I: integer;
begin
  Dead := true;
  DrawCell(LibC, LibR);
  if Splash then Sprite16(CXof(LibC) - 16, CYof(LibR) - 16, SprSplash, 1);
  if BirdAt(LibC, LibR) <> 0 then SndCaught else SndDrown;
  Wait(30);
  DrawCell(LibC, LibR);
end;

procedure Vanish(C, R: integer);           { Seerose weg: Ringe }
var I: integer;
begin
  with Cell[C, R] do
  begin
    State := stRipple; Size := 0; RF := 0; Timer := 6;
    if Item = iKroete then Item := iNone;
  end;
  DrawCell(C, R);
  SndBlub;
  I := BirdAt(C, R);
  if I <> 0 then begin BirdOn[I] := false; SndBirdGo; end;   { Dummvogel geht baden }
  if (C = LibC) and (R = LibR) and not Dead then Die(false);
end;

procedure BirdGone(I: integer);            { Dummvogel verschwindet }
var C, R: integer;
begin
  C := BirdC[I]; R := BirdR[I];
  BirdOn[I] := false;
  DrawCell(C, R);
  SndBirdGo;
end;

procedure Collect;
var D, I: integer;
begin
  D := Cell[LibC, LibR].Item;
  if D = iNone then exit;
  Cell[LibC, LibR].Item := iNone;
  Got[D] := Got[D] + 1;
  case D of
    iKaefer: begin SndKaefer; AddScore(50.0 * Level); end;
    iBiene:  begin SndBiene; AddScore(1000.0 * Level); end;
    iKroete: begin SndKroete; AddScore(-250.0 * Level); end;
    iBluete:                                { fuenf Ausgaenge wie im Original }
      case Random(5) of
        0: begin
             FlLife := FlLife + 1;
             SndBluete;
             if Lives < 6 then begin Lives := Lives + 1; UpdLives; end;
             SndLife;
           end;
        1: begin FlDeath := FlDeath + 1; Vanish(LibC, LibR); exit; end;
        2: begin FlBonus := FlBonus + 1; SndBluete; SndBonus; AddScore(2000.0 * Level); end;
        3, 4:
          begin
            I := 0;
            if BirdOn[1] then I := 1 else if BirdOn[2] then I := 2;
            if I <> 0 then
            begin
              FlBirdGo := FlBirdGo + 1;
              SndBluete;
              BirdGone(I);                   { Dummvogel geht }
            end
            else
            begin                            { Dummvogel kommt }
              FlBirdCome := FlBirdCome + 1;
              SndBluete;
              if NBirds < 1 then NBirds := 1;
              BirdWait[1] := 0;
              PlaceBird(1);
            end;
          end;
      end;
  end;
  DrawCell(LibC, LibR);
end;

procedure SpawnToad;           { Kroete wie im Original, selten und zufaellig }
var C, R: integer;
begin
  if Random(21) <> 0 then exit;
  if Random(350 - 10 * (Level - 10)) <> 0 then exit;
  C := Random(Cols); R := Random(Rows);
  if ((C = 0) or (C = Cols - 1)) and ((R = 0) or (R = Rows - 1)) then exit;
  if (C = LibC) and (R = LibR) then exit;
  with Cell[C, R] do
  begin
    if Item <> iNone then exit;
    Item := iKroete;
  end;
  Given[3] := Given[3] + 1;
  if Cell[C, R].Size > 0 then Overlays(C, R);
end;

procedure UpdateCells;
var C, R: integer;
begin
  for C := 0 to Cols - 1 do
    for R := 0 to Rows - 1 do
      with Cell[C, R] do
        if State <> stFixed then
        begin
          Timer := Timer - 1;
          if Timer <= 0 then
            case State of
              stLive:
                begin
                  State := stShrink;
                  Timer := StepT;
                end;
              stShrink:
                begin
                  Size := Size - 1;
                  Timer := StepT;
                  if Size < 3 then Vanish(C, R)
                  else
                  begin
                    Shrunk(C, R);
                    if (Size < DrownS) and (C = LibC) and (R = LibR) and not Dead then
                      Die(false);
                    if (Size < BirdS) and (BirdAt(C, R) <> 0) then
                      BirdGone(BirdAt(C, R));
                  end;
                end;
              stRipple:
                begin
                  RF := RF + 1;
                  if RF > 3 then SetEmpty(C, R) else Timer := 6;
                  DrawCell(C, R);
                end;
              stEmpty:
                begin
                  NewPad(C, R);
                  DrawCell(C, R);
                end;
            end;
        end;
end;

{ Nachbarhilfe: Hat die Libelle kein brauchbares Blatt neben sich und
  taucht auch keines bald auf, dann waechst auf einem leeren Nachbarfeld
  in Kuerze eines nach. }
procedure HelpLib;
const
  DX: array[0..3] of integer = (0, 0, -1, 1);
  DY: array[0..3] of integer = (-1, 1, 0, 0);
var D, NC, NR, N, Pick: integer;
    Ok: boolean;
begin
  if Frozen or Dead then exit;
  Ok := false; N := 0; Pick := -1;
  for D := 0 to 3 do
  begin
    NC := LibC + DX[D]; NR := LibR + DY[D];
    if (NC >= 0) and (NC < Cols) and (NR >= 0) and (NR < Rows) then
      with Cell[NC, NR] do
      begin
        if (Size >= DrownS + 3) and (State <> stShrink) then Ok := true;
        if (State = stEmpty) and (Timer <= 150) then Ok := true;
        if State = stEmpty then
        begin
          N := N + 1;
          if Random(N) = 0 then Pick := D;   { zufaellig unter den leeren }
        end;
      end;
  end;
  if (not Ok) and (Pick >= 0) then
    Cell[LibC + DX[Pick], LibR + DY[Pick]].Timer := 75 + Random(75);
end;

procedure MoveBirds;
const
  DX: array[0..3] of integer = (0, 0, -1, 1);
  DY: array[0..3] of integer = (-1, 1, 0, 0);
var I, D, NC, NR, BestD, BestN, Dist, OC, ORow: integer;
begin
  for I := 1 to NBirds do
    if BirdWait[I] > 0 then
    begin
      BirdWait[I] := BirdWait[I] - 1;
      if BirdWait[I] = 0 then PlaceBird(I);
    end
    else
    begin
      BirdT[I] := BirdT[I] - 1;
      if (BirdT[I] = 10) and BirdOn[I] then DrawCell(BirdC[I], BirdR[I]);  { ducken }
      if BirdT[I] <= 0 then
      begin
        BirdT[I] := BirdHop;
        if not BirdOn[I] then PlaceBird(I)    { neuer Dummvogel kommt }
        else
        begin
          BestD := 999; BestN := -1;
          for D := 0 to 3 do
          begin
            NC := BirdC[I] + DX[D]; NR := BirdR[I] + DY[D];
            if (NC >= 0) and (NC < Cols) and (NR >= 0) and (NR < Rows) then
              if (Cell[NC, NR].Size >= BirdS + 2) and (BirdAt(NC, NR) = 0) then
              begin
                Dist := Abs(NC - LibC) + Abs(NR - LibR) + Random(2);
                if Random(5) = 0 then Dist := Random(6);   { etwas dumm }
                if Dist < BestD then begin BestD := Dist; BestN := D; end;
              end;
          end;
          if BestN < 0 then DrawCell(BirdC[I], BirdR[I])   { wieder aufrichten }
          else
          begin
            OC := BirdC[I]; ORow := BirdR[I];
            BirdC[I] := OC + DX[BestN]; BirdR[I] := ORow + DY[BestN];
            BirdD[I] := BestN;
            DrawCell(OC, ORow);
            Overlays(BirdC[I], BirdR[I]);
            if (BirdC[I] = LibC) and (BirdR[I] = LibR) and not Dead then
              Die(false);
          end;
        end;
      end;
    end;
end;

procedure Jump(DC, DR: integer);
var NC, NR, OC, ORow: integer;
begin
  NC := LibC + DC; NR := LibR + DR;
  if (NC < 0) or (NC >= Cols) or (NR < 0) or (NR >= Rows) then exit;
  OC := LibC; ORow := LibR;
  LibC := NC; LibR := NR;
  if DR < 0 then LibDir := 1
  else if DR > 0 then LibDir := 0
  else if DC < 0 then LibDir := 2
  else LibDir := 3;
  if Frozen then
  begin
    Frozen := false;
    if (OC <> CoinC) or (ORow <> CoinR) then Cell[OC, ORow].State := stLive;
  end;
  DrawCell(OC, ORow);
  if (Cell[NC, NR].Size < DrownS) then
  begin
    Die(true);
    exit;
  end;
  Overlays(NC, NR);
  if BirdAt(NC, NR) <> 0 then
  begin
    Die(false);
    exit;
  end;
  if (NC = CoinC) and (NR = CoinR) then
  begin
    Won := true;
    exit;
  end;
  Collect;
end;

procedure Respawn;
var C, R, BC, BR, BestD, D: integer;
begin
  { grosse Seerose moeglichst weit links unten, ohne Dummvogel }
  BC := -1; BR := 0; BestD := 9999;
  for C := 0 to Cols - 1 do
    for R := 0 to Rows - 1 do
      with Cell[C, R] do
        if (Size >= MinS) and (BirdAt(C, R) = 0) and
           not ((C = CoinC) and (R = CoinR)) then
        begin
          D := C * 30 + (Rows - 1 - R) * 10 - Size;
          if D < BestD then begin BestD := D; BC := C; BR := R; end;
        end;
  if BC < 0 then begin BC := 0; BR := Rows - 1; end;
  with Cell[BC, BR] do
  begin
    State := stFixed; Size := MaxRY;
    if Item = iKroete then Item := iNone;
  end;
  Frozen := true;
  LibC := BC; LibR := BR;
  LibDir := 0;
  Dead := false;
  DrawCell(LibC, LibR);
end;

procedure PlayLevel;
begin
  InitLevel;
  DrawStatus;
  DrawPond;
  Popup('LEVEL ' + Num(Level, 2), 'Taste drÅcken', cRed);
  WaitKey;
  if Ch = #27 then begin Quit := true; exit; end;
  PopClose;
  SndLevel;
  StartBirds;
  repeat
    WaitFrame;
    ReadKey;
    case Ch of
      '8': Jump( 0, -1);
      '2': Jump( 0,  1);
      '4': Jump(-1,  0);
      '6': Jump( 1,  0);
      's', 'S': ToggleSound;
      'p', 'P':
        begin
          Popup('PAUSE', 'Taste drÅcken', cRed);
          WaitKey;
          PopClose;
        end;
      #27: Quit := true;
    end;
    if not (Dead or Won or Quit) then
    begin
      UpdateCells;
      if (Frames and 15) = 0 then HelpLib;
      if not Dead then MoveBirds;
      PlayTicks := PlayTicks + 1;
      SpawnToad;
      Sparkle;
    end;
    if Dead then
    begin
      Deaths := Deaths + 1;
      Lives := Lives - 1;
      UpdLives;
      Condolence;
      if Lives > 0 then Respawn;
    end;
  until Won or Quit or (Lives <= 0);
  if Won then
  begin
    AddScore(500.0 + 100.0 * Level);
    Popup('GESCHAFFT!', 'Bonus ' + Num(500 + 100 * Level, 1) + ' Punkte', cRed);
    SndWon;
    Wait(130);
  end;
end;

{ ---------------- Titelbild ---------------- }

procedure ShowSnd;                     { Tonzustand im Titelbild }
begin
  DBox(16, 384, 64, 16, cBlue, cBlack, pat25);
  if SoundOn then Txt(16, 384, 'Ton an', cBlue, 0)
  else Txt(16, 384, 'Ton aus', cBlue, 0);
end;

procedure DrawTitle;
const
  TS: array[0..9] of integer = (21, 17, 21, 13, 9, 5, 0, 11, 16, 21);
var I, Y: integer;
begin
  Box(0, 0, 640, 400, cBlack);
  case TitleVar of
    1: begin                                { Abendhimmel und Ufer }
         DBox(0, 0, 640, 24, cBlue, cBlack, pat12);
         DBox(0, 24, 640, 24, cBlue, cBlack, pat25);
         DBox(0, 48, 640, 28, cBlue, cBlack, patCheck);
         DBox(0, 76, 640, 28, cBlack, cBlue, pat25);
         DBox(0, 176, 640, 224, cGreen, cBlack, pat12);
       end;
    2: DBox(0, 0, 640, 400, cBlue, cBlack, pat25);   { dunkelblau }
  end;
  Txt(212, 12, 'HOPPLER', cRed, 3);
  Txt(208, 8, 'HOPPLER', cYellow, 3);
  TxtC(80, 'frei nach Perestroika/Toppler', cWhite, 0);
  Y := 144;
  Water(0, 104, 640, 72);
  for I := 0 to 9 do
    if I = 6 then Blit(RingBase + 14, 32 + I * 64, Y)
    else Blit(TS[I], 32 + I * 64, Y);
  LibDir := 0;
  Libelle(32, Y);
  BirdT[1] := 99; BirdD[1] := 0;
  Vogel(160, Y, 1);
  Level := 0;
  Coin(608, Y, 1);
  Txt(24, 188, 'Im Original wetteiferten Demokrat und BÅrokrat um Geld und Ressourcen.', cYellow, 0);
  Txt(24, 206, 'Hier treffen sich Cartoonfiguren am Teich: Die Drachenlibelle hÅpft', cYellow, 0);
  Txt(24, 224, 'Åber kleiner werdende SeerosenblÑtter. Der gro·e Dummvogel will sie', cYellow, 0);
  Txt(24, 242, 'fangen. Mit dem Ziffernblock 8 4 6 2 zur MÅnze - nicht baden gehen!', cYellow, 0);
  Y := 268;
  Blit(9, 64, Y + 8);  ItemSprite(iKaefer, 64, Y + 8);
  Txt(96, Y, 'WasserkÑfer  50 x Level', cWhite, 0);
  Blit(9, 344, Y + 8); ItemSprite(iBiene, 344, Y + 8);
  Txt(376, Y, 'Biene  1000 x Level', cWhite, 0);
  Y := Y + 28;
  Blit(9, 64, Y + 8);  ItemSprite(iKroete, 64, Y + 8);
  Txt(96, Y, 'Krîte  -250 x Level', cWhite, 0);
  Blit(9, 344, Y + 8); ItemSprite(iBluete, 344, Y + 8);
  Txt(376, Y, 'BlÅte: GlÅck oder Pech', cWhite, 0);
  TxtC(338, 'Leertaste: Start  B: Bestenliste  P: Pause  S: Ton  ESC: Ende', cGreen, 0);
  ShowSnd;
  TxtC(376, 'Code: Claude, Prompts: rfka01 (2026)', cCyan, 0);
  Txt(584, 384, Version, cBlue, 0);
end;

{ ---------------- Statistik und Bestenliste ---------------- }

procedure LoadHi;
var I: integer;
begin
  for I := 1 to 10 do
  begin
    HiTab[I].Name := '-'; HiTab[I].Score := 0; HiTab[I].Lvl := 0;
  end;
  Assign(HF, 'HOPPLER.HI');
  {$I-} Reset(HF); {$I+}
  if IOResult = 0 then
  begin
    I := 0;
    while (I < 10) and not Eof(HF) do
    begin
      I := I + 1;
      Read(HF, HiTab[I]);
    end;
    Close(HF);
  end;
end;

procedure SaveHi;
var I: integer;
begin
  Assign(HF, 'HOPPLER.HI');
  {$I-}
  Rewrite(HF);
  if IOResult = 0 then
  begin
    for I := 1 to 10 do Write(HF, HiTab[I]);
    Close(HF);
  end;
  I := IOResult;
  {$I+}
end;

procedure DarkBack;               { Hintergrund wie das Titelbild }
begin
  DBox(0, 0, 640, 400, cBlue, cBlack, pat25);
end;

procedure ShowHi(Mark: integer);
var I, Y, C: integer;
    S: Str80;
begin
  DarkBack;
  Txt(228, 20, 'BESTENLISTE', cRed, 1);
  Txt(226, 18, 'BESTENLISTE', cYellow, 1);
  for I := 1 to 10 do
  begin
    Y := 76 + I * 26;
    if I = Mark then C := cGreen else C := cWhite;
    Txt(128, Y, Num(I, 2) + '.', C, 0);
    Txt(168, Y, HiTab[I].Name, C, 0);
    if HiTab[I].Score > 0 then
    begin
      Str(HiTab[I].Score:7:0, S);
      Txt(312, Y, S, C, 0);
      Txt(400, Y, 'Level ' + Num(HiTab[I].Lvl, 2), C, 0);
    end;
  end;
  TxtC(376, 'Taste drÅcken', cGreen, 0);
  WaitKey;
end;

function EnterName(Pos: integer): Str80;
var S: Str80;
    Done: boolean;
begin
  DarkBack;
  TxtC(60, 'NEUER EINTRAG', cYellow, 1);
  TxtC(110, 'Platz ' + Num(Pos, 1) + ' mit ' + NumR(Score, 1) + ' Punkten', cWhite, 0);
  TxtC(170, 'Wie hei·t du?  (Eingabe mit RETURN)', cWhite, 0);
  S := '';
  Done := false;
  repeat
    Box(264, 212, 112, 24, cBlack);
    Txt(272, 216, S + '_', cGreen, 0);
    repeat WaitFrame; RawKey; until Ch <> #0;
    if (Ch = #13) or (Ch = #27) then Done := true
    else if ((Ch = #8) or (Ch = #127)) and (Length(S) > 0) then
      S := Copy(S, 1, Length(S) - 1)
    else if (Length(S) < 12) and
            (((Ch >= ' ') and (Ch <= '~')) or (Ord(Ch) = $84) or
             (Ord(Ch) = $94) or (Ord(Ch) = $81) or (Ord(Ch) = $8E) or
             (Ord(Ch) = $99) or (Ord(Ch) = $9A) or (Ord(Ch) = $E1)) then
      S := S + Ch;
  until Done;
  if S = '' then S := '???';
  EnterName := S;
end;

procedure StatLine(Y, I: integer; Name: Str80);
var S: Str80;
begin
  case I of
    1: Sprite16(136, Y, SprKaefer, 0);
    2: Sprite16(136, Y, SprBiene, 0);
    3: Sprite16(136, Y, SprKroete, 0);
    4: Sprite16(136, Y, SprBluete, 0);
  end;
  TxtB(160, Y, Name, cBlack);
  Str(Given[I]:3, S);
  TxtB(344, Y, S, cBlack);
  Str(Got[I]:3, S);
  TxtB(448, Y, S, cRed);
end;

function DosSec: real;            { Sekunden seit Mitternacht (DOS-Uhr) }
var Rg: DosRegs;
begin
  Rg.AX := $2C00;
  MsDos(Rg);
  DosSec := 3600.0 * Hi(Rg.CX) + 60.0 * Lo(Rg.CX) + Hi(Rg.DX) + Lo(Rg.DX) / 100.0;
end;

procedure Statistics;
var Sec: real;
    M, S: integer;
begin
  Box(76, 52, 488, 296, cBlack);
  DBox(80, 56, 480, 288, cRed, cYellow, patCheck);
  TxtC(68, 'Statistik deines Spiels', cRed, 1);
  Sec := DosSec - StartSec;
  if Sec < 0 then Sec := Sec + 86400.0;     { ueber Mitternacht }
  if Sec < 1 then Sec := PlayTicks / 50.0;  { DOS-Uhr steht: Bildwechsel }
  M := Trunc(Sec / 60.0);
  S := Trunc(Sec - 60.0 * M);
  TxtB(320 - 22 * 4, 112, 'Spieldauer ' + Num(M, 1) + ' Min. ' + Num(S, 2) + ' Sek.', cBlack);
  TxtB(320, 140, 'verteilt  erwischt', cBlack);
  StatLine(162, 1, 'WasserkÑfer');
  StatLine(184, 2, 'Biene');
  StatLine(206, 3, 'Krîte');
  StatLine(228, 4, 'SeerosenblÅte');
  TxtB(104, 260, 'BlÅte: Extraleben ' + Num(FlLife, 1) + '  Pech ' + Num(FlDeath, 1) +
       '  Bonus ' + Num(FlBonus, 1), cBlack);
  TxtB(104, 282, 'Dummvogel kam ' + Num(FlBirdCome, 1) + '  ging ' + Num(FlBirdGo, 1) +
       '   Leben verloren ' + Num(Deaths, 1), cBlack);
  TxtB(320 - 13 * 4, 316, 'Taste drÅcken', cBlack);
  WaitKey;
end;

procedure HiScore;                { nach dem Spiel: Eintrag in die Liste }
var I, P: integer;
begin
  P := 0;
  for I := 10 downto 1 do
    if Score > HiTab[I].Score then P := I;
  if P = 0 then exit;
  for I := 10 downto P + 1 do HiTab[I] := HiTab[I - 1];
  HiTab[P].Score := Score;
  HiTab[P].Lvl := Min(Level, LastLevel);
  HiTab[P].Name := '';
  HiTab[P].Name := EnterName(P);
  SaveHi;
  ShowHi(P);
end;

procedure Title;
var I: integer;
begin
  repeat
    DrawTitle;
    repeat
      WaitFrame;
      I := Random(10);            { Zufall vom Warten abhaengig machen }
      ReadKey;
      if (Ch = 's') or (Ch = 'S') then begin ToggleSound; ShowSnd; end;
    until (Ch = ' ') or (Ch = #27) or (Ch = #13) or (Ch = 'b') or (Ch = 'B');
    if (Ch = 'b') or (Ch = 'B') then ShowHi(0);
  until (Ch = ' ') or (Ch = #27) or (Ch = #13);
  Quit := Ch = #27;
end;

begin
  if not IsDMV then
  begin
    Writeln('HOPPLER lÑuft nur auf einer NCR Decision Mate V mit Farbgrafik.');
    Halt;
  end;
  GINIT;
  SELCHAR(0, 0);                  { CHARGEN0.OVR laden }
  CurOK := false;
  Frames := 0;
  SqHead := 0; SqTail := 0; SqPh := 0; BlubT := 0;
  SoundOn := true;
  Best := 0;
  LibDir := 0;
  Box(0, 0, 640, 400, cBlack);
  LoadImages;
  LoadHi;
  Randomize;
  StartLevel := 1;                { HOPPLER n = Start in Level n }
  if ParamCount > 0 then
  begin
    Val(ParamStr(1), StartLevel, VErr);
    if (VErr <> 0) or (StartLevel < 1) or (StartLevel > LastLevel) then
      StartLevel := 1;
  end;
  repeat
    Title;
    if not Quit then
    begin
      Level := StartLevel; Lives := 3; Score := 0;
      FillChar(Given, SizeOf(Given), 0);
      FillChar(Got, SizeOf(Got), 0);
      FlLife := 0; FlDeath := 0; FlBonus := 0; FlBirdGo := 0;
      FlBirdCome := 0; Deaths := 0; PlayTicks := 0;
      StartSec := DosSec;
      NextLife := 10000.0; LifeStep := 20000.0;
      repeat
        PlayLevel;
        if Won then Level := Level + 1;
      until Quit or (Lives <= 0) or (Level > LastLevel);
      if Level > LastLevel then
      begin
        Popup('ALLE 25 LEVEL!', 'Punkte ' + NumR(Score, 1), cRed);
        SndWon;
        Wait(50);
        WaitKey;
        Statistics;
        HiScore;
      end
      else if Lives <= 0 then
      begin
        Popup('GAME OVER', 'Punkte ' + NumR(Score, 1), cRed);
        SndOver;
        Wait(50);
        WaitKey;
        Statistics;
        HiScore;
      end;
      Quit := false;
    end;
  until Quit;
  SndIdle;
  GEXIT;
end.
