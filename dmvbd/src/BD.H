/* BD.H - DMVBD "Before Dawn": gemeinsame Typen
   Abspieler fuer After-Dark-kompatible Bildschirmschoner-Module
   (Windows 3.0 Real Mode) auf der NCR DMV.
   Code: Claude, Prompts: rfka01 */

#ifndef BD_H
#define BD_H

typedef unsigned char  BYTE;
typedef unsigned short WORD;
typedef unsigned long  DWORD;
typedef short          BOOL;
typedef WORD           HANDLE;
typedef WORD           HDC;
typedef DWORD          COLORREF;

#define FAR     __far
#define PASCAL  __pascal
#define API     __far __pascal __loadds  /* von Modulen gerufen */

typedef struct { short x, y; } POINT;
typedef struct { short left, top, right, bottom; } RECT;
typedef RECT FAR *LPRECT;
typedef char FAR *LPSTR;
typedef void FAR *LPVOID;

/* ---- Windows-3.0-DDI-Strukturen (Treiberschicht) ------------------ */

/* Geraet bzw. Speicher-Bitmap; bmType 0 = Speicher, sonst Geraet */
typedef struct {
    short bmType;
    short bmWidth, bmHeight;
    short bmWidthBytes;      /* Bytes je Zeile und Ebene */
    BYTE  bmPlanes;          /* 3 Farbe (G,R,B), 1 Mono */
    BYTE  bmBitsPixel;       /* 1 */
    BYTE FAR *bmBits;        /* Ebenen je Zeile nacheinander, MSB links */
    DWORD bmWidthPlanes;     /* Bytes einer Ebene (bmWidthBytes*Hoehe) */
    LPVOID bmlpPDevice;
    short bmSegmentIndex, bmScanSegment, bmFillBytes;
    short futureUse4, futureUse5;
} BITMAP;
typedef BITMAP FAR *LPBITMAP;

/* physischer Pinsel/Stift: Muster 8x8 je Ebene */
typedef struct {
    BYTE  pat[3][8];
    BYTE  color;             /* physische Farbe: Bit0 G, Bit1 R, Bit2 B */
    BYTE  style;
} PBRUSH;
typedef PBRUSH FAR *LPPBRUSH;

typedef struct {
    BYTE  color;             /* physische Farbe: Bit0 G, Bit1 R, Bit2 B */
    BYTE  style;             /* PS_SOLID 0 ... PS_NULL 5 */
} PPEN;
typedef PPEN FAR *LPPPEN;
typedef POINT FAR *LPPOINT;
#define OS_POLYLINE 18
#define OS_RECTANGLE 6
#define OS_ELLIPSE 7

typedef struct {
    short    Rop2;
    short    bkMode;
    DWORD    bkColor;        /* physisch */
    DWORD    TextColor;      /* physisch */
} DRAWMODE;
typedef DRAWMODE FAR *LPDRAWMODE;

typedef struct {
    short dpVersion, dpTechnology, dpHorzSize, dpVertSize;
    short dpHorzRes, dpVertRes, dpBitsPixel, dpPlanes;
    short dpNumBrushes, dpNumPens, futureuse, dpNumFonts, dpNumColors;
    short dpDEVICEsize;
    WORD  dpCurves, dpLines, dpPolygonals, dpText, dpClip, dpRaster;
    short dpAspectX, dpAspectY, dpAspectXY;
    short dpStyleLen;
    POINT dpMLoWin, dpMLoVpt, dpMHiWin, dpMHiVpt;
    POINT dpELoWin, dpELoVpt, dpEHiWin, dpEHiVpt;
    POINT dpTwpWin, dpTwpVpt;
    short dpLogPixelsX, dpLogPixelsY;
    short dpDCManage;
    short futureuse3[5];
    short dpPalColors, dpPalReserved, dpPalResolution;
} GDIINFO;

/* ROP3-Codes (hohes Wort) */
#define R_SRCCOPY    0xCC
#define R_SRCPAINT   0xEE
#define R_SRCAND     0x88
#define R_SRCINVERT  0x66
#define R_SRCERASE   0x44
#define R_NOTSRCCOPY 0x33
#define R_NOTSRCERASE 0x11
#define R_MERGEPAINT 0xBB
#define R_DSTINVERT  0x55
#define R_BLACKNESS  0x00
#define R_WHITENESS  0xFF
#define R_PATCOPY    0xF0
#define R_PATINVERT  0x5A
#define R_MERGECOPY  0xC0

/* ---- Treiber (DRV7220.C), Aufrufe wie DDI ------------------------ */
short FAR PASCAL Enable(LPVOID lpDevice, short style, LPSTR lpType, LPSTR lpOut, LPVOID lpData);
void  FAR PASCAL Disable(LPBITMAP lpDev);
DWORD FAR PASCAL ColorInfo(LPBITMAP lpDev, DWORD rgb, DWORD FAR *lpPhys);
short FAR PASCAL RealizeObject(LPBITMAP lpDev, short style, LPVOID lpIn, LPVOID lpOut, LPVOID lpXform);
BOOL  FAR PASCAL BitBlt(LPBITMAP lpDst, short dx, short dy, LPBITMAP lpSrc, short sx, short sy,
                        short w, short h, DWORD rop, LPPBRUSH lpBrush, LPDRAWMODE lpDM);
short FAR PASCAL Output(LPBITMAP d, short style, short n, LPPOINT pts, LPPPEN pen, LPPBRUSH br,
                        LPDRAWMODE dm, LPRECT clip);
DWORD FAR PASCAL Pixel(LPBITMAP lpDst, short x, short y, DWORD phys, LPDRAWMODE lpDM);

#define OBJ_PEN   1
#define OBJ_BRUSH 2

/* Treiber-Hilfen fuer DMVBD */
extern short drv_mono;            /* 1 = Mono-Karte */
void drv_bitmap_init(LPBITMAP bm, short w, short h, BYTE FAR *bits);
WORD drv_bitmap_bytes(short w, short h);
extern short drv_keep;
BYTE drv_dither_rgb(BYTE r, BYTE g, BYTE b, short x, short y);
void drv_clear(void);
BYTE drv_dither(BYTE idx16, short x, short y);   /* Windows-Farbe -> DMV-Farbe */

/* ---- Lader (NELOAD.C) --------------------------------------------- */
typedef struct {
    WORD  nseg;
    WORD  seg[16];          /* Paragraph je NE-Segment */
    WORD  dgroup;
    WORD  autodata;
    WORD  heap;
    DWORD csip;
    void (FAR *module)(void);
    WORD  hinst;
    BYTE FAR *file;          /* ganze Datei im Speicher */
    DWORD flen;
    WORD  rsrc;              /* Ressourcentabelle (Versatz) */
} NEMOD;
int  ne_load(NEMOD *m, const char *path);
void ne_free(NEMOD *m);
void w16_reset(void);
extern int ne_still;
BYTE FAR *ne_resource(NEMOD *m, WORD type, LPSTR name, WORD *len);
extern NEMOD g_mod;

/* ---- Mini-Win16 (W16.C) ------------------------------------------- */
void w16_init(void);
HDC  w16_screen_dc(void);
HANDLE w16_galloc(WORD bytes, BYTE FAR **p);
extern DWORD g_tick, g_tick0;
extern int   g_unimpl;

#endif
