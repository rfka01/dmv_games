/* MJ.H - gemeinsame Vereinbarungen fuer DMV-Mahjongg.
   K&R-C fuer DRs C-Compiler (CP/M-8000, CP/M-68K) und fuer DOS.
   Code: Claude (Anthropic), Idee und Prompts: rfka01. Lizenz: MIT */

#ifdef HOST
#define UW unsigned short
#else
#ifdef __GNUC__                 /* gcc fuer CP/M-68K: int hat 32 Bit */
#define UW unsigned short
#else
#define UW unsigned
#endif
#endif

#define M16(x) ((x) & 0xFFFF)

#define HW_FARBE 1      /* Farbbildschirm */
#define HW_MAUS  2      /* K806 vorhanden */

/* Tasten, wie die Konsole sie liefert (ADM-3A-Codes) */
#define K_LINKS  0x08
#define K_RUNTER 0x0A
#define K_HOCH   0x0B
#define K_RECHTS 0x0C
#define K_ESC    0x1B
#define K_CR     0x0D
