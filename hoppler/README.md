# Hoppler

*frei nach Perestroika/Toppler für die NCR DMV – loosely based on Perestroika/Toppler for the NCR DMV*

Version 0.18 (08.10.2026, Testfassung / test release) – Diskette / disk image: [`../images/HOPPLER-V0.18.IMG`](../images/HOPPLER-V0.18.IMG)

![Titelbild / title screen](screenshots/titel.png)
![Level 1](screenshots/level1.png)

*Screenshots: MAME, Treiber / driver `dmv` (Version 0.16)*

## Deutsch

Im Original (Perestroika/Toppler, Moskau 1990) wetteiferten Demokrat und Bürokrat um Geld und Ressourcen.
Hier treffen sich Cartoonfiguren am Teich: Die Drachenlibelle hüpft über kleiner werdende Seerosenblätter
zur Münze. Der große Dummvogel will sie fangen.

- 25 Level, Raster wächst wie im Original von 7×4 auf 14×8
- Bonustiere wie im Original: Wasserkäfer (50 × Level), Biene (1000 × Level), Kröte (−250 × Level),
  Seerosenblüte (Glück oder Pech)
- Extraleben bei 10000, 30000, 70000 … Punkten, Statistik nach dem Spiel, Bestenliste (HOPPLER.HI)
- Töne über den Tastatur-Controller der DMV (8741, Befehl 06h)
- Animiertes Titelbild: Libelle hebt ab, Dummvogel blinzelt, Münze dreht sich, Kröte schaut aus dem Wasser

**Tasten:** hoch 8 / W / Pfeil · runter 2 / S / Pfeil · links 4 / A / Pfeil · rechts 6 / D / Pfeil ·
P Pause · T Ton an/aus · B Bestenliste (Titel) · ESC Ende
**Start:** `HOPPLER` oder `HOPPLER n` (Start in Level n)
**Braucht:** DMV mit Farbgrafik, 8088-Karte, MS-DOS. HOPPLER.DAT (vorberechnete Seerosenbilder) wird beim
ersten Start erzeugt, falls sie fehlt.

## English

In the original (Perestroika/Toppler, Moscow 1990) a democrat and a bureaucrat competed for money and
resources. Here cartoon characters meet at a pond: the dragonfly hops across shrinking lily pads towards
the coin while the big dumb bird tries to catch it.

- 25 levels, grid grows from 7×4 to 14×8 as in the original
- Bonus animals as in the original: water beetle, bee, toad, water lily flower
- Extra lives, statistics after the game, high-score list
- Sound via the DMV keyboard controller (8741, command 06h)
- Animated title screen

**Keys:** up 8 / W / cursor · down 2 / S / cursor · left 4 / A / cursor · right 6 / D / cursor ·
P pause · T sound on/off · B high scores (title) · ESC quit
**Requires:** colour DMV, 8088 board, MS-DOS.

## Dateien / Files

| Datei / File | Inhalt / Contents |
|---|---|
| `HOPPLER.PAS` | Hauptprogramm, Turbo Pascal 3.01A (Codepage 437, CRLF) / main program |
| `DMVGFX.INC` | Grafik (µPD7220), Text, Ton (8741), Takt – auch für andere Programme / graphics, text, sound, timing – reusable |
| `FONT.INC` | eigene 8×16-Schrift mit Umlauten, erzeugt von `tools/mkfont.py` / own 8×16 font, generated |
| `SPRITES.INC` | Figuren, erzeugt von `tools/mksprites.py` / sprites, generated |
| `HOPPLER.TXT` | Kurzanleitung auf der Diskette / readme on the disk |
| `tools/font_def.py` | Schrift als ASCII-Raster / font as ASCII art |
| `tools/sprites_def.py` | Figuren als ASCII-Raster / sprites as ASCII art |
| `tools/mkfont.py`, `tools/mksprites.py` | erzeugen FONT.INC und SPRITES.INC / generate FONT.INC and SPRITES.INC |
| `tools/preview.py` | Vorschau der Sprites als PNG / sprite preview |
| `tools/mkimg.py` | baut das Diskettenabbild aus einer 360-KB-DMV-Vorlage / builds the disk image from a 360 KB DMV template |

## Bauen / Building

Turbo Pascal 3.01A, `HOPPLER.PAS` als Hauptdatei, Compiler-Option C (COM-Datei). `DMVGFX.INC`, `FONT.INC`
und `SPRITES.INC` müssen im selben Verzeichnis liegen. Seit Version 0.17 wird TurboGraf nicht mehr gebraucht.

Turbo Pascal 3.01A, main file `HOPPLER.PAS`, compiler option C (COM file). `DMVGFX.INC`, `FONT.INC` and
`SPRITES.INC` must be in the same directory. TurboGraf is no longer needed since version 0.17.

## Versionen / Versions

- 0.18 – zusätzlich W/A/S/D, Ton jetzt mit T / W/A/S/D added, sound toggle moved to T
- 0.17 – ohne TurboGraf (eigene Grafikbibliothek und Schrift), animiertes Titelbild / no TurboGraf, animated title
- 0.16 – erste veröffentlichte Fassung / first published version

## Lizenz / License

MIT (siehe [../LICENSE](../LICENSE)).
Code: Claude (Anthropic) · Idee und Prompts / idea and prompts: rfka01
