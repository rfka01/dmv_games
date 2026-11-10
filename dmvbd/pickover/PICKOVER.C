/* PICKOVER.C - "Pickover": eigenes Bildschirmschoner-Modul fuer DMVBD und
   After Dark 1.x fuer Windows 3.0 (Real Mode).
   Seltsame Attraktoren nach Clifford Pickover (Chaos in Wonderland):
       x' = sin(a y) - z cos(b x)
       y' = z sin(c x) - cos(d y)
       z' = e sin(x)
   Die Bahn wird gedreht auf die Ebene projiziert; jeder Punkt zaehlt, wie
   oft die Bahn ihn trifft. Die Dichte wird als Grau gezeichnet (4x4-Bayer
   aus Schwarz und Weiss, wahlweise ueber Blau und Cyan nach Weiss).
   Ablauf: zwei Zaehlpuffer. Waehrend ein Gebilde Zeile fuer Zeile unten
   hereinlaeuft und oben hinauszieht, wird das naechste gerechnet. Gerollt
   wird mit ScrollDC (in DMVBD: Hardware-Rollen des 7220), die neuen
   Zeilen kommen mit SetDIBitsToDevice. Ganzzahlig (Q12, Sinustafel).
   Code: Claude, Prompts: rfka01 - MIT-Lizenz */

#include <windows.h>
#include <string.h>
#include "PICKOVER.H"
#include "SINTAB.H"

#define PW 320                   /* Gebilde: Breite, Hoehe in Punkten */
#define PH 300
#define CSTR (PW / 2)            /* Zaehler: 4 Bit je Punkt */
#define LUECKE 30                /* leere Zeilen zwischen zwei Gebilden */
#define MAXN 4                   /* hoechstens 4 Zeilen je Bild */
#define ERKUNDEN 1500L           /* Schritte nur fuer die Ausdehnung */

static HANDLE hInst, hBuf[2];
static BYTE FAR *cnt[2];         /* Zaehler, oberste Zeile zuerst */
static int   W, H, xoff, mono, bunt;
static int   tempo;
static long  nmax;
static int   ein, zeile, luecke; /* einlaufender Puffer, naechste Zeile */
static int   bereit;             /* naechster Puffer fertig gerechnet */
static int   erstes;
/* Zustand der Rechnung (Puffer 1 - ein) */
static long  iter;
static int   x, y, z;            /* Q12 */
static int   a, b, c, d, e;      /* Q12 */
static int   ca, sa;             /* Drehung der Projektion, Q12 */
static long  sx, sy, mx, my;     /* Massstab (Q12 -> Punkte, *1/4096), Mitte */
static long  neu;
static int   u0, u1, v0, v1;     /* Ausdehnung beim Erkunden, Q12 */
static unsigned long seed;

static struct {
    BITMAPINFOHEADER h;
    RGBQUAD pal[16];
} bmi;
static BYTE zbuf[MAXN][PW / 2];  /* Ausgabezeilen, 4 Bit */

static const BYTE bayer[4][4] = { {0,8,2,10}, {12,4,14,6}, {3,11,1,9}, {15,7,13,5} };
/* Zaehler 0..15 -> Helligkeit 0..48 (etwa logarithmisch) */
static const BYTE hell[16] = { 0, 14, 21, 26, 30, 33, 36, 38, 40, 42, 43, 44, 45, 46, 47, 48 };

static int rnd(int n) { seed = seed * 1103515245UL + 12345UL; return (int)((seed >> 16) % (unsigned)n); }

/* sin, cos eines Winkels in Q12-Bogenmass, Ergebnis Q12; linear zwischen
   den Tafelwerten (sonst laeuft die Bahn auf dem Raster in Kreislaeufe) */
static int qtab(long u)
{
    int i = (int)(u >> 12), f = (int)(u >> 4) & 255;
    int s0 = sintab[i & 1023], s1 = sintab[(i + 1) & 1023];
    return (s0 + (((s1 - s0) * f) >> 8)) >> 2;        /* |s1-s0| <= 101: passt in 16 Bit */
}
static int qsin(int t) { return qtab((long)t * 163L); }
static int qcos(int t) { return qtab((long)t * 163L + 256L * 4096L); }
static int mul(int p, int q) { return (int)(((long)p * q) >> 12); }

static int zuf(int m, int lo)    /* Zufall in +-m (Q12), Betrag mindestens lo */
{
    int v = lo + rnd(m - lo);
    return rnd(2) ? v : -v;
}

static void neu_beginnen(int k)
{
    long r;
    _fmemset(cnt[k], 0, (WORD)CSTR * PH);
    a = zuf(12288, 2000); b = zuf(12288, 1200); c = zuf(12288, 1200); d = zuf(12288, 2000);
    e = 2048 + rnd(3072);                          /* 0.5 .. 1.25 */
    r = rnd(1024);                                 /* Drehung der Projektion */
    ca = sintab[(int)(r + 256) & 1023] >> 2; sa = sintab[(int)r] >> 2;
    u0 = v0 = 32767; u1 = v1 = -32767;
    x = 410; y = 205; z = 100;
    iter = 0; neu = 0;
}

static void zaehlen(BYTE FAR *p, int px, int py)
{
    BYTE FAR *q = p + (long)py * CSTR + (px >> 1);
    BYTE v = *q;
    if (px & 1) { if ((v & 15) < 15) { if (!(v & 15)) neu++; *q = v + 1; } }
    else        { if ((v >> 4) < 15) { if (!(v >> 4)) neu++; *q = v + 16; } }
}

/* n Schritte fuer Puffer k; 1, wenn das Gebilde fertig ist */
static int rechnen(int k, int n)
{
    long lim = (erstes ? nmax / 4 : nmax) + ERKUNDEN;   /* das erste schneller */
    while (n-- > 0 && iter < lim) {
        int nx = qsin(mul(a, y)) - mul(z, qcos(mul(b, x)));
        int ny = mul(z, qsin(mul(c, x))) - qcos(mul(d, y));
        int nz = mul(e, qsin(x));
        int px, py, u, v;
        x = nx; y = ny; z = nz;
        iter++;
        if (iter < 32) continue;                     /* einschwingen */
        u = mul(x, ca) - mul(y, sa);
        v = mul(x, sa) + mul(y, ca);
        if (iter < ERKUNDEN) {                       /* erst nur die Ausdehnung */
            if (u < u0) u0 = u; if (u > u1) u1 = u;
            if (v < v0) v0 = v; if (v > v1) v1 = v;
            continue;
        }
        if (iter == ERKUNDEN) {                      /* Massstab: fuellt den Kasten */
            long du = (long)u1 - u0 + 1, dv = (long)v1 - v0 + 1;
            if (du < 400 || dv < 400) { neu_beginnen(k); continue; }   /* Punkt/Linie */
            sx = (long)(PW - 6) * 4096L / du;
            sy = sx * 5L / 6L;                       /* DMV-Punkte sind hoeher als breit */
            if (sy * dv / 4096L > PH - 6) { sy = (long)(PH - 6) * 4096L / dv; sx = sy * 6L / 5L; }
            mx = PW / 2 - ((((long)u0 + u1) / 2) * sx >> 12);
            my = PH / 2 + ((((long)v0 + v1) / 2) * sy >> 12);
        }
        px = (int)(mx + ((long)u * sx >> 12));
        py = (int)(my - ((long)v * sy >> 12));
        if (px < 0 || px >= PW || py < 0 || py >= PH) continue;
        zaehlen(cnt[k], px, py);
        if (iter == ERKUNDEN + 4000 && neu < 1800) neu_beginnen(k);   /* zu karg: neue Werte */
    }
    return iter >= lim;
}

/* Zeile r des Puffers k als 4-Bit-Zeile (Bildschirmzeile ys fuers Raster) */
static void zeile_bauen(BYTE *z4, int k, int r, int ys)
{
    BYTE FAR *q = cnt[k] + (long)r * CSTR;
    int i;
    for (i = 0; i < PW; i += 2) {
        BYTE v = *q++, o = 0, j;
        for (j = 0; j < 2; j++) {
            int h = hell[j ? v & 15 : v >> 4], t = bayer[ys & 3][(xoff + i + j) & 3], col = 0;
            if (!bunt || mono) col = h > t * 3 ? 15 : 0;              /* Schwarz..Weiss */
            else if (h > 32) col = (h - 32) > t ? 15 : 14;           /* Cyan..Weiss */
            else if (h > 16) col = (h - 16) > t ? 14 : 12;           /* Blau..Cyan */
            else col = h > t ? 12 : 0;                               /* Schwarz..Blau */
            o = (BYTE)(o << 4 | col);
        }
        *z4++ = o;
    }
}

static int doInit(LPSYSTEM sys, LPMODULE m)
{
    static const BYTE pal[16][3] = {
        {0,0,0},{128,0,0},{0,128,0},{128,128,0},{0,0,128},{128,0,128},{0,128,128},{128,128,128},
        {192,192,192},{255,0,0},{0,255,0},{255,255,0},{0,0,255},{255,0,255},{0,255,255},{255,255,255} };
    int i;
    W = m->ptRgnSize.x; H = m->ptRgnSize.y;
    mono = sys->iBitsPerPixel == 1;
    tempo = m->iControlValue[0]; if (tempo < 1) tempo = 1; if (tempo > MAXN) tempo = MAXN;
    nmax = (long)m->iControlValue[1] * 1000L; if (nmax < 5000L) nmax = 5000L;
    bunt = m->iControlValue[2] != 0;
    for (i = 0; i < 2; i++) {
        hBuf[i] = GlobalAlloc(GMEM_MOVEABLE, (DWORD)CSTR * PH);
        if (!hBuf[i]) return MEM_ERROR;
        cnt[i] = (BYTE FAR *)GlobalLock(hBuf[i]);
    }
    bmi.h.biSize = sizeof(BITMAPINFOHEADER);
    bmi.h.biWidth = PW; bmi.h.biHeight = 1;
    bmi.h.biPlanes = 1; bmi.h.biBitCount = 4;
    for (i = 0; i < 16; i++) {
        bmi.pal[i].rgbRed = pal[i][0]; bmi.pal[i].rgbGreen = pal[i][1]; bmi.pal[i].rgbBlue = pal[i][2];
    }
    xoff = (W - PW) / 2; if (xoff < 0) xoff = 0;
    xoff &= ~3;
    seed = GetCurrentTime();
    return NOERROR;
}

static int doBlank(HDC hdc)
{
    RECT r;
    SetRect(&r, 0, 0, W, H);
    FillRect(hdc, &r, GetStockObject(BLACK_BRUSH));
    ein = 0; zeile = PH; luecke = 0;      /* nichts laeuft ein; Puffer 1 rechnen */
    erstes = 1; bereit = 0;
    neu_beginnen(1);
    return NOERROR;
}

static int doFrame(HDC hdc)
{
    RECT upd;
    int n = tempo, k, schritte;
    ScrollDC(hdc, 0, -n, NULL, NULL, NULL, &upd);
    if (zeile < PH) {                        /* Gebilde laeuft ein */
        if (zeile + n > PH) n = PH - zeile;
        for (k = 0; k < n; k++)              /* DIB: unterste Zeile zuerst */
            zeile_bauen(zbuf[n - 1 - k], ein, zeile + k, H - n + k);
        bmi.h.biHeight = n;
        SetDIBitsToDevice(hdc, xoff, H - n, PW, n, 0, 0, 0, n, (LPSTR)zbuf, (LPBITMAPINFO)&bmi, DIB_RGB_COLORS);
        zeile += n;
        if (zeile >= PH) luecke = 0;
    } else {
        luecke += n;                         /* leerer Himmel, ab und zu ein Stern */
        if (rnd(6) == 0) SetPixel(hdc, rnd(W), H - 1, RGB(255, 255, 255));
    }
    /* Rechnen: so viele Schritte, dass das naechste Gebilde fertig ist,
       wenn das laufende samt Luecke durch ist */
    if (!bereit) {
        long rest = (long)(PH - (zeile < PH ? zeile : PH)) + LUECKE;
        schritte = (int)(nmax * tempo / rest + 1);
        if (zeile >= PH && luecke >= LUECKE) schritte = 1500;   /* wartet: schneller */
        if (schritte > 1500) schritte = 1500;
        if (schritte < 100) schritte = 100;
        bereit = rechnen(1 - ein, schritte);
    }
    if (bereit && zeile >= PH && luecke >= LUECKE) {
        ein = 1 - ein; zeile = 0; erstes = 0;
        bereit = 0; neu_beginnen(1 - ein);
    }
    return NOERROR;
}

static int doClose(void)
{
    int i;
    for (i = 0; i < 2; i++)
        if (hBuf[i]) { GlobalUnlock(hBuf[i]); GlobalFree(hBuf[i]); hBuf[i] = 0; }
    return NOERROR;
}

int FAR PASCAL Module(int msg, HDC hdc, HANDLE hSystem)
{
    LPSYSTEM sys = (LPSYSTEM)GlobalLock(hSystem);
    LPMODULE mod = (LPMODULE)GlobalLock(sys->hModuleInfo);
    int r = NOERROR;
    switch (msg) {
    case INITIALIZE: r = doInit(sys, mod); break;
    case BLANK:      r = doBlank(hdc); break;
    case DRAWFRAME:  r = doFrame(hdc); break;
    case CLOSE:      r = doClose(); break;
    }
    GlobalUnlock(sys->hModuleInfo);
    GlobalUnlock(hSystem);
    return r;
}

int PASCAL LibMain(HANDLE hInstance, WORD wDataSeg, WORD wHeapSize, LPSTR lpCmdLine)
{
    hInst = hInstance;
    return 1;
}

int FAR PASCAL WEP(int bSystemExit) { return 1; }
