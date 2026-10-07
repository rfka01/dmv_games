# DMV Games

Spiele für den NCR Decision Mate V (DMV) mit Farbgrafik (µPD7220) unter MS-DOS.
Games for the NCR Decision Mate V (DMV) with colour graphics (µPD7220) under MS-DOS.

| Spiel / Game | Verzeichnis / Directory | Diskette / Disk image |
|---|---|---|
| **Hoppler** – frei nach Perestroika/Toppler / loosely based on Perestroika/Toppler | [`hoppler/`](hoppler/) | [`images/HOPPLER-V0.18.IMG`](images/HOPPLER-V0.18.IMG) |

[![Hoppler](hoppler/screenshots/titel.png)](hoppler/)

## Aufbau / Layout

- je Spiel ein Verzeichnis mit Quellen und Werkzeugen / one directory per game with sources and tools
- `images/` – 360-KB-Diskettenabbilder (DMV-Format, 40 Spuren, 2 Seiten, 9 Sektoren), Version im Dateinamen / 360 KB disk images, version in the file name

Getestet in MAME (Treiber `dmv`). Tested in MAME (`dmv` driver).

## Lizenz / License

MIT, siehe [LICENSE](LICENSE). Die Spiele brauchen keine fremden Bibliotheken: Grafik, Schrift und Ton
stammen aus der eigenen `DMVGFX.INC` (siehe [`hoppler/`](hoppler/)).
MIT, see [LICENSE](LICENSE). No third-party libraries needed: graphics, font and sound come from our own
`DMVGFX.INC` (see [`hoppler/`](hoppler/)).

Code: Claude (Anthropic) · Idee und Prompts / idea and prompts: rfka01
