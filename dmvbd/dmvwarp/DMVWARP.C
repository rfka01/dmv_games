/* DMVWARP.C - "DMV-Warp": eigenes Bildschirmschoner-Modul fuer DMVBD und After
   Dark 1.x fuer Windows 3.0 (Real Mode).
   Der Schriftzug DECISION MATE V fliegt in Wellen von rechts nach links
   und zieht einen Regenbogen aus Bloecken hinter sich her, der hinten
   breiter wird (wie ein Lichtstrahl im Warp). Die Bloecke bleiben stehen,
   wo sie entstehen; die aeltesten verloeschen wieder.
   Code: Claude, Prompts: rfka01 - MIT-Lizenz */

#include <windows.h>
#include "DMVWARP.H"
#include "LOGOGR.H"
#include "SINTAB.H"

#define MAXB 96                  /* Bloecke im Schweif */
#define BW 6                     /* Blockbreite, dahinter 2 Punkte Luecke */
#define NS 6                     /* Streifen */

static HANDLE hInst;
static HBITMAP bmLogo;
static HDC   hMem;
static HBITMAP bmOld;
static HBRUSH farbe[NS];
static int   W, H, mono, tempo, laenge;
static int   lx, ly, olx, oly, phase, basis, amp, unterwegs;
static long fx, fy, vx, vy;      /* Bahn, Q8: Lage und Schritt je Punkt Tempo */
static struct { int x, y, h; } blk[MAXB];
static int   nb, kopf;           /* Zahl, naechster Platz */
static int   seit;               /* Weg seit dem letzten Block, Q8 */
static unsigned long seed;

static const COLORREF regenbogen[NS] = {
    RGB(255, 0, 0), RGB(255, 128, 0), RGB(255, 255, 0),
    RGB(0, 255, 0), RGB(0, 64, 255), RGB(192, 0, 255) };

static int rnd(int n) { seed = seed * 1103515245UL + 12345UL; return (int)((seed >> 16) % (unsigned)n); }

static void schwarz(HDC hdc, int x, int y, int w, int h)
{
    RECT r; SetRect(&r, x, y, x + w, y + h);
    FillRect(hdc, &r, GetStockObject(BLACK_BRUSH));
}

/* Block mit NS Streifen, Hoehe h (durch NS teilbar), Mitte y */
static void block(HDC hdc, int x, int y, int h, int an)
{
    int s, sh = h / NS, y0 = y - h / 2;
    RECT r;
    for (s = 0; s < NS; s++) {
        SetRect(&r, x, y0 + s * sh, x + BW, y0 + (s + 1) * sh);
        FillRect(hdc, &r, an ? (mono ? GetStockObject(WHITE_BRUSH) : farbe[s]) : GetStockObject(BLACK_BRUSH));
    }
}

/* neuer Flug: von rechts herein unter einem Winkel bis etwa +-35 Grad,
   mit oder ohne Welle; Mitte des Bildes irgendwo zwischen oben und unten */
static void neuer_flug(void)
{
    int w = rnd(200) - 100;                 /* Winkel in 1/1024 Umlauf */
    long ym = 70 + rnd(H - 160);            /* Hoehe in der Bildmitte */
    vx = -(long)(sintab[(w + 256) & 1023]) >> 6;      /* Q8, nach links */
    vy = (long)(sintab[w & 1023]) >> 6;
    fx = (long)(W + 10) << 8;
    fy = (ym << 8) - (vy * (W / 2 + 10 + LOGOW / 2)) * 256L / -vx;
    amp = rnd(3) ? 0 : 10 + rnd(40);        /* manchmal eine Welle dazu */
    phase = rnd(1024); unterwegs = 1; seit = 0;
    lx = (int)(fx >> 8); basis = ly = (int)(fy >> 8);
}

static int doInit(LPSYSTEM sys, LPMODULE m)
{
    int i;
    W = m->ptRgnSize.x; H = m->ptRgnSize.y;
    mono = sys->iBitsPerPixel == 1;
    tempo = m->iControlValue[0]; if (tempo < 1) tempo = 1; if (tempo > 10) tempo = 10;
    laenge = m->iControlValue[1]; if (laenge < 10) laenge = 10; if (laenge > MAXB) laenge = MAXB;
    bmLogo = LoadBitmap(hInst, "LOGO");
    for (i = 0; i < NS; i++) farbe[i] = CreateSolidBrush(regenbogen[i]);
    if (!bmLogo || !farbe[NS - 1]) return MEM_ERROR;
    seed = GetCurrentTime();
    return NOERROR;
}

static int doBlank(HDC hdc)
{
    RECT r;
    SetRect(&r, 0, 0, W, H);
    FillRect(hdc, &r, GetStockObject(BLACK_BRUSH));
    hMem = CreateCompatibleDC(hdc);
    bmOld = SelectObject(hMem, bmLogo);
    nb = 0; kopf = 0;
    neuer_flug();
    olx = lx; oly = basis;
    return NOERROR;
}

static int doFrame(HDC hdc)
{
    int step = 1 + tempo, ny;
    /* aeltesten Block loeschen, wenn der Schweif lang genug ist (oder der
       Schriftzug schon draussen ist) */
    if (nb > 0 && (nb >= laenge || !unterwegs)) {
        int a = (kopf - nb + MAXB) % MAXB;
        block(hdc, blk[a].x, blk[a].y, blk[a].h, 0);
        nb--;
    }
    if (!unterwegs) {                       /* warten, bis der Schweif weg ist */
        if (nb == 0) { neuer_flug(); olx = lx; oly = basis; }
        return NOERROR;
    }
    olx = lx; oly = ly;
    fx += vx * step; fy += vy * step;
    lx = (int)(fx >> 8);
    phase += 6 + tempo;
    ly = (int)(fy >> 8) + (int)(((long)amp * sintab[phase & 1023]) >> 14);
    ny = ly & ~1;
    /* neuer Block am rechten Ende des Schriftzugs, hinten breiter */
    seit += (int)(-vx * step);                 /* Q8: waagrechter Weg */
    if (seit >= (BW + 2) << 8 && lx + LOGOW < W) {
        int k;
        seit = 0;
        if (nb == MAXB) {                    /* voll: aeltesten aufgeben */
            int a = (kopf - nb + MAXB) % MAXB;
            block(hdc, blk[a].x, blk[a].y, blk[a].h, 0);
            nb--;
        }
        blk[kopf].x = lx + LOGOW + 2; blk[kopf].y = ny + LOGOH / 2 + 4; blk[kopf].h = 18;
        block(hdc, blk[kopf].x, blk[kopf].y, 18, 1);
        kopf = (kopf + 1) % MAXB;
        nb++;
        /* hinten breiter: wer jetzt 8, 16, ... Bloecke alt ist, waechst */
        for (k = 8; k < nb; k += 8) {
            int a = (kopf - 1 - k + MAXB) % MAXB, h = 18 + 6 * (k / 8);
            if (h > 48) break;
            block(hdc, blk[a].x, blk[a].y, h, 1);
            blk[a].h = h;
        }
    }
    /* Schriftzug: alte Raender schwarz, dann neu */
    if (olx > lx) schwarz(hdc, lx + LOGOW, oly & ~1, olx - lx, LOGOH);
    if ((oly & ~1) < ny) schwarz(hdc, olx, oly & ~1, LOGOW, ny - (oly & ~1));
    if ((oly & ~1) > ny) schwarz(hdc, olx, ny + LOGOH, LOGOW, (oly & ~1) - ny);
    BitBlt(hdc, lx, ny, LOGOW, LOGOH, hMem, 0, 0, SRCCOPY);
    if (lx + LOGOW < 0 || ly + LOGOH < -40 || ly > H + 40) unterwegs = 0;
    return NOERROR;
}

static int doClose(void)
{
    int i;
    if (hMem) { SelectObject(hMem, bmOld); DeleteDC(hMem); hMem = 0; }
    if (bmLogo) DeleteObject(bmLogo);
    for (i = 0; i < NS; i++) if (farbe[i]) DeleteObject(farbe[i]);
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
