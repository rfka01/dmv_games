/* W16.H - Tabelle der Win16-Importe (W16TAB.C, erzeugt) */
typedef struct {
    BYTE mod;                /* Index in w16mods */
    WORD ord;                /* Ordinalzahl */
    void (__far *fn)();      /* umgesetzte Funktion */
} W16IMP;
extern const W16IMP w16tab[];
extern const BYTE *w16argb[];   /* Parameterbytes je Ordinal, 255 = unbekannt */
extern const WORD w16nord[];
extern const char *w16mods[];
