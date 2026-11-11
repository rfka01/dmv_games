/* DRV7220.C - DMVBD: Bildschirmtreiber fuer den uPD7220 der NCR DMV
   Aufbau und Aufrufe wie ein Windows-3.0-Anzeigetreiber (DDI): Enable,
   Disable, ColorInfo, RealizeObject, BitBlt, Pixel. Kennt keine Handles
   oder DCs - nur BITMAP (Geraet oder Speicher), physische Pinsel und
   DRAWMODE -, damit daraus spaeter DMV7220.DRV werden kann.

   Geraet: 640x400, Farbe 3 Ebenen (G 0000h, R 4000h, B 8000h), Mono 1.
   Speicher-Bitmaps im Geraeteformat: je Zeile die Ebenen nacheinander,
   Bit 0 links und Bytes von links nach rechts - also genau die Worte des
   7220 (ein Treiber darf sein Bitmapformat frei waehlen; Windows sieht es
   nur ueber GetBitmapBits/SetBitmapBits, die umrechnen muessten).
   Code: Claude, Prompts: rfka01 */

#include <conio.h>
#include <dos.h>
#include <string.h>
#include <stdlib.h>
#include "BD.H"

#define P_STAT 0xA0
#define P_PARM 0xA0
#define P_CMD  0xA1
#define P_FIFO 0xA1
#define P_ZOOM 0xA2

short drv_mono;
short drv_keep;
/* Hardware-Rollen: der Bildspeicher ist ein Ring aus RING Zeilen; Zeile 0
   des Bildschirms liegt in Ringzeile d_org. Zwei GDC-Partitionen zeigen
   den Ring ab d_org (wie DMVDEMO, Bild 13). */
#define RING 408
static WORD d_org;
static void g_areas(void);
static BOOL blt_pat_scr(short dx, short dy, short w, short h, BYTE r, LPPBRUSH br);
static WORD rowaddr(WORD y) { WORD r = y + d_org; if (r >= RING) r -= RING; return r * 40; }                         /* 1: Enable loescht den Schirm nicht */
static BYTE rev[256];
static const WORD planebase[3] = { 0x0000, 0x4000, 0x8000 };

/* ---- 7220 ------------------------------------------------------------ */
static void gwait(void) { while (inp(P_STAT) & 0x0A) ; }
static void gcmd(BYTE c) { gwait(); outp(P_CMD, c); }
static void gpar(BYTE p) { while (inp(P_STAT) & 0x02) ; outp(P_PARM, p); }
static void gpw(WORD w) { gpar((BYTE)w); gpar((BYTE)(w >> 8)); }

static void g_curs(WORD addr) { gcmd(0x49); gpw(addr); gpar(0); }
static void g_mask(WORD m) { gcmd(0x4A); gpw(m); }
static void g_figs(WORD n) { gcmd(0x4C); gpar(0x02); gpw((n - 1) & 0x3FFF); }

/* Wortstrom an den 7220: vor jedem Byte nur FIFO-voll (Bit 1) abfragen */
extern void out_words(const WORD *p, WORD n);
#pragma aux out_words = \
    "    jcxz L9" \
    "L1: in al,0A0h" "test al,2" "jnz L1" "lodsb" "out 0A0h,al" \
    "L2: in al,0A0h" "test al,2" "jnz L2" "lodsb" "out 0A0h,al" \
    "    loop L1" \
    "L9:" \
    parm [si] [cx] modify [ax si cx];

/* n Worte ab addr schreiben (Modus 0..3), Maske m.
   FIGS DC=0: jedes Wort einmal (DC>0 wiederholt das erste Wort) */
static void g_words(WORD addr, WORD m, BYTE mode, const WORD *w, WORD n)
{
    g_curs(addr); g_mask(m); g_figs(1);
    gcmd(0x20 | mode);
    out_words(w, n);
}

/* Flaeche ueber GCHRD (wie DMVGRAF gbox): CURS auf die untere linke Ecke
   mit Punktadresse, Muster PRAM 8..15, FIGS 12h (GC, Richtung 2),
   DC = Zeilen-1 mit GD, D = Breite in Punkten. Zeilen laufen nach oben,
   abwechselnd von links und von rechts. */
static void g_gchr(WORD addr, BYTE dot, BYTE mode, const BYTE *mu, WORD rows, WORD width)
{
    int i;
    gcmd(0x20 | mode);                     /* Schreibmodus, ohne Daten */
    gcmd(0x49); gpw(addr); gpar((BYTE)(dot << 4));
    gcmd(0x78); for (i = 0; i < 8; i++) gpar(mu[i]);
    gcmd(0x4C); gpar(0x12);
    gpar((BYTE)(rows - 1)); gpar((BYTE)((((rows - 1) >> 8) & 0x3F) | 0x40));
    gpar((BYTE)width); gpar((BYTE)((width >> 8) & 0x3F));
    gcmd(0x68);
}

static void g_fill(WORD addr, WORD n, WORD v)
{
    g_curs(addr); g_mask(0xFFFF);
    gcmd(0x4C); gpar(0x02); gpw((n - 1) & 0x3FFF);
    gcmd(0x20); gpw(v);
}

static WORD g_read(WORD addr)
{
    WORD lo, hi; unsigned t;
    g_curs(addr);
    gcmd(0x4C); gpar(0x02); gpar(0x01); gpar(0x00);
    gcmd(0xA0);
    for (t = 0; !(inp(P_STAT) & 1) && --t; ) ;
    lo = inp(P_FIFO);
    for (t = 0; !(inp(P_STAT) & 1) && --t; ) ;
    hi = inp(P_FIFO);
    return lo | (hi << 8);
}

static const BYTE prgraf[16] = { 0,0,0,0x59, 0,0,0,0x59, 0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF };
static const BYTE prtext[16] = { 0,0,0,0x19, 0,0,0,0x19, 0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF };

static void g_zoom(BYTE z)
{
    gcmd(0x46); gwait(); outp(P_ZOOM, z);
    gcmd(0x46); gpar(z);
}

static void g_graf(void)
{
    int i;
    d_org = 0;
    gcmd(0x0C);
    g_zoom(0);
    gcmd(0x4B); gpar(0);
    gcmd(0x70); for (i = 0; i < 16; i++) gpar(prgraf[i]);
    /* immer alle drei Ebenen loeschen: auf einer Farbkarte mit /M blieben
       sonst Rot und Blau stehen; auf Mono sind 4000h/8000h Spiegel */
    if (!drv_keep) for (i = 0; i < 3; i++) g_fill(planebase[i], 16384, 0);
    gcmd(0x6B); gcmd(0x0D);
}

void drv_clear(void)
{
    int i;
    for (i = 0; i < 3; i++) g_fill(planebase[i], 16384, 0);
    if (d_org) { d_org = 0; g_areas(); }
}

static void g_text(void)
{
    int i;
    g_zoom(0);
    gcmd(0x4B); gpar(0x8F);
    gcmd(0x70); for (i = 0; i < 16; i++) gpar(prtext[i]);
    g_fill(0, 2000, 0xFC20);
    gcmd(0x6B); gcmd(0x0D);
    bdos(2, 0x1A, 0);          /* Strg-Z: Konsole loescht und setzt Cursor */
}

/* Partitionen fuer den Ring ab d_org (beim naechsten Bildwechsel) */
static void g_areas(void)
{
    WORD l0 = RING - d_org, l1, a0 = d_org * 40;
    BYTE pr[8];
    if (l0 > 400) l0 = 400;
    /* Partition 1 reicht bis ans Ende jedes Bildes: zeigt der GDC mehr als
       400 Zeilen (in MAME der Fall), kaeme sonst Partition 2 ins Bild - und
       deren PRAM 8..11 ist das Muster der Grafikzeichen, also Abfall */
    l1 = 1023;
    pr[0] = (BYTE)a0; pr[1] = (BYTE)(a0 >> 8); pr[2] = (BYTE)((l0 & 15) << 4); pr[3] = (BYTE)(((l0 >> 4) & 0x3F) | 0x40);
    pr[4] = 0; pr[5] = 0; pr[6] = (BYTE)((l1 & 15) << 4); pr[7] = (BYTE)(((l1 >> 4) & 0x3F) | 0x40);
    gwait();
    while (inp(P_STAT) & 0x20) ;            /* Ende des laufenden ... */
    while (!(inp(P_STAT) & 0x20)) ;         /* ... und Anfang des naechsten VSYNC */
    gcmd(0x70); { int i; for (i = 0; i < 8; i++) gpar(pr[i]); }
}

/* ganzen Schirm um n Zeilen rollen (n > 0: Inhalt nach oben); der frei
   werdende Streifen wird schwarz */
static void drv_rollen(short n)
{
    static const PBRUSH schwarz;
    short a = n < 0 ? -n : n;
    if (a >= 400) { drv_clear(); return; }
    d_org = (WORD)((d_org + RING + n) % RING);
    if (n > 0) blt_pat_scr(0, 400 - a, 640, a, R_BLACKNESS, (LPPBRUSH)&schwarz);
    else       blt_pat_scr(0, 0, 640, a, R_BLACKNESS, (LPPBRUSH)&schwarz);
    g_areas();
}

/* ---- DDI ---------------------------------------------------------------- */
short FAR PASCAL Enable(LPVOID lpDevice, short style, LPSTR lpType, LPSTR lpOut, LPVOID lpData)
{
    int i;
    if (style & 1) {                       /* GDIINFO */
        GDIINFO FAR *g = (GDIINFO FAR *)lpDevice;
        _fmemset(g, 0, sizeof *g);
        g->dpVersion = 0x300; g->dpTechnology = 1;          /* DT_RASDISPLAY */
        g->dpHorzSize = 240; g->dpVertSize = 175;
        g->dpHorzRes = 640; g->dpVertRes = 400;
        g->dpBitsPixel = 1; g->dpPlanes = drv_mono ? 1 : 3;
        g->dpNumBrushes = -1; g->dpNumPens = drv_mono ? 2 : 8; g->dpNumColors = drv_mono ? 2 : 8;
        g->dpDEVICEsize = sizeof(BITMAP);
        g->dpRaster = 0x0001 | 0x0008;                       /* RC_BITBLT, RC_BITMAP64? nein: RC_BANDING 0 */
        g->dpAspectX = 10; g->dpAspectY = 13; g->dpAspectXY = 16;
        g->dpLogPixelsX = 96; g->dpLogPixelsY = 72;
        return sizeof(GDIINFO);
    }
    for (i = 0; i < 256; i++) {
        BYTE b = 0; int k;
        for (k = 0; k < 8; k++) if (i & (1 << k)) b |= 0x80 >> k;
        rev[i] = b;
    }
    {
        LPBITMAP d = (LPBITMAP)lpDevice;
        _fmemset(d, 0, sizeof *d);
        d->bmType = 0x2000; d->bmWidth = 640; d->bmHeight = 400;
        d->bmWidthBytes = 80; d->bmPlanes = drv_mono ? 1 : 3; d->bmBitsPixel = 1;
    }
    g_graf();
    return 1;
}

void FAR PASCAL Disable(LPBITMAP lpDev) { g_text(); }

/* RGB -> physische Farbe (Bit0 G, Bit1 R, Bit2 B; Mono Bit0) */
DWORD FAR PASCAL ColorInfo(LPBITMAP lpDev, DWORD rgb, DWORD FAR *lpPhys)
{
    BYTE r = (BYTE)rgb, g = (BYTE)(rgb >> 8), b = (BYTE)(rgb >> 16), c;
    if (lpPhys) {                          /* RGB -> physisch */
        if (drv_mono) c = (r * 30 + g * 59 + b * 11) >= 12800 ? 1 : 0;
        else c = (g >= 128 ? 1 : 0) | (r >= 128 ? 2 : 0) | (b >= 128 ? 4 : 0);
        *lpPhys = c;
        return drv_mono ? (c ? 0xFFFFFFL : 0) :
               (((c & 2) ? 0xFFL : 0) | ((c & 1) ? 0xFF00L : 0) | ((c & 4) ? 0xFF0000L : 0));
    }
    c = (BYTE)rgb;                         /* physisch -> RGB */
    if (drv_mono) return c ? 0xFFFFFFL : 0;
    return ((c & 2) ? 0xFFL : 0) | ((c & 1) ? 0xFF00L : 0) | ((c & 4) ? 0xFF0000L : 0);
}

/* Windows-Farbe (Index der 16 Standardfarben) -> DMV-Farbe an (x,y) */
static const BYTE bayer2[2][2] = { { 0, 2 }, { 3, 1 } };
BYTE drv_dither(BYTE i, short x, short y)
{
    /* Grundfarbe (Bit0 G, Bit1 R, Bit2 B) und Deckung in Vierteln */
    static const BYTE base[16] = { 0, 2, 1, 3, 4, 6, 5, 7, 7, 2, 1, 3, 4, 6, 5, 7 };
    static const BYTE cov[16]  = { 0, 2, 2, 2, 2, 2, 2, 2, 3, 4, 4, 4, 4, 4, 4, 4 };
    static const BYTE lum[16]  = { 0, 1, 1, 2, 0, 1, 1, 2, 3, 2, 2, 3, 1, 2, 3, 4 };
    BYTE t = bayer2[y & 1][x & 1];
    if (drv_mono) return lum[i & 15] > t ? 1 : 0;
    return cov[i & 15] > t ? base[i & 15] : 0;
}

/* RGB-Farbe am Punkt (x, y): je Grundfarbe 0..4 Viertel (64er-Stufen),
   gegen das 2x2-Bayer-Raster; Mono nach Helligkeit */
BYTE drv_dither_rgb(BYTE r, BYTE g, BYTE b, short x, short y)
{
    BYTE t = bayer2[y & 1][x & 1], c = 0;
    if (drv_mono) {
        WORD l = (r * 30 + g * 59 + b * 11) / 100;
        return (BYTE)(((l + 32) >> 6) > t);
    }
    if (((g + 32) >> 6) > t) c |= 1;
    if (((r + 32) >> 6) > t) c |= 2;
    if (((b + 32) >> 6) > t) c |= 4;
    return c;
}

short FAR PASCAL RealizeObject(LPBITMAP lpDev, short style, LPVOID lpIn, LPVOID lpOut, LPVOID lpXform)
{
    /* lpIn: physische Farbe (DWORD) oder - mit style|0x100 - Windows-Farbindex */
    LPPBRUSH b = (LPPBRUSH)lpOut;
    int p, y, x;
    if (!lpOut) return sizeof(PBRUSH);
    if (style & 0x200) {                   /* RGB (CreateSolidBrush) */
        DWORD rgb = *(DWORD FAR *)lpIn;
        BYTE r = (BYTE)rgb, g = (BYTE)(rgb >> 8), bl = (BYTE)(rgb >> 16);
        for (p = 0; p < 3; p++)
            for (y = 0; y < 8; y++) {
                BYTE v = 0;
                for (x = 0; x < 8; x++) if (drv_dither_rgb(r, g, bl, x, y) & (1 << p)) v |= 0x80 >> x;
                b->pat[p][y] = v;
            }
        b->color = drv_dither_rgb(r, g, bl, 0, 0);
    } else if (style & 0x100) {
        BYTE idx = *(BYTE FAR *)lpIn;
        for (p = 0; p < 3; p++)
            for (y = 0; y < 8; y++) {
                BYTE v = 0;
                for (x = 0; x < 8; x++) if (drv_dither(idx, x, y) & (1 << p)) v |= 0x80 >> x;
                b->pat[p][y] = v;
            }
        b->color = drv_dither(idx, 0, 0);
    } else {
        BYTE c = (BYTE)*(DWORD FAR *)lpIn;
        for (p = 0; p < 3; p++) _fmemset(b->pat[p], (c >> p) & 1 ? 0xFF : 0, 8);
        b->color = c;
    }
    b->style = (BYTE)style;
    return sizeof(PBRUSH);
}

/* ---- Speicher-Bitmaps ------------------------------------------------ */
WORD drv_bitmap_bytes(short w, short h)
{
    WORD wb = ((w + 15) >> 4) << 1;
    return wb * h * (drv_mono ? 1 : 3);
}
void drv_bitmap_init(LPBITMAP bm, short w, short h, BYTE FAR *bits)
{
    _fmemset(bm, 0, sizeof *bm);
    bm->bmWidth = w; bm->bmHeight = h;
    bm->bmWidthBytes = ((w + 15) >> 4) << 1;
    bm->bmPlanes = drv_mono ? 1 : 3; bm->bmBitsPixel = 1;
    bm->bmBits = bits;
    bm->bmWidthPlanes = (DWORD)bm->bmWidthBytes * h;
}
#define ROWP(bm, y, p) ((bm)->bmBits + ((DWORD)(y) * (bm)->bmPlanes + (p)) * (bm)->bmWidthBytes)

/* Bit x einer Speicherzeile (MSB links) */
#define GETBIT(r, x) (((r)[(x) >> 3] >> ((x) & 7)) & 1)   /* Bit 0 = linker Punkt */


/* Quell-ROP -> WDAT-Modus (0 ersetzen, 1 xor, 2 loeschen, 3 setzen)
   und ob die Quelle invertiert geschrieben wird */
static BOOL rop_src_mode(BYTE r, BYTE *mode, BYTE *inv)
{
    switch (r) {
    case R_SRCCOPY:    *mode = 0; *inv = 0; return 1;
    case R_NOTSRCCOPY: *mode = 0; *inv = 1; return 1;
    case R_SRCPAINT:   *mode = 3; *inv = 0; return 1;
    case R_SRCAND:     *mode = 2; *inv = 1; return 1;
    case R_SRCINVERT:  *mode = 1; *inv = 0; return 1;
    case R_MERGEPAINT: *mode = 3; *inv = 1; return 1;
    }
    return 0;
}

extern void __cdecl blit_row(WORD addr, WORD mode, WORD fm, WORD lm, WORD n,
                             WORD qoff, WORD qseg, WORD b, WORD inv);

/* Quell-ROP mit Pinsel: DSPDxax (E20746h, "Pinselfarbe, wo die Quelle 1
   ist") - je Ebene setzen bzw. loeschen mit der Quelle als Maske. Gerasterte
   Pinsel werden dabei je Ebene zur naechsten vollen Farbe. */
static int pat_on(LPPBRUSH br, int p)
{
    int i, n = 0;
    BYTE m;
    if (!br) return 0;
    for (i = 0; i < 8; i++) for (m = br->pat[p][i]; m; m &= m - 1) n++;
    return n >= 32;
}

static BOOL blt_mem_scr(LPBITMAP src, short dx, short dy, short sx, short sy, short w, short h, BYTE r,
                        LPPBRUSH br)
{
    BYTE mode[3], inv;
    WORD n, fm, lm, y, p, np = drv_mono ? 1 : 3, wb, qseg, qoff, step;
    short x0, S, b;
    if (r == 0xE2) {                     /* DSPDxax */
        for (p = 0; p < np; p++) mode[p] = pat_on(br, p) ? 3 : 2;
        inv = 0;
    } else {
        if (!rop_src_mode(r, &mode[0], &inv)) return 0;
        mode[1] = mode[2] = mode[0];
    }
    x0 = dx & ~15;
    n = ((dx + w - 1) >> 4) - (x0 >> 4) + 1;
    if (n > 40) return 0;
    fm = 0xFFFF << (dx & 15);
    lm = 0xFFFF >> (15 - ((dx + w - 1) & 15));
    if (n == 1) fm &= lm;
    S = sx - (dx & 15);                  /* erstes Quellbit von Wort 0, >= -15 */
    b = S & 7;
    wb = src->bmWidthBytes;
    qseg = FP_SEG(src->bmBits);
    qoff = FP_OFF(src->bmBits) + (WORD)sy * src->bmPlanes * wb + (WORD)(S >> 3);
    step = src->bmPlanes == 1 ? 0 : wb;  /* Ebene zu Ebene */
    g_figs(1);
    for (y = 0; y < (WORD)h; y++) {
        WORD addr = rowaddr(dy + y) + (x0 >> 4), q = qoff;
        for (p = 0; p < np; p++, q += step)
            blit_row(planebase[p] + addr, mode[p], fm, lm, n, q, qseg, b, inv ? 0xFFFF : 0);
        qoff += src->bmPlanes * wb;
    }
    return 1;
}

/* Muster/Konstante -> Bildschirm: je Ebene ein GCHRD-Befehl */
static BYTE ror8(BYTE v, BYTE c) { c &= 7; return (BYTE)((v >> c) | (v << (8 - c))); }

static BOOL blt_pat_ring(short dx, short dy, short w, short h, BYTE r, LPPBRUSH br);
static BOOL blt_pat_scr(short dx, short dy, short w, short h, BYTE r, LPPBRUSH br)
{
    short ys = RING - d_org;               /* Bildschirmzeile der Ringzeile 0 */
    if (d_org && dy < ys && dy + h > ys)    /* Flaeche ueber das Ringende: teilen */
        return blt_pat_ring(dx, dy, w, ys - dy, r, br) && blt_pat_ring(dx, ys, w, dy + h - ys, r, br);
    return blt_pat_ring(dx, dy, w, h, r, br);
}

static BOOL blt_pat_ring(short dx, short dy, short w, short h, BYTE r, LPPBRUSH br)
{
    WORD p, np = drv_mono ? 1 : 3, yb = dy + h - 1;
    WORD addr = rowaddr(yb) + (dx >> 4);
    BYTE mu[8], i;
    static const BYTE ff[8] = { 0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF };
    for (p = 0; p < np; p++) {
        BYTE mode, all = 0xFF, any = 0;
        const BYTE *m = ff;
        if (r == R_PATCOPY || r == R_PATINVERT) {
            for (i = 0; i < 8; i++) {          /* Zeile i (0 = unten) aus PRAM 15-i */
                BYTE v = rev[br->pat[p][(yb - i) & 7]];
                all &= v; any |= v;
                mu[7 - i] = ror8(v, (BYTE)((i & 1) ? dx + w : dx));
            }
            m = mu;
        }
        switch (r) {
        case R_PATCOPY:
            if (!any) { mode = 2; m = ff; } else if (all == 0xFF) { mode = 3; m = ff; } else mode = 0;
            break;
        case R_PATINVERT:
            if (!any) continue;
            mode = 1; break;
        case R_DSTINVERT: mode = 1; break;
        case R_BLACKNESS: mode = 2; break;
        case R_WHITENESS: mode = 3; break;
        default: return 0;
        }
        g_gchr(planebase[p] + addr, (BYTE)(dx & 15), mode, m, h, w);
    }
    return 1;
}

/* Muster-ROPs im Speicher: byteweise (Bit 0 = linker Punkt) */
static BOOL pat_mem(LPBITMAP d, short dx, short dy, short w, short h, BYTE r, LPPBRUSH br)
{
    short y, p, x0 = dx >> 3, x1 = (dx + w - 1) >> 3, i;
    BYTE fm = (BYTE)(0xFF << (dx & 7)), lm = (BYTE)(0xFF >> (7 - ((dx + w - 1) & 7)));
    if (x0 == x1) fm &= lm;
    for (y = dy; y < dy + h; y++)
        for (p = 0; p < d->bmPlanes; p++) {
            BYTE FAR *row = ROWP(d, y, p);
            BYTE pv = br ? rev[br->pat[d->bmPlanes == 1 ? 0 : p][y & 7]] : 0;
            for (i = x0; i <= x1; i++) {
                BYTE m = i == x0 ? fm : i == x1 ? lm : 0xFF, v = row[i];
                switch (r) {
                case R_PATCOPY:   v = (BYTE)((v & ~m) | (pv & m)); break;
                case R_PATINVERT: v ^= pv & m; break;
                case R_DSTINVERT: v ^= m; break;
                case R_BLACKNESS: v &= ~m; break;
                case R_WHITENESS: v |= m; break;
                }
                row[i] = v;
            }
        }
    return 1;
}

/* allgemeine ROP3 im Speicher (Bit fuer Bit ueber die Wahrheitstafel) */
static BOOL blt_mem_mem(LPBITMAP d, short dx, short dy, LPBITMAP s, short sx, short sy,
                        short w, short h, BYTE r, LPPBRUSH br)
{
    if (!s && (r == R_PATCOPY || r == R_PATINVERT || r == R_DSTINVERT || r == R_BLACKNESS || r == R_WHITENESS))
        return pat_mem(d, dx, dy, w, h, r, br);
    {
    short y, x, p;
    for (y = 0; y < h; y++)
        for (p = 0; p < d->bmPlanes; p++) {
            BYTE FAR *dr = ROWP(d, dy + y, p);
            BYTE FAR *sr = s ? ROWP(s, sy + y, s->bmPlanes == 1 ? 0 : p) : 0;
            BYTE pb = br ? br->pat[p][(dy + y) & 7] : 0;
            for (x = 0; x < w; x++) {
                short X = dx + x;
                BYTE D = GETBIT(dr, X), S = sr ? GETBIT(sr, sx + x) : 0, P = (pb >> (7 - (X & 7))) & 1;
                BYTE v = (r >> ((P << 2) | (S << 1) | D)) & 1;
                if (v) dr[X >> 3] |= 1 << (X & 7); else dr[X >> 3] &= ~(1 << (X & 7));
            }
        }
    }
    return 1;
}

BOOL FAR PASCAL BitBlt(LPBITMAP lpDst, short dx, short dy, LPBITMAP lpSrc, short sx, short sy,
                       short w, short h, DWORD rop, LPPBRUSH lpBrush, LPDRAWMODE lpDM)
{
    BYTE r = (BYTE)(rop >> 16);
    if (w <= 0 || h <= 0) return 1;
    if (lpDst->bmType) {                  /* Bildschirm */
        if (lpSrc && !lpSrc->bmType) return blt_mem_scr(lpSrc, dx, dy, sx, sy, w, h, r, lpBrush);
        if (!lpSrc) return blt_pat_scr(dx, dy, w, h, r, lpBrush);
        /* Bildschirm -> Bildschirm: ganzer Schirm senkrecht verschoben
           (ScrollDC) ueber das Hardware-Rollen; sonst noch nicht */
        if (lpSrc->bmType && r == R_SRCCOPY && dx == 0 && sx == 0 && w == 640 && h == 400 - abs(sy - dy)
            && (dy == 0 || sy == 0)) { drv_rollen(sy - dy); return 1; }
        return 0;
    }
    if (lpSrc && lpSrc->bmType) return 0; /* Bildschirm -> Speicher: spaeter (RDAT) */
    return blt_mem_mem(lpDst, dx, dy, lpSrc, sx, sy, w, h, r, lpBrush);
}

DWORD FAR PASCAL Pixel(LPBITMAP lpDst, short x, short y, DWORD phys, LPDRAWMODE lpDM)
{
    WORD p, np = drv_mono ? 1 : 3, addr = rowaddr(y) + (x >> 4);
    if (!lpDst->bmType) return 0;
    for (p = 0; p < np; p++) {
        WORD v = ((BYTE)phys >> p) & 1 ? 0xFFFF : 0;
        g_words(planebase[p] + addr, 1 << (x & 15), 0, &v, 1);
    }
    return phys;
}

/* ---- Linien (Output OS_POLYLINE) ------------------------------------------
   Bildschirm: Linienbefehl des 7220 (FIGS Linie, FIGD) je Ebene, wie
   DMVGRAF gline; der letzte Punkt wird wie bei Windows nicht gezeichnet.
   Zeichenart nach ROP2 und Stiftfarbe je Ebene: setzen, loeschen oder
   umkehren. Speicher-Bitmaps: Bresenham in C. */

/* Modus fuer Ebene p: -1 nichts, sonst WDAT-Modus 1 xor, 2 loeschen, 3 setzen */
static int rop2_mode(short rop2, BYTE col, int p)
{
    int on = (col >> p) & 1;
    switch (rop2) {
    case 1:  return 2;                       /* R2_BLACK */
    case 16: return 3;                       /* R2_WHITE */
    case 6:  return 1;                       /* R2_NOT */
    case 7:  return on ? 1 : -1;             /* R2_XORPEN */
    case 10: return on ? -1 : 1;             /* R2_NOTXORPEN */
    case 15: return on ? 3 : -1;             /* R2_MERGEPEN */
    case 9:  return on ? -1 : 2;             /* R2_MASKPEN */
    case 11: return -1;                      /* R2_NOP */
    case 4:  return on ? 2 : 3;              /* R2_NOTCOPYPEN */
    default: return on ? 3 : 2;              /* R2_COPYPEN */
    }
}

/* Linie (x0,y0)-(x1,y1) auf dem Schirm, beide im Bild; ohne Endpunkt,
   wenn last = 0 */
static void scr_line(short x0, short y0, short x1, short y1, BYTE col, short rop2, int last)
{
    short dx = x1 - x0, dy = y1 - y0, ax = abs(dx), ay = abs(dy), dir, maj, mn, n;
    WORD p, np = drv_mono ? 1 : 3;
    if (dy >= 0) dir = dx >= 0 ? (ay >= ax ? 0 : 1) : (ay >= ax ? 7 : 6);
    else         dir = dx > 0 ? (ax >= ay ? 2 : 3) : (ay >= ax ? 4 : 5);
    if (dir == 0 || dir == 3 || dir == 4 || dir == 7) { maj = ay; mn = ax; } else { maj = ax; mn = ay; }
    n = last ? maj : maj - 1;               /* DC: Punkte - 1 */
    if (n < 0) return;
    for (p = 0; p < np; p++) {
        int m = rop2_mode(rop2, col, p);
        if (m < 0) continue;
        gcmd((BYTE)(0x20 | m));
        gcmd(0x49); gpw(planebase[p] + rowaddr(y0) + (x0 >> 4)); gpar((BYTE)((x0 & 15) << 4));
        gcmd(0x78); gpar(0xFF); gpar(0xFF);
        gcmd(0x4C); gpar((BYTE)(0x08 | dir));
        gpar((BYTE)n); gpar((BYTE)(((n >> 8) & 0x3F) | 0x40));
        gpw((2 * mn - maj) & 0x3FFF); gpw((2 * (mn - maj)) & 0x3FFF); gpw((2 * mn) & 0x3FFF);
        gcmd(0x6C);
    }
}

/* Linie auf dem Schirm mit Rollring: ueber das Ringende teilen */
static void scr_line_r(short x0, short y0, short x1, short y1, BYTE col, short rop2, int last)
{
    short ys = RING - d_org;
    if (d_org && ((y0 < ys) != (y1 < ys))) {
        /* Schnitt mit der Zeile ys - 1 / ys */
        short ya = y0 < y1 ? ys - 1 : ys, yb = y0 < y1 ? ys : ys - 1;
        short xa = x0 + (short)((long)(x1 - x0) * (ya - y0) / (y1 - y0));
        short xb = x0 + (short)((long)(x1 - x0) * (yb - y0) / (y1 - y0));
        scr_line(x0, y0, xa, ya, col, rop2, 1);
        scr_line(xb, yb, x1, y1, col, rop2, last);
        return;
    }
    scr_line(x0, y0, x1, y1, col, rop2, last);
}

static void mem_line(LPBITMAP d, short x0, short y0, short x1, short y1, BYTE col, short rop2)
{
    short dx = abs(x1 - x0), sx = x0 < x1 ? 1 : -1, dy = -abs(y1 - y0), sy = y0 < y1 ? 1 : -1, err = dx + dy, p;
    while (x0 != x1 || y0 != y1) {
        if (x0 >= 0 && y0 >= 0 && x0 < d->bmWidth && y0 < d->bmHeight)
            for (p = 0; p < d->bmPlanes; p++) {
                int m = rop2_mode(rop2, col, p);
                BYTE FAR *b = ROWP(d, y0, p) + (x0 >> 3), bit = (BYTE)(1 << (x0 & 7));
                if (m == 1) *b ^= bit; else if (m == 2) *b &= ~bit; else if (m == 3) *b |= bit;
            }
        {
            short e2 = 2 * err;                  /* beide Schritte vom selben Fehler */
            if (e2 >= dy) { err += dy; x0 += sx; }
            if (e2 <= dx) { err += dx; y0 += sy; }
        }
    }
}

/* Cohen-Sutherland auf [0,W) x [0,H); 0 = ganz draussen */
static int oc(short x, short y, short W, short H)
{ return (x < 0) | (x >= W) << 1 | (y < 0) << 2 | (y >= H) << 3; }
static int clip_line(short *x0, short *y0, short *x1, short *y1, short W, short H, int *end_in)
{
    long X0 = *x0, Y0 = *y0, X1 = *x1, Y1 = *y1;
    int c0 = oc(*x0, *y0, W, H), c1 = oc(*x1, *y1, W, H), k;
    *end_in = !c1;
    for (k = 0; k < 8 && (c0 | c1); k++) {
        int c = c0 ? c0 : c1; long x, y;
        if (c0 & c1) return 0;
        if (c & 8)      { y = H - 1; x = X0 + (X1 - X0) * (y - Y0) / (Y1 - Y0); }
        else if (c & 4) { y = 0;     x = X0 + (X1 - X0) * (y - Y0) / (Y1 - Y0); }
        else if (c & 2) { x = W - 1; y = Y0 + (Y1 - Y0) * (x - X0) / (X1 - X0); }
        else            { x = 0;     y = Y0 + (Y1 - Y0) * (x - X0) / (X1 - X0); }
        if (c == c0) { X0 = x; Y0 = y; c0 = oc((short)x, (short)y, W, H); }
        else         { X1 = x; Y1 = y; c1 = oc((short)x, (short)y, W, H); }
    }
    if (c0 | c1) return 0;
    *x0 = (short)X0; *y0 = (short)Y0; *x1 = (short)X1; *y1 = (short)Y1;
    return 1;
}

static void polylinie(LPBITMAP d, short n, LPPOINT pts, LPPPEN pen, short rop2)
{
    short i;
    for (i = 0; i + 1 < n; i++) {
        short x0 = pts[i].x, y0 = pts[i].y, x1 = pts[i + 1].x, y1 = pts[i + 1].y;
        int ein;
        if (!d->bmType) { mem_line(d, x0, y0, x1, y1, pen->color, rop2); continue; }
        if (!clip_line(&x0, &y0, &x1, &y1, d->bmWidth, d->bmHeight, &ein)) continue;
        scr_line_r(x0, y0, x1, y1, pen->color, rop2, !ein);   /* Ende abgeschnitten: Randpunkt mit */
    }
}

/* Flaeche mit dem Pinsel nach ROP2 */
static BYTE rop2_rop3(short rop2)
{
    switch (rop2) {
    case 1: return R_BLACKNESS; case 16: return R_WHITENESS;
    case 6: return R_DSTINVERT; case 7: return R_PATINVERT;
    default: return R_PATCOPY;
    }
}
static void span(LPBITMAP d, short x0, short x1, short y, short h, LPPBRUSH br, short rop2)
{
    if (x0 < 0) x0 = 0;
    if (x1 > d->bmWidth) x1 = d->bmWidth;
    if (y < 0) { h += y; y = 0; }
    if (y + h > d->bmHeight) h = d->bmHeight - y;
    if (x1 <= x0 || h <= 0) return;
    if (d->bmType) blt_pat_scr(x0, y, x1 - x0, h, rop2_rop3(rop2), br);
    else blt_mem_mem(d, x0, y, 0, 0, 0, x1 - x0, h, rop2_rop3(rop2), br);
}

static long isqrt(long v)
{
    long r = 0, b = 1L << 30;
    while (b > v) b >>= 2;
    while (b) { if (v >= r + b) { v -= r + b; r = (r >> 1) + b; } else r >>= 1; b >>= 2; }
    return r;
}

/* Ellipse im Rechteck (l,t)-(r,b), r und b ausgeschlossen: je Zeile die
   Spanne; gleiche Spannen hintereinander werden ein Rechteck. Der Rand
   ist der Zug durch die Spannenenden. */
#define MAXRAND 802
static POINT rand_[MAXRAND];
static void ellipse(LPBITMAP d, short l, short t, short r, short b, LPPPEN pen, LPPBRUSH br, short rop2)
{
    long A = r - l - 1, B = b - t - 1, c2 = l + r - 1, c2y = t + b - 1;
    short y, n = 0, fy = 0, fx0 = 0, fx1 = 0, fh = 0, hohl = !br || br->style == 1;
    short nl = 0;
    if (A < 0 || B < 0) return;
    for (y = t; y < b; y++) {
        long D = 2L * y - c2y, w2, q;
        short x0, x1;
        if (B == 0) w2 = A;
        else { q = (B * B - D * D) * 4096L / (B * B); if (q < 0) q = 0; w2 = A * isqrt(q * 4096L) / 4096L; }
        x0 = (short)((c2 - w2 + 1) >> 1); x1 = (short)((c2 + w2) >> 1);
        if (!hohl) {
            if (fh && x0 == fx0 && x1 == fx1) fh++;
            else { if (fh) span(d, fx0, fx1 + 1, fy, fh, br, rop2); fy = y; fx0 = x0; fx1 = x1; fh = 1; }
        }
        if (nl < MAXRAND / 2) { rand_[nl].x = x0; rand_[nl].y = y; rand_[MAXRAND - 1 - nl].x = x1; rand_[MAXRAND - 1 - nl].y = y; nl++; }
    }
    if (fh) span(d, fx0, fx1 + 1, fy, fh, br, rop2);
    if (pen && pen->style != 5 && nl) {
        /* links von oben nach unten, rechts von unten nach oben, zu */
        POINT FAR *p = rand_;
        _fmemmove(&rand_[nl], &rand_[MAXRAND - nl], nl * sizeof(POINT));
        n = 2 * nl;
        rand_[n] = rand_[0];
        polylinie(d, n + 1, (LPPOINT)p, pen, rop2);
        (void)fx1;
    }
}

short FAR PASCAL Output(LPBITMAP d, short style, short n, LPPOINT pts, LPPPEN pen, LPPBRUSH br,
                        LPDRAWMODE dm, LPRECT clip)
{
    short rop2 = dm ? dm->Rop2 : 13;
    if (style == OS_ELLIPSE && n >= 2) {
        ellipse(d, pts[0].x, pts[0].y, pts[1].x, pts[1].y, pen, br, rop2);
        return 1;
    }
    if (style == OS_RECTANGLE && n >= 2) {
        short l = pts[0].x, t = pts[0].y, r = pts[1].x, b = pts[1].y;
        if (br && br->style != 1) span(d, l, r, t, b - t, br, rop2);
        if (pen && pen->style != 5 && r > l && b > t) {
            POINT q[5];
            q[0].x = l; q[0].y = t; q[1].x = r - 1; q[1].y = t; q[2].x = r - 1; q[2].y = b - 1;
            q[3].x = l; q[3].y = b - 1; q[4] = q[0];
            polylinie(d, 5, (LPPOINT)q, pen, rop2);
        }
        return 1;
    }
    if (style != OS_POLYLINE || !pen || pen->style == 5) return 0;
    polylinie(d, n, pts, pen, rop2);
    return 1;
}
