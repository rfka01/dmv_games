/* W16.C - DMVBD: kleines Win16 (KERNEL/USER/GDI-Teilmenge) fuer
   After-Dark-Module. Verwaltet Handles, DCs und Objekte wie das echte
   GDI und ruft fuer alles Zeichnen den Treiber DRV7220.C (DDI) auf.
   Alle Funktionen sind FAR PASCAL mit __loadds, weil die Module sie mit
   ihrem eigenen DS rufen.
   Code: Claude, Prompts: rfka01 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dos.h>
#include "BD.H"

DWORD g_tick;
int   g_unimpl;

/* ---- Handles -------------------------------------------------------- */
enum { H_FREE, H_DC, H_BITMAP, H_BRUSH, H_PEN, H_GLOBAL, H_FONT, H_PAL };
typedef struct {
    BYTE  type;
    BYTE  stock;
    WORD  seg;               /* Speicher (Global, Bitmap-Bits) */
    BITMAP bm;               /* H_BITMAP / Geraet */
    PBRUSH pb;               /* H_BRUSH / H_PEN */
    /* DC */
    BYTE  screen;
    HANDLE hbm, hbr, hpen;
    DWORD bkcolor, txcolor;
    short bkmode, rop2;
    short cx, cy;            /* aktuelle Stelle (MoveTo/LineTo) */
    PPEN  pp;                /* H_PEN */
} OBJ;
#define MAXH 96
static OBJ obj[MAXH];
static BITMAP g_dev;           /* PDEVICE des Bildschirms */
static HDC    g_hscreen;
static HANDLE stock[20];

#define H2I(h) ((h) >> 1)
#define I2H(i) ((i) << 1)

static HANDLE h_new(BYTE type)
{
    int i;
    for (i = 1; i < MAXH; i++)
        if (obj[i].type == H_FREE) { memset(&obj[i], 0, sizeof obj[i]); obj[i].type = type; return I2H(i); }
    return 0;
}
static OBJ *h_get(HANDLE h, BYTE type)
{
    WORD i = H2I(h);
    if (!h || i >= MAXH || obj[i].type != type) return 0;
    return &obj[i];
}

/* ---- Standard-Farben ------------------------------------------------- */
static const BYTE pal16[16][3] = {
    {0,0,0},{128,0,0},{0,128,0},{128,128,0},{0,0,128},{128,0,128},{0,128,128},{128,128,128},
    {192,192,192},{255,0,0},{0,255,0},{255,255,0},{0,0,255},{255,0,255},{0,255,255},{255,255,255} };
static BYTE nearest16(BYTE r, BYTE g, BYTE b)
{
    int i, best = 0; long bd = 0x7FFFFFFFL;
    for (i = 0; i < 16; i++) {
        long dr = (long)r - pal16[i][0], dg = (long)g - pal16[i][1], db = (long)b - pal16[i][2];
        long d = dr * dr + dg * dg + db * db;
        if (d < bd) { bd = d; best = i; }
    }
    return (BYTE)best;
}

static HANDLE mk_brush(BYTE idx16)
{
    HANDLE h = h_new(H_BRUSH);
    OBJ *o = &obj[H2I(h)];
    RealizeObject(&g_dev, OBJ_BRUSH | 0x100, (LPVOID)&idx16, &o->pb, 0);
    return h;
}

/* ---- Speicher -------------------------------------------------------- */
HANDLE w16_galloc(WORD bytes, BYTE FAR **p)
{
    WORD seg; HANDLE h;
    if (_dos_allocmem((bytes + 15) >> 4, (unsigned *)&seg)) return 0;
    h = h_new(H_GLOBAL);
    obj[H2I(h)].seg = seg;
    _fmemset(MK_FP(seg, 0), 0, bytes);
    if (p) *p = (BYTE FAR *)MK_FP(seg, 0);
    return h;
}

static BOOL rect_clip(short *x, short *y, short *w, short *h, short *sx, short *sy, short W, short H)
{
    if (*x < 0) { *w += *x; if (sx) *sx -= *x; *x = 0; }
    if (*y < 0) { *h += *y; if (sy) *sy -= *y; *y = 0; }
    if (*x + *w > W) *w = W - *x;
    if (*y + *h > H) *h = H - *y;
    return *w > 0 && *h > 0;
}

static LPBITMAP dc_surface(OBJ *dc)
{
    OBJ *b;
    if (dc->screen) return &g_dev;
    b = h_get(dc->hbm, H_BITMAP);
    return b ? &b->bm : 0;
}

/* ---- Einrichtung ------------------------------------------------------ */
void w16_init(void)
{
    static GDIINFO gi;
    int i;
    Enable(&gi, 1, 0, 0, 0);
    Enable(&g_dev, 0, 0, 0, 0);
    g_hscreen = h_new(H_DC);
    obj[H2I(g_hscreen)].screen = 1;
    obj[H2I(g_hscreen)].txcolor = 0; obj[H2I(g_hscreen)].bkcolor = 7;
    /* Stockobjekte: WHITE, LTGRAY, GRAY, DKGRAY, BLACK, NULL-Pinsel */
    stock[0] = mk_brush(15); stock[1] = mk_brush(8); stock[2] = mk_brush(7);
    stock[3] = mk_brush(7);  stock[4] = mk_brush(0); stock[5] = mk_brush(0);
    obj[H2I(stock[5])].pb.style = 1;                        /* NULL_BRUSH: hohl */
    for (i = 6; i < 9; i++) stock[i] = h_new(H_PEN);
    obj[H2I(stock[6])].pp.color = drv_mono ? 1 : 7;          /* WHITE_PEN */
    obj[H2I(stock[7])].pp.color = 0;                         /* BLACK_PEN */
    obj[H2I(stock[8])].pp.style = 5;                         /* NULL_PEN */
    for (i = 0; i < 9; i++) obj[H2I(stock[i])].stock = 1;
    obj[H2I(g_hscreen)].hbr = stock[0];
    obj[H2I(g_hscreen)].hpen = stock[7];
    obj[H2I(g_hscreen)].rop2 = 13;
}
HDC w16_screen_dc(void) { return g_hscreen; }
void w16_done(void) { Disable(&g_dev); }


/* ---- nicht umgesetzte Importe ---------------------------------------- */
extern const char *w16_stubname(WORD i, WORD *ord);
static WORD unb_seen[64]; static WORD unb_n;
DWORD __far __loadds w16_unbek(WORD nr)
{
    WORD i;
    for (i = 0; i < unb_n; i++) if (unb_seen[i] == nr) return 0;
    if (unb_n < 64) unb_seen[unb_n++] = nr;
    g_unimpl++;
    return 0;
}
void w16_report(void)
{
    WORD i, ord;
    for (i = 0; i < unb_n; i++) {
        const char *m = w16_stubname(unb_seen[i], &ord);
        printf("DMVBD: nicht umgesetzt: %s.%u\n", m, ord);
    }
}

/* ================= KERNEL ================= */
void API w16_FatalExit(short code) { w16_done(); printf("DMVBD: FatalExit %d\n", code); exit(1); }
short API w16_LocalInit(WORD seg, WORD start, WORD end) { return 1; }
DWORD API w16_GetVersion(void) { return 0x0003; }     /* Windows 3.0, DOS-Teil 0 */

static WORD lheap;   /* naechster freier Versatz im Datensegment des Moduls */
void w16_localbase(WORD off) { lheap = (off + 15) & ~15; }
HANDLE API w16_LocalAlloc(WORD flags, WORD size)
{
    WORD r = lheap;
    if ((DWORD)lheap + size + 3 > 0xFFF0L) return 0;    /* DGROUP voll */
    lheap += (size + 3) & ~1;
    if (flags & 0x40) _fmemset(MK_FP(g_mod.dgroup, r), 0, size);
    return r;
}
HANDLE API w16_LocalFree(HANDLE h) { return 0; }
WORD API w16_LocalLock(HANDLE h) { return h; }
BOOL API w16_LocalUnlock(HANDLE h) { return 0; }

HANDLE API w16_GlobalAlloc(WORD flags, DWORD size)
{
    if (size > 0xFFF0L) return 0;
    return w16_galloc((WORD)size, 0);
}
HANDLE API w16_GlobalFree(HANDLE h)
{
    OBJ *o = h_get(h, H_GLOBAL);
    if (!o) return h;
    _dos_freemem(o->seg); o->type = H_FREE; return 0;
}
DWORD API w16_GlobalLock(HANDLE h)
{
    OBJ *o = h_get(h, H_GLOBAL);
    return o ? ((DWORD)o->seg << 16) : 0;
}
BOOL API w16_GlobalUnlock(HANDLE h) { return 0; }
void API w16_OutputDebugString(LPSTR s) { }
DWORD API w16_GetDOSEnvironment(void)
{
    extern unsigned _psp;
    return (DWORD)(*(WORD FAR *)MK_FP(_psp, 0x2C)) << 16;
}
void API w16_FatalAppExit(WORD a, LPSTR s)
{
    w16_done(); printf("DMVBD: FatalAppExit: %Fs\n", s); exit(1);
}

/* ================= USER ================= */
/* Uhr bei jedem Aufruf lesen: manche Module warten in DRAWFRAME darauf,
   dass sie weiterlaeuft (DOS-Uhr, 1/100 s, ab Mitternacht in ms) */
DWORD g_tick0;
DWORD w16_jetzt(void)
{
    union REGS r;
    r.h.ah = 0x2C; intdos(&r, &r);
    return ((DWORD)r.h.ch * 3600L + r.h.cl * 60L + r.h.dh) * 1000L + r.h.dl * 10L;
}
DWORD API w16_GetCurrentTime(void) { return w16_jetzt() - g_tick0; }
DWORD API w16_GetTickCount(void) { return w16_jetzt() - g_tick0; }
void API w16_SetRect(LPRECT r, short l, short t, short rr, short b)
{ r->left = l; r->top = t; r->right = rr; r->bottom = b; }
void API w16_CopyRect(LPRECT d, LPRECT s) { *d = *s; }
void API w16_OffsetRect(LPRECT r, short x, short y)
{ r->left += x; r->right += x; r->top += y; r->bottom += y; }
void API w16_InflateRect(LPRECT r, short x, short y)
{ r->left -= x; r->right += x; r->top -= y; r->bottom += y; }
BOOL API w16_IsRectEmpty(LPRECT r) { return r->left >= r->right || r->top >= r->bottom; }
BOOL API w16_PtInRect(LPRECT r, DWORD pt)
{
    short x = (short)pt, y = (short)(pt >> 16);
    return x >= r->left && x < r->right && y >= r->top && y < r->bottom;
}
BOOL API w16_EqualRect(LPRECT a, LPRECT b)
{ return a->left == b->left && a->top == b->top && a->right == b->right && a->bottom == b->bottom; }
BOOL API w16_IntersectRect(LPRECT d, LPRECT a, LPRECT b)
{
    RECT r;
    r.left = a->left > b->left ? a->left : b->left;
    r.top = a->top > b->top ? a->top : b->top;
    r.right = a->right < b->right ? a->right : b->right;
    r.bottom = a->bottom < b->bottom ? a->bottom : b->bottom;
    if (r.left >= r.right || r.top >= r.bottom) { memset(&r, 0, sizeof r); *d = r; return 0; }
    *d = r; return 1;
}
BOOL API w16_UnionRect(LPRECT d, LPRECT a, LPRECT b)
{
    RECT r;
    if (w16_IsRectEmpty(a)) { *d = *b; return !w16_IsRectEmpty(b); }
    if (w16_IsRectEmpty(b)) { *d = *a; return 1; }
    r.left = a->left < b->left ? a->left : b->left;
    r.top = a->top < b->top ? a->top : b->top;
    r.right = a->right > b->right ? a->right : b->right;
    r.bottom = a->bottom > b->bottom ? a->bottom : b->bottom;
    *d = r; return 1;
}
short API w16_GetKeyState(short vk) { return 0; }
short API w16_GetAsyncKeyState(short vk) { return 0; }
BOOL API w16_PeekMessage(LPVOID msg, WORD hwnd, WORD a, WORD b, WORD rem) { return 0; }
BOOL API w16_PostMessage(WORD hwnd, WORD msg, WORD wp, DWORD lp) { return 1; }
WORD API w16_GetCapture(void) { return 0; }
void API w16_MessageBeep(WORD t) { }

static BOOL patblt(HDC hdc, short x, short y, short w, short h, DWORD rop, HANDLE hbr)
{
    OBJ *dc = h_get(hdc, H_DC), *br;
    LPBITMAP s;
    if (!dc || !(s = dc_surface(dc))) return 0;
    br = h_get(hbr ? hbr : dc->hbr, H_BRUSH);
    if (!rect_clip(&x, &y, &w, &h, 0, 0, s->bmWidth, s->bmHeight)) return 1;
    return BitBlt(s, x, y, 0, 0, 0, w, h, rop, br ? &br->pb : 0, 0);
}
short API w16_FillRect(HDC hdc, LPRECT r, HANDLE hbr)
{
    return patblt(hdc, r->left, r->top, r->right - r->left, r->bottom - r->top, 0xF00021L, hbr);
}
void API w16_InvertRect(HDC hdc, LPRECT r)
{
    patblt(hdc, r->left, r->top, r->right - r->left, r->bottom - r->top, 0x550009L, 0);
}

/* DIB-Ressource -> Bitmap im Geraeteformat (gerastert) */
HANDLE API w16_LoadBitmap(HANDLE hinst, LPSTR name)
{
    WORD len, nc, stride, x, y, bpp, w, h, p;
    BYTE FAR *d = ne_resource(&g_mod, 2, name, &len), FAR *px, FAR *bits;
    BYTE map[256], FAR *pal;
    HANDLE hb, hg;
    OBJ *o;
    if (!d) return 0;
    w = *(WORD FAR *)(d + 4); h = *(WORD FAR *)(d + 8); bpp = *(WORD FAR *)(d + 14);
    nc = *(WORD FAR *)(d + 32); if (!nc) nc = 1 << bpp;
    if (bpp > 8) return 0;
    pal = d + *(WORD FAR *)d;               /* RGBQUAD: Blau, Gruen, Rot */
    for (x = 0; x < 256; x++) map[x] = (BYTE)x;
    px = d + *(WORD FAR *)d + 4 * nc;
    stride = ((w * bpp + 31) >> 5) << 2;
    hg = w16_galloc(drv_bitmap_bytes(w, h), &bits);
    if (!hg) return 0;
    hb = h_new(H_BITMAP); o = &obj[H2I(hb)];
    o->seg = obj[H2I(hg)].seg; obj[H2I(hg)].type = H_FREE;
    drv_bitmap_init(&o->bm, w, h, bits);
    {
        /* Rasterfarben je Zeilenparitaet und Spaltenparitaet vorab; je Zeile
           drei Akkumulatoren (eine je Ebene), Byte fuer Byte abgelegt */
        static BYTE ct[2][2][256];
        WORD np = o->bm.bmPlanes, wb = o->bm.bmWidthBytes;
        /* jede Farbe der Tafel fuer sich gerastert (je Grundfarbe 0..4 Viertel) */
        for (y = 0; y < 2; y++) for (x = 0; x < 2; x++) for (p = 0; p < nc && p < 256; p++)
            ct[y][x][p] = drv_dither_rgb(pal[4 * p + 2], pal[4 * p + 1], pal[4 * p], x, y);
        if (bpp == 4) {
            /* 4 Bit: je Quellbyte zwei Punkte ueber eine Tafel je Zeilen-
               paritaet (Bits 0-1 Gruen, 2-3 Rot, 4-5 Blau; linker Punkt
               unten, wie im Bitmapformat des Treibers: Bit 0 links). Die Tafel gilt, solange die Farbtafel gleich bleibt. */
            static BYTE t2[2][256], tpal[64], tok;
            if (!tok || _fmemcmp(tpal, pal, 64)) {
                WORD v;
                for (y = 0; y < 2; y++)
                    for (v = 0; v < 256; v++) {
                        BYTE c0 = ct[y][0][map[v >> 4]], c1 = ct[y][1][map[v & 15]], t = 0;
                        for (p = 0; p < 3; p++)
                            t |= (BYTE)((((c0 >> p) & 1) | ((c1 >> p) & 1) << 1) << (2 * p));
                        t2[y][v] = t;
                    }
                _fmemcpy(tpal, pal, 64); tok = 1;
            }
            for (y = 0; y < h; y++) {
                BYTE FAR *row = px + (DWORD)(h - 1 - y) * stride;
                BYTE FAR *dst = bits + (DWORD)y * np * wb;
                const BYTE *t = t2[y & 1];
                WORD n = (w + 1) >> 1, k = 0, xb = 0;
                BYTE a0 = 0, a1 = 0, a2 = 0;
                while (n--) {
                    BYTE v = t[*row++], sh = (BYTE)(2 * k);
                    if ((w & 1) && !n) v &= 0x15;        /* letzter halber Punkt: nur der linke */
                    a0 |= (BYTE)((v & 3) << sh);
                    a1 |= (BYTE)(((v >> 2) & 3) << sh);
                    a2 |= (BYTE)((v >> 4) << sh);
                    if (++k == 4 || !n) {
                        dst[xb] = a0;
                        if (np > 1) { dst[wb + xb] = a1; dst[2 * wb + xb] = a2; }
                        xb++; k = 0; a0 = a1 = a2 = 0;
                    }
                }
            }
            return hb;
        }
        for (y = 0; y < h; y++) {
            BYTE FAR *row = px + (DWORD)(h - 1 - y) * stride;
            BYTE FAR *dst = bits + (DWORD)y * np * wb;
            const BYTE *te = ct[y & 1][0], *to = ct[y & 1][1];
            BYTE a0 = 0, a1 = 0, a2 = 0, bit = 1;
            WORD xb = 0;
            for (x = 0; x < w; x++) {
                BYTE v, c;
                if (bpp == 4) v = (x & 1) ? row[x >> 1] & 15 : row[x >> 1] >> 4;
                else if (bpp == 1) v = (row[x >> 3] >> (7 - (x & 7))) & 1;
                else v = row[x];
                c = (x & 1 ? to : te)[map[v]];
                if (c & 1) a0 |= bit;
                if (c & 2) a1 |= bit;
                if (c & 4) a2 |= bit;
                bit <<= 1;
                if (!bit || x + 1 == w) {
                    dst[xb] = a0;
                    if (np > 1) { dst[wb + xb] = a1; dst[2 * wb + xb] = a2; }
                    xb++; a0 = a1 = a2 = 0; bit = 1;
                }
            }
        }
    }
    return hb;
}

/* ================= GDI ================= */
HANDLE API w16_GetStockObject(short i) { return (i >= 0 && i < 20) ? stock[i] : 0; }
HANDLE API w16_CreateSolidBrush(DWORD rgb)
{
    HANDLE h = h_new(H_BRUSH);
    if (h) RealizeObject(&g_dev, OBJ_BRUSH | 0x200, (LPVOID)&rgb, &obj[H2I(h)].pb, 0);
    return h;
}
HDC API w16_CreateCompatibleDC(HDC hdc)
{
    HANDLE h = h_new(H_DC);
    if (h) { obj[H2I(h)].hbr = stock[0]; obj[H2I(h)].hpen = stock[7]; obj[H2I(h)].rop2 = 13; obj[H2I(h)].bkcolor = 7; }
    return h;
}
BOOL API w16_DeleteDC(HDC h) { OBJ *o = h_get(h, H_DC); if (o && !o->screen) o->type = H_FREE; return 1; }
BOOL API w16_DeleteObject(HANDLE h)
{
    WORD i = H2I(h);
    if (!h || i >= MAXH || obj[i].stock) return 0;
    if (obj[i].type == H_BITMAP && obj[i].seg) _dos_freemem(obj[i].seg);
    obj[i].type = H_FREE;
    return 1;
}
HANDLE API w16_CreateCompatibleBitmap(HDC hdc, short w, short h)
{
    BYTE FAR *bits; HANDLE hg, hb; OBJ *o;
    hg = w16_galloc(drv_bitmap_bytes(w, h), &bits);
    if (!hg) return 0;
    hb = h_new(H_BITMAP); o = &obj[H2I(hb)];
    o->seg = obj[H2I(hg)].seg; obj[H2I(hg)].type = H_FREE;
    drv_bitmap_init(&o->bm, w, h, bits);
    return hb;
}
HANDLE API w16_SelectObject(HDC hdc, HANDLE h)
{
    OBJ *dc = h_get(hdc, H_DC); WORD i = H2I(h); HANDLE old = 0;
    if (!dc || !h || i >= MAXH) return 0;
    switch (obj[i].type) {
    case H_BITMAP: old = dc->hbm; dc->hbm = h; break;
    case H_BRUSH:  old = dc->hbr; dc->hbr = h; break;
    case H_PEN:    old = dc->hpen; dc->hpen = h; break;
    default: return 0;
    }
    return old ? old : h;
}
short API w16_GetObject(HANDLE h, short n, LPVOID p)
{
    OBJ *o = h_get(h, H_BITMAP);
    BYTE buf[14];
    if (!o) return 0;
    *(short *)(buf + 0) = 0; *(short *)(buf + 2) = o->bm.bmWidth; *(short *)(buf + 4) = o->bm.bmHeight;
    *(short *)(buf + 6) = o->bm.bmWidthBytes; buf[8] = o->bm.bmPlanes; buf[9] = 1;
    *(DWORD *)(buf + 10) = 0;
    if (n > 14) n = 14;
    _fmemcpy(p, buf, n);
    return n;
}
BOOL API w16_BitBlt(HDC hd, short x, short y, short w, short h, HDC hs, short xs, short ys, DWORD rop)
{
    OBJ *dd = h_get(hd, H_DC), *sd = hs ? h_get(hs, H_DC) : 0, *br;
    LPBITMAP d, s = 0;
    if (!dd || !(d = dc_surface(dd))) return 0;
    if (hs) {
        if (!sd || !(s = dc_surface(sd))) return 0;
        if (!rect_clip(&xs, &ys, &w, &h, &x, &y, s->bmWidth, s->bmHeight)) return 1;
    }
    if (!rect_clip(&x, &y, &w, &h, hs ? &xs : 0, hs ? &ys : 0, d->bmWidth, d->bmHeight)) return 1;
    br = h_get(dd->hbr, H_BRUSH);
    return BitBlt(d, x, y, s, xs, ys, w, h, rop, br ? &br->pb : 0, 0);
}
BOOL API w16_PatBlt(HDC hdc, short x, short y, short w, short h, DWORD rop)
{
    return patblt(hdc, x, y, w, h, rop, 0);
}
DWORD API w16_SetBkColor(HDC h, DWORD c) { OBJ *o = h_get(h, H_DC); DWORD r = o ? o->bkcolor : 0; if (o) o->bkcolor = c; return r; }
DWORD API w16_SetTextColor(HDC h, DWORD c) { OBJ *o = h_get(h, H_DC); DWORD r = o ? o->txcolor : 0; if (o) o->txcolor = c; return r; }
short API w16_SetBkMode(HDC h, short m) { OBJ *o = h_get(h, H_DC); short r = o ? o->bkmode : 0; if (o) o->bkmode = m; return r; }
short API w16_SetROP2(HDC h, short m) { OBJ *o = h_get(h, H_DC); short r = o ? o->rop2 : 0; if (o) o->rop2 = m; return r; }
DWORD API w16_SetPixel(HDC hdc, short x, short y, DWORD rgb)
{
    OBJ *dc = h_get(hdc, H_DC); LPBITMAP s; DWORD phys;
    if (!dc || !(s = dc_surface(dc))) return 0;
    if (x < 0 || y < 0 || x >= s->bmWidth || y >= s->bmHeight) return (DWORD)-1;
    ColorInfo(s, rgb, &phys);
    if (s->bmType) Pixel(s, x, y, phys, 0);
    else {
        int p;
        for (p = 0; p < s->bmPlanes; p++) {
            BYTE FAR *b = s->bmBits + ((DWORD)y * s->bmPlanes + p) * s->bmWidthBytes + (x >> 3);
            if ((BYTE)phys & (1 << p)) *b |= 1 << (x & 7); else *b &= ~(1 << (x & 7));
        }
    }
    return rgb;
}

/* ---- Rollen und DIB-Zeilen (fuer Module mit Hardware-Rollen) ------------ */

/* ScrollDC: nur der ganze Bildschirm senkrecht (dx = 0); der Treiber
   rollt dann ueber die Startadresse des 7220 (BitBlt Schirm -> Schirm).
   Der frei werdende Streifen ist schwarz und steht in *upd. */
BOOL API w16_ScrollDC(HDC hdc, short dx, short dy, LPRECT scr, LPRECT clip, WORD hrgn, LPRECT upd)
{
    OBJ *dc = h_get(hdc, H_DC);
    short a = dy < 0 ? -dy : dy;
    if (!dc || !dc->screen || dx || !dy || a >= 400) return 0;
    if (scr && (scr->left > 0 || scr->top > 0 || scr->right < 640 || scr->bottom < 400)) return 0;
    if (!BitBlt(&g_dev, 0, dy > 0 ? dy : 0, &g_dev, 0, dy > 0 ? 0 : a, 640, 400 - a, 0xCC0020L, 0, 0)) return 0;
    if (upd) {
        upd->left = 0; upd->right = 640;
        upd->top = dy < 0 ? 400 - a : 0; upd->bottom = dy < 0 ? 400 : a;
    }
    return 1;
}

/* SetDIBitsToDevice: Zeilen einer DIB (1, 4 oder 8 Bit, unten zuerst)
   Zeile fuer Zeile umrechnen (gerastert nach Bildschirmlage) und als
   Speicher-Bitmap einer Zeile an den Treiber geben */
short API w16_SetDIBitsToDevice(HDC hdc, short x, short y, short cx, short cy, short xs, short ys,
                                WORD start, WORD num, LPVOID lpBits, LPVOID lpbmi, WORD use)
{
    static BYTE rb[3 * 80];
    BYTE map[256];
    OBJ *dc = h_get(hdc, H_DC);
    BYTE FAR *bi = (BYTE FAR *)lpbmi, FAR *px = (BYTE FAR *)lpBits;
    LPBITMAP d;
    BITMAP rbm;
    WORD bpp, nc, stride, k, i, n = 0;
    short w, bw;
    if (!dc || !(d = dc_surface(dc)) || !bi || !px) return 0;
    w = (short)*(DWORD FAR *)(bi + 4);
    bpp = *(WORD FAR *)(bi + 14);
    if (bpp != 1 && bpp != 4 && bpp != 8) return 0;
    nc = (WORD)*(DWORD FAR *)(bi + 32); if (!nc) nc = 1 << bpp;
    for (i = 0; i < nc && i < 256; i++) {
        BYTE FAR *q = bi + *(WORD FAR *)bi + 4 * i;
        map[i] = use ? (BYTE)i : nearest16(q[2], q[1], q[0]);
    }
    stride = ((w * bpp + 31) >> 5) << 2;
    if (xs < 0) { cx += xs; x -= xs; xs = 0; }
    if (xs + cx > w) cx = w - xs;
    if (cx > 640) cx = 640;
    if (cx <= 0) return 0;
    for (k = 0; k < num; k++) {
        BYTE FAR *row = px + (DWORD)k * stride;
        short yd = y + cy - 1 - (short)(start + k) + ys, xd = x, sx0 = 0, ww = cx, hh = 1;
        BYTE a0 = 0, a1 = 0, a2 = 0, bit = 1, np;
        WORD xb = 0;
        if (yd < 0 || yd >= d->bmHeight) continue;
        _fmemset(&rbm, 0, sizeof rbm);
        drv_bitmap_init(&rbm, cx, 1, (BYTE FAR *)rb);
        np = rbm.bmPlanes; bw = rbm.bmWidthBytes;
        for (i = 0; i < (WORD)cx; i++) {
            WORD sxp = xs + i;
            BYTE v, c;
            if (bpp == 4) v = (sxp & 1) ? row[sxp >> 1] & 15 : row[sxp >> 1] >> 4;
            else if (bpp == 1) v = (row[sxp >> 3] >> (7 - (sxp & 7))) & 1;
            else v = row[sxp];
            if (use) c = drv_dither(map[v], x + i, yd);
            else { BYTE FAR *q = bi + *(WORD FAR *)bi + 4 * v; c = drv_dither_rgb(q[2], q[1], q[0], x + i, yd); }
            if (c & 1) a0 |= bit;
            if (c & 2) a1 |= bit;
            if (c & 4) a2 |= bit;
            bit <<= 1;
            if (!bit || i + 1 == (WORD)cx) {
                rb[xb] = a0;
                if (np > 1) { rb[bw + xb] = a1; rb[2 * bw + xb] = a2; }
                xb++; a0 = a1 = a2 = 0; bit = 1;
            }
        }
        if (!rect_clip(&xd, &yd, &ww, &hh, &sx0, 0, d->bmWidth, d->bmHeight)) continue;
        BitBlt(d, xd, yd, &rbm, sx0, 0, ww, 1, 0xCC0020L, 0, 0);
        n++;
    }
    return n;
}


/* nach einem Modul: allen Speicher und alle Handles freigeben */
void w16_reset(void)
{
    int i;
    for (i = 1; i < MAXH; i++)
        if ((obj[i].type == H_GLOBAL || obj[i].type == H_BITMAP) && obj[i].seg) _dos_freemem(obj[i].seg);
    memset(obj, 0, sizeof obj);
    unb_n = 0;
}

/* ---- Stifte und Linien --------------------------------------------------- */
HANDLE API w16_CreatePen(short style, short width, DWORD rgb)
{
    HANDLE h = h_new(H_PEN);
    DWORD phys;
    if (!h) return 0;
    ColorInfo(&g_dev, rgb, &phys);
    obj[H2I(h)].pp.color = (BYTE)phys;
    obj[H2I(h)].pp.style = (BYTE)(style == 5 ? 5 : 0);       /* gestrichelt: voll */
    (void)width;
    return h;
}

DWORD API w16_MoveTo(HDC hdc, short x, short y)
{
    OBJ *dc = h_get(hdc, H_DC);
    DWORD old;
    if (!dc) return 0;
    old = (DWORD)(WORD)dc->cx | (DWORD)(WORD)dc->cy << 16;
    dc->cx = x; dc->cy = y;
    return old;
}

BOOL API w16_LineTo(HDC hdc, short x, short y)
{
    OBJ *dc = h_get(hdc, H_DC), *pn;
    LPBITMAP s;
    POINT pt[2];
    DRAWMODE dm;
    if (!dc || !(s = dc_surface(dc))) return 0;
    pn = h_get(dc->hpen ? dc->hpen : stock[7], H_PEN);
    pt[0].x = dc->cx; pt[0].y = dc->cy; pt[1].x = x; pt[1].y = y;
    dc->cx = x; dc->cy = y;
    if (!pn) return 1;
    _fmemset(&dm, 0, sizeof dm);
    dm.Rop2 = dc->rop2 ? dc->rop2 : 13;
    Output(s, OS_POLYLINE, 2, (LPPOINT)pt, (LPPPEN)&pn->pp, 0, (LPDRAWMODE)&dm, 0);
    return 1;
}

BOOL API w16_Polyline(HDC hdc, LPPOINT pts, short n)
{
    OBJ *dc = h_get(hdc, H_DC), *pn;
    LPBITMAP s;
    DRAWMODE dm;
    if (!dc || !(s = dc_surface(dc)) || n < 2) return 0;
    pn = h_get(dc->hpen ? dc->hpen : stock[7], H_PEN);
    if (!pn) return 1;
    _fmemset(&dm, 0, sizeof dm);
    dm.Rop2 = dc->rop2 ? dc->rop2 : 13;
    Output(s, OS_POLYLINE, n, pts, (LPPPEN)&pn->pp, 0, (LPDRAWMODE)&dm, 0);
    return 1;
}

/* ---- Ellipse, Rechteck, Rahmen ------------------------------------------ */
static BOOL figur(HDC hdc, short art, short l, short t, short r, short b)
{
    OBJ *dc = h_get(hdc, H_DC), *pn, *br;
    LPBITMAP s;
    POINT pt[2];
    DRAWMODE dm;
    if (!dc || !(s = dc_surface(dc))) return 0;
    if (r < l) { short h = l; l = r; r = h; }
    if (b < t) { short h = t; t = b; b = h; }
    pn = h_get(dc->hpen ? dc->hpen : stock[7], H_PEN);
    br = h_get(dc->hbr ? dc->hbr : stock[0], H_BRUSH);
    pt[0].x = l; pt[0].y = t; pt[1].x = r; pt[1].y = b;
    _fmemset(&dm, 0, sizeof dm);
    dm.Rop2 = dc->rop2 ? dc->rop2 : 13;
    Output(s, art, 2, (LPPOINT)pt, pn ? (LPPPEN)&pn->pp : 0, br ? (LPPBRUSH)&br->pb : 0, (LPDRAWMODE)&dm, 0);
    return 1;
}
BOOL API w16_Ellipse(HDC hdc, short l, short t, short r, short b) { return figur(hdc, OS_ELLIPSE, l, t, r, b); }
BOOL API w16_Rectangle(HDC hdc, short l, short t, short r, short b) { return figur(hdc, OS_RECTANGLE, l, t, r, b); }

short API w16_FrameRect(HDC hdc, LPRECT rc, HANDLE hbr)
{
    RECT e;
    if (!rc) return 0;
    e = *rc; e.bottom = e.top + 1;            w16_FillRect(hdc, (LPRECT)&e, hbr);
    e = *rc; e.top = e.bottom - 1;            w16_FillRect(hdc, (LPRECT)&e, hbr);
    e = *rc; e.right = e.left + 1;            w16_FillRect(hdc, (LPRECT)&e, hbr);
    e = *rc; e.left = e.right - 1;            w16_FillRect(hdc, (LPRECT)&e, hbr);
    return 1;
}
