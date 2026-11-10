/* FLYDMV.C - "Fliegende DMV": eigenes Bildschirmschoner-Modul fuer
   DMVBD und After Dark 1.x fuer Windows 3.0 (Real Mode).
   Eine NCR DMV fliegt mit kleiner Rakete durchs All und stoesst
   5,25-Zoll-Disketten in Regenbogenfarben aus, die hinter ihr
   davontrudeln - oder sie zieht ein flatterndes Banner hinter sich her
   (Einstellung 3: Disketten, Banner oder abwechselnd); dazu funkelnde Sterne auf blauem Himmel, ein Wolken-
   planet und eine laechelnde Sonne. Das Gehaeuse folgt dem grauen
   DMV-Gehaeuse unseres MAME-Layouts (dmv.lay).
   Die Schnittstelle (Module(), Meldungen, AD_SYSTEM/AD_MODULE) folgt der
   veroeffentlichten After-Dark-Modulschnittstelle; der Code ist eigen.
   Code: Claude, Prompts: rfka01 - MIT-Lizenz */

#include <windows.h>
#include "FLYDMV.H"

static HANDLE   hInst;
static HDC      hMem;
static HBITMAP  bmBan[3], bmDmv[2], bmDisk[6][4], bmSun, bmPlanet, bmStar[4], bmOld;
static HBRUSH   hSky;
static int      W, H;               /* Zeichenbereich */
static int      nDisks, speed;
static unsigned long seed;

typedef struct { int x, y, ox, oy, vx, vy, fr, col, on, t; } DISK;
#define MAXDISK 12
static DISK disk[MAXDISK];
static int  dx, dy, odx, ody, phase, flame, eject, nextcol;
static int  ladung, banner, bfr;     /* 0 Disketten, 1 Banner, 2 Wechsel; dieser Flug mit Banner? */
static int  stars;
#define NSTAR 40
#define DMVW 96
#define BANW 224               /* Banner samt Seilen */
#define BANH 40
#define BANDY 8                /* Banner unter der Oberkante der DMV */
#define DMVH 56
#define PLW 176
#define PLH 120
static int  sx[NSTAR], sy[NSTAR];
static BYTE sk[NSTAR];

static int rnd(int n) { seed = seed * 1103515245UL + 12345UL; return (int)((seed >> 16) % (unsigned)n); }

/* Rechteck alt minus neu schwarz loeschen (hoechstens zwei Streifen) */
static void erase(HDC hdc, int ox, int oy, int nx, int ny, int w, int h)
{
    RECT r;
    HBRUSH bl = hSky;
    if (ox < nx) { SetRect(&r, ox, oy, nx, oy + h); FillRect(hdc, &r, bl); }
    if (ox > nx) { SetRect(&r, nx + w, oy, ox + w, oy + h); FillRect(hdc, &r, bl); }
    if (oy < ny) { SetRect(&r, ox, oy, ox + w, ny); FillRect(hdc, &r, bl); }
    if (oy > ny) { SetRect(&r, ox, ny + h, ox + w, oy + h); FillRect(hdc, &r, bl); }
}

/* Positionen immer gerade: dann passt das 2x2-Raster gerasterter Farben
   der Bitmaps (DMV-Farbe und Mono) zum Raster des Hintergrunds */
static void blit(HDC hdc, HBITMAP bm, int x, int y, int w, int h)
{
    SelectObject(hMem, bm);
    BitBlt(hdc, x & ~1, y & ~1, w, h, hMem, 0, 0, SRCCOPY);
}

static void sky(HDC hdc, int x, int y, int w, int h)
{
    RECT r; SetRect(&r, x & ~1, y & ~1, (x & ~1) + w, (y & ~1) + h);
    FillRect(hdc, &r, hSky);
}

static int doInit(LPMODULE m)
{
    static char *dn[6][4] = {
        { "D00", "D01", "D02", "D03" }, { "D10", "D11", "D12", "D13" }, { "D20", "D21", "D22", "D23" },
        { "D30", "D31", "D32", "D33" }, { "D40", "D41", "D42", "D43" }, { "D50", "D51", "D52", "D53" } };
    int c, k;
    W = m->ptRgnSize.x; H = m->ptRgnSize.y;
    nDisks = m->iControlValue[0]; if (nDisks < 1) nDisks = 1; if (nDisks > MAXDISK) nDisks = MAXDISK;
    speed = m->iControlValue[1]; if (speed < 1) speed = 1; if (speed > 10) speed = 10;
    ladung = m->iControlValue[2]; if (ladung < 0 || ladung > 2) ladung = 2;
    bmBan[0] = LoadBitmap(hInst, "BAN0"); bmBan[1] = LoadBitmap(hInst, "BAN1"); bmBan[2] = LoadBitmap(hInst, "BAN2");
    bmDmv[0] = LoadBitmap(hInst, "DMV0"); bmDmv[1] = LoadBitmap(hInst, "DMV1");
    for (c = 0; c < 6; c++) for (k = 0; k < 4; k++) bmDisk[c][k] = LoadBitmap(hInst, dn[c][k]);
    bmSun = LoadBitmap(hInst, "SUN"); bmPlanet = LoadBitmap(hInst, "PLANET");
    bmStar[0] = LoadBitmap(hInst, "STAR0"); bmStar[1] = LoadBitmap(hInst, "STAR1");
    bmStar[2] = LoadBitmap(hInst, "STAR2"); bmStar[3] = LoadBitmap(hInst, "STAR3");
    hSky = CreateSolidBrush(RGB(0, 0, 64));       /* Nachtblau: auf der DMV ein Viertel Blau gerastert */
    if (!bmDmv[0] || !bmPlanet || !bmStar[3] || !hSky) return MEM_ERROR;
    seed = GetCurrentTime();
    return NOERROR;
}

/* neue Flughoehe: zwischen Sonne und Planet, moeglichst weit weg von
   den Disketten, die noch fliegen, und nicht auf der alten Bahn */
static void newDmv(void)
{
    int k, best = -1, bestd = -1, alt = dy;
    for (k = 0; k < 12; k++) {
        int y = 84 + rnd(224 - 84 + 1), i, dmin = 400;
        for (i = 0; i < MAXDISK; i++) if (disk[i].on) {
            int g = disk[i].y > y + DMVH ? disk[i].y - (y + DMVH)
                  : y > disk[i].y + 24 ? y - (disk[i].y + 24) : 0;
            if (g < dmin) dmin = g;
        }
        if (k && (y > alt - 30 && y < alt + 30)) dmin /= 2;   /* lieber eine neue Hoehe */
        if (dmin > bestd) { bestd = dmin; best = y; }
    }
    dx = W + rnd(80); dy = best; phase = rnd(64);
    banner = ladung == 1 || (ladung == 2 && !banner);    /* Wechsel: abwechselnd */
}

static int doBlank(HDC hdc)
{
    RECT r; int i;
    hMem = CreateCompatibleDC(hdc);
    bmOld = SelectObject(hMem, bmSun);
    SetRect(&r, 0, 0, W, H);
    FillRect(hdc, &r, hSky);
    blit(hdc, bmSun, 120, 18, 64, 64);
    blit(hdc, bmPlanet, W - PLW - 20, H - PLH, PLW, PLH);
    /* Sterne: nicht auf Sonne und Planet */
    for (i = 0; i < NSTAR; i++) {
        int x, y;
        do { x = rnd(W - 9); y = rnd(H - 9); }
        while ((x > 110 && x < 190 && y < 90) || (x > W - PLW - 30 && y > H - PLH - 10));
        sx[i] = x; sy[i] = y; sk[i] = (BYTE)rnd(4);
        blit(hdc, bmStar[sk[i]], x, y, 9, 9);
    }
    newDmv(); odx = dx; ody = dy;
    for (i = 0; i < MAXDISK; i++) disk[i].on = 0;
    eject = 0; nextcol = 0; stars = 0;
    return NOERROR;
}

static int doFrame(HDC hdc)
{
    int i, ny, step = 1 + speed / 3;
    /* Sterne funkeln: je Bild zwei Sterne eine Stufe weiter (0 1 2 3 2 1 0 ...) */
    for (i = 0; i < 2; i++) {
        int k = stars++ % NSTAR, f = (sk[k] + 1) & 7;
        sk[k] = (BYTE)f;
        blit(hdc, bmStar[f < 4 ? f : 7 - f], sx[k], sy[k], 9, 9);
    }
    /* Disketten */
    for (i = 0; i < nDisks; i++) {
        DISK *d = &disk[i];
        if (!d->on) continue;
        d->ox = d->x; d->oy = d->y;
        d->x += d->vx; d->y += d->vy;
        if (++d->t >= 3) { d->t = 0; d->fr = (d->fr + 1) & 3; }
        if (d->x > W || d->y < 4 || d->y > H - PLH - 30) {
            sky(hdc, d->ox, d->oy, 24, 24);
            d->on = 0; continue;
        }
        erase(hdc, d->ox & ~1, d->oy & ~1, d->x & ~1, d->y & ~1, 24, 24);
        blit(hdc, bmDisk[d->col][d->fr], d->x, d->y, 24, 24);
    }
    /* DMV: nach links, sanft auf und ab */
    odx = dx; ody = dy;
    dx -= step; phase++;
    ny = dy + ((phase & 32) ? 1 : -1) * ((phase & 7) == 0);
    dy = ny;
    if (dx < -DMVW - (banner ? BANW : 0) - 4) {
        sky(hdc, odx, ody, DMVW, DMVH);
        if (banner) sky(hdc, odx + DMVW, ody + BANDY, BANW, BANH);
        newDmv(); odx = dx; ody = dy;
    }
    /* Diskette ausstossen */
    if (!banner && ++eject >= 18 - speed && dx + DMVW < W - 30 && dx > -40) {
        for (i = 0; i < nDisks; i++) if (!disk[i].on) {
            DISK *d = &disk[i];
            d->on = 1; d->x = dx + DMVW + 2; d->y = dy + 16;   /* hinter der Flamme */
            d->vx = 1 + rnd(2);
            d->vy = rnd(3) - 1; d->fr = 0; d->t = 0;
            d->col = nextcol; nextcol = (nextcol + 1) % 6;
            eject = 0; break;
        }
    }
    flame ^= 1;
    if (banner) {                           /* Banner flattert hinterher */
        if ((phase & 3) == 0) bfr = (bfr + 1) % 3;
        erase(hdc, (odx + DMVW) & ~1, (ody + BANDY) & ~1, (dx + DMVW) & ~1, (dy + BANDY) & ~1, BANW, BANH);
        blit(hdc, bmBan[bfr], dx + DMVW, dy + BANDY, BANW, BANH);
    }
    erase(hdc, odx & ~1, ody & ~1, dx & ~1, dy & ~1, DMVW, DMVH);
    blit(hdc, bmDmv[flame], dx, dy, DMVW, DMVH);
    return NOERROR;
}

static int doClose(void)
{
    int c, k;
    if (hMem) { SelectObject(hMem, bmOld); DeleteDC(hMem); hMem = 0; }
    DeleteObject(bmDmv[0]); DeleteObject(bmDmv[1]);
    for (c = 0; c < 3; c++) DeleteObject(bmBan[c]);
    for (c = 0; c < 6; c++) for (k = 0; k < 4; k++) DeleteObject(bmDisk[c][k]);
    DeleteObject(bmSun); DeleteObject(bmPlanet);
    for (c = 0; c < 4; c++) DeleteObject(bmStar[c]);
    DeleteObject(hSky);
    return NOERROR;
}

int FAR PASCAL Module(int msg, HDC hdc, HANDLE hSystem)
{
    LPSYSTEM sys = (LPSYSTEM)GlobalLock(hSystem);
    LPMODULE mod = (LPMODULE)GlobalLock(sys->hModuleInfo);
    int r = NOERROR;
    switch (msg) {
    case INITIALIZE: r = doInit(mod); break;
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
