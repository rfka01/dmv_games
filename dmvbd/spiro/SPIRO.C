/* SPIRO.C - "Spirograph": eigenes Bildschirmschoner-Modul fuer DMVBD und
   After Dark 1.x fuer Windows 3.0 (Real Mode).
   Ein Rad rollt in einem Zahnkranz (oder aussen um ein Mittelrad), ein
   Stift in einem Loch des Rads zeichnet die Bahn. Jede Bahn ist eine
   Summe umlaufender Zeiger z = Summe A_k * e^(i * t * n_k / q):
     Rad innen:   (R-r) e^(it) + d e^(-i t (R-r)/r)          (Hypotrochoide)
     Rad aussen:  (R+r) e^(it) - d e^( i t (R+r)/r)          (Epitrochoide)
     Rad im Rad:  ein zweites Rad rollt im ersten, drei Zeiger
     Oval, Dreieck: unrunde Raeder - ein weiterer Zeiger mit der doppelten
                  bzw. dreifachen Drehzahl des Rads
   Alle Drehzahlen sind Brueche mit gemeinsamem Nenner q: die Bahn
   schliesst sich nach q Umlaeufen.
   Der Zahnkranz (weiss, Zacken nach innen) bzw. das Mittelrad steht still;
   ein weisses Kreuz (XOR) zeigt das Loch mit dem Stift, dahinter bleibt der
   farbige Strich. Nach einigen Figuren wird der Innenraum geloescht.
   Ganzzahlig: Winkel in 1/1024 Umlauf, Sinustafel.
   Code: Claude, Prompts: rfka01 - MIT-Lizenz */

#include <windows.h>
#include "SPIRO.H"
#include "SINTAB.H"

#define NZ 4                     /* hoechstens 4 Zeiger */

static HANDLE hInst;
static int   W, H, cx, cy, mono;
static int   tempo, figuren, kranz_an, wahl;
static int   blatt;              /* 0 Zahnkranz, 1 Mittelrad */
static int   R, scale;           /* Kranz- bzw. Mittelradradius (Einheiten), Massstab Q8 */
static int   nz, amp[NZ], frq[NZ], q;   /* Zeiger der laufenden Figur */
static long  t, tend;            /* Winkel in 1/1024 Umlauf */
static int   nfig, nloesch, pause;
static int   lx, ly, markok, farbe;
static HPEN  stift[6], weiss;
static unsigned long seed;

static const COLORREF regenbogen[6] = {
    RGB(255, 0, 0), RGB(255, 255, 0), RGB(0, 255, 0),
    RGB(0, 255, 255), RGB(0, 0, 255), RGB(255, 0, 255) };

static int rnd(int n) { seed = seed * 1103515245UL + 12345UL; return n > 0 ? (int)((seed >> 16) % (unsigned)n) : 0; }
static int ggt(int a, int b) { if (a < 0) a = -a; if (b < 0) b = -b; while (b) { int c = a % b; a = b; b = c; } return a; }

static int qsin(long a) { return sintab[(int)(a & 1023)]; }          /* 1.0 = 16384 */
static int qcos(long a) { return sintab[(int)((a + 256) & 1023)]; }

/* Einheiten (mal 16384) -> Bildschirmpunkte; DMV-Punkte hoeher als breit */
static int sxp(long x) { return cx + (int)((x >> 6) * scale >> 16); }
static int syp(long y) { return cy + (int)((y >> 6) * scale * 5L / 6L >> 16); }

/* Stiftpunkt beim Winkel a */
static void bahn(long a, int FAR *px, int FAR *py)
{
    long x = 0, y = 0;
    int k;
    for (k = 0; k < nz; k++) {
        long w = a * frq[k] / q;
        x += (long)amp[k] * qcos(w);
        y += (long)amp[k] * qsin(w);
    }
    *px = sxp(x); *py = syp(y);
}

/* Kreis mit Radius rr (Einheiten) und kleinen Zacken der Tiefe z
   (z < 0: nach innen) */
static void zahnrad(HDC hdc, int rr, int z)
{
    int k, n = rr > 30 ? rr : 30;          /* etwa eine Zacke je Einheit */
    HPEN op = SelectObject(hdc, weiss);
    MoveTo(hdc, sxp((long)rr * 16384L), syp(0));
    for (k = 1; k <= 120; k++) {
        long w = (long)k * 1024 / 120;
        LineTo(hdc, sxp((long)rr * qcos(w)), syp((long)rr * qsin(w)));
    }
    for (k = 0; k < n; k++) {               /* Zacken als kleine Dreiecke */
        long w0 = (long)k * 1024 / n, w1 = w0 + 512 / n, w2 = (long)(k + 1) * 1024 / n;
        MoveTo(hdc, sxp((long)rr * qcos(w0)), syp((long)rr * qsin(w0)));
        LineTo(hdc, sxp((long)(rr + z) * qcos(w1)), syp((long)(rr + z) * qsin(w1)));
        LineTo(hdc, sxp((long)rr * qcos(w2)), syp((long)rr * qsin(w2)));
    }
    SelectObject(hdc, op);
}

static void gestell(HDC hdc)
{
    if (!kranz_an) return;
    if (blatt == 0) zahnrad(hdc, R, -2);    /* Zahnkranz, Zacken nach innen */
    else zahnrad(hdc, R, 2);                /* Mittelrad, Zacken nach aussen */
}

/* Innenraum loeschen: beim Zahnkranz streifenweise innerhalb des Kranzes
   (der Kranz bleibt stehen), beim Mittelrad alles und das Rad neu */
static void innen_loeschen(HDC hdc)
{
    RECT rc;
    HBRUSH bl = GetStockObject(BLACK_BRUSH);
    long rx, ry;
    int y;
    if (blatt == 1 || !kranz_an) {
        SetRect(&rc, 0, 0, W, H); FillRect(hdc, &rc, bl);
        gestell(hdc);
        return;
    }
    rx = ((long)(R - 3) * 256L * scale) >> 16;          /* Halbachsen in Punkten */
    ry = rx * 5L / 6L;
    for (y = (int)-ry; y <= (int)ry; y += 2) {
        long yy = (y < 0 ? -y : y) + 1, v, hw;      /* aeussere der beiden Zeilen */
        if (yy > ry) yy = ry;
        v = rx * rx - rx * rx * yy / ry * yy / ry;  /* (hw)^2 = rx^2 (1 - (y/ry)^2) */
        if (v <= 0) continue;
        for (hw = 0; (hw + 1) * (hw + 1) <= v; hw++) ;
        SetRect(&rc, cx - (int)hw, cy + y, cx + (int)hw + 1, cy + y + 2);
        FillRect(hdc, &rc, bl);
    }
}

static void neues_blatt(void)
{
    int e;
    /* Gemischt: Mittelrad hoechstens jedes zweite Blatt, sonst Zahnkranz */
    if (wahl == 2) blatt = 1;
    else if (wahl) blatt = 0;
    else blatt = blatt ? 0 : (rnd(3) == 0);
    if (blatt == 0) { R = 70 + rnd(50); e = R + 1; }
    else            { R = 18 + rnd(14); e = 3 * R + 4; }    /* Mittelrad klein, Bahn aussen */
    scale = (int)(((long)(H / 2 - 6) * 6L / 5L * 256L) / e);
    if ((long)scale * e / 256 > W / 2 - 6) scale = (int)((long)(W / 2 - 6) * 256L / e);
}

static void neue_figur(void)
{
    int art = wahl ? wahl : 1 + rnd(5), r, d, g;
    if (blatt == 1) art = 2;
    else if (art == 2) art = 1;
    nz = 2;
    switch (art) {
    case 2:                                     /* Rad aussen um das Mittelrad */
        r = R / 3 + rnd(R * 2 / 3 + 1);
        d = r / 3 + rnd(r * 2 / 3 + 1);
        g = ggt(R + r, r);
        q = r / g;
        amp[0] = R + r;  frq[0] = q;
        amp[1] = -d;     frq[1] = (R + r) / g;
        break;
    case 3: {                                   /* Rad im Rad */
        int r2;
        q = 3 + rnd(5);
        frq[0] = q; frq[1] = -(q + 1 + rnd(3 * q));
        while (ggt(frq[1], q) != 1) frq[1]--;
        r = (int)((long)R * q / (q - frq[1]));     /* Rad 1 aus seiner Drehzahl */
        r2 = r / 3 + rnd(r / 3 + 1);
        d = r2 / 2 + rnd(r2 / 3 + 1);            /* bleibt im Kranz */
        amp[0] = R - r; amp[1] = r - r2; amp[2] = d;
        frq[2] = (1 + rnd(3)) * -frq[1] + q;       /* Rad 2 dreht schneller, Gegensinn */
        nz = 3;
        break; }
    default:                                    /* Rad innen; 4 Oval, 5 Dreieck */
        do r = 9 + rnd(R - 16); while (ggt(R, r) > r / 3 && rnd(4));
        d = r / 4 + rnd(r * 2 / 3);
        g = ggt(R, r);
        q = r / g;
        amp[0] = R - r; frq[0] = q;
        amp[1] = d;     frq[1] = -(R - r) / g;
        if (art == 4 || art == 5) {             /* unrundes Rad: Oberschwingung */
            if (d > (r - 2) * 3 / 4) d = (r - 2) * 3 / 4;   /* samt Ausbuchtung im Kranz */
            amp[1] = d;
            amp[2] = d / 4 + 1;
            frq[2] = (art == 4 ? 2 : 3) * frq[1] + (art == 4 ? 1 : 2) * frq[0];
            nz = 3;
        }
        break;
    }
    tend = 1024L * q;
    t = 0;
    farbe = rnd(6);
    bahn(0, &lx, &ly);
}

/* Stift: kleines weisses Kreuz, XOR (zweimal = weg) */
static void marke(HDC hdc, int x, int y)
{
    int old = SetROP2(hdc, R2_XORPEN);
    HPEN op = SelectObject(hdc, weiss);
    MoveTo(hdc, x - 3, y); LineTo(hdc, x + 4, y);
    MoveTo(hdc, x, y - 3); LineTo(hdc, x, y);
    MoveTo(hdc, x, y + 1); LineTo(hdc, x, y + 4);
    SelectObject(hdc, op);
    SetROP2(hdc, old);
}

static int doInit(LPSYSTEM sys, LPMODULE m)
{
    int i;
    W = m->ptRgnSize.x; H = m->ptRgnSize.y; cx = W / 2; cy = H / 2;
    mono = sys->iBitsPerPixel == 1;
    tempo = m->iControlValue[0]; if (tempo < 1) tempo = 1; if (tempo > 10) tempo = 10;
    figuren = m->iControlValue[1]; if (figuren < 1) figuren = 1; if (figuren > 8) figuren = 8;
    kranz_an = m->iControlValue[2] != 0;
    wahl = m->iControlValue[3]; if (wahl < 0 || wahl > 5) wahl = 0;
    for (i = 0; i < 6; i++) {
        stift[i] = CreatePen(PS_SOLID, 1, mono ? RGB(255, 255, 255) : regenbogen[i]);
        if (!stift[i]) return MEM_ERROR;
    }
    weiss = CreatePen(PS_SOLID, 1, RGB(255, 255, 255));
    seed = GetCurrentTime();
    return NOERROR;
}

static int doBlank(HDC hdc)
{
    RECT rc;
    SetRect(&rc, 0, 0, W, H);
    FillRect(hdc, &rc, GetStockObject(BLACK_BRUSH));
    nfig = 0; nloesch = 0; pause = 0; markok = 0;
    blatt = 1;                              /* das erste Blatt: Zahnkranz */
    neues_blatt();
    gestell(hdc);
    neue_figur();
    return NOERROR;
}

static int doFrame(HDC hdc)
{
    int k, n = 4 + tempo * 4;               /* Strecken je Bild */
    HPEN op;
    if (pause) {                            /* fertig: kurz stehen lassen */
        if (--pause == 0) {
            if (++nfig >= figuren) {        /* Innenraum leeren */
                nfig = 0;
                if (++nloesch >= (blatt ? 1 : 3)) {   /* neues Blatt; Mittelrad nur einmal */
                    RECT rc;
                    nloesch = 0;
                    SetRect(&rc, 0, 0, W, H);
                    FillRect(hdc, &rc, GetStockObject(BLACK_BRUSH));
                    neues_blatt();
                    gestell(hdc);
                } else innen_loeschen(hdc);
            }
            neue_figur();
        }
        return NOERROR;
    }
    if (markok) { marke(hdc, lx, ly); markok = 0; }   /* alte Marke weg */
    op = SelectObject(hdc, stift[farbe]);
    MoveTo(hdc, lx, ly);
    for (k = 0; k < n && t < tend; k++) {
        int px, py;
        t += 6;
        if (t > tend) t = tend;
        if ((t & 1023) < 6 && !mono) {          /* je Umlauf naechste Farbe */
            farbe = (farbe + 1) % 6;
            SelectObject(hdc, stift[farbe]);
        }
        bahn(t, &px, &py);
        LineTo(hdc, px, py);
        lx = px; ly = py;
    }
    SelectObject(hdc, op);
    if (t >= tend) { pause = 40; return NOERROR; }
    marke(hdc, lx, ly); markok = 1;
    return NOERROR;
}

static int doClose(void)
{
    int i;
    for (i = 0; i < 6; i++) if (stift[i]) DeleteObject(stift[i]);
    if (weiss) DeleteObject(weiss);
    return NOERROR;
}

/* Verteiler. After Dark schickt auch MODULESELECTED (Auswahl im
   Kontrollfeld): dann gehoeren das eigene Modul-Handle und die Nummern
   der Einstellungen in AD_MODULE, sonst liest After Dark die Einstellungen
   aus dem zuletzt gewaehlten Modul. Auf schnellen Rechnern (ab 80286 oder
   Windows im Standard-/Erweiterten Modus) hoechstens ein Bild je TAKT ms;
   DMVBD meldet 8086 im Real Mode und laeuft ungebremst. */
#define TAKT 50
int FAR PASCAL Module(int msg, HDC hdc, HANDLE hSystem)
{
    static DWORD tlast;
    static int takt;
    LPSYSTEM sys = (LPSYSTEM)GlobalLock(hSystem);
    LPMODULE mod;
    int r = NOERROR, i;
    if (!sys) return NOERROR;
    if ((mod = (LPMODULE)GlobalLock(sys->hModuleInfo)) != NULL) {
        switch (msg) {
        case INITIALIZE:
            takt = sys->iProcessorType >= 2 || sys->iOperatingMode != 0;
            tlast = GetCurrentTime();
            r = doInit(sys, mod); break;
        case BLANK:      r = doBlank(hdc); break;
        case DRAWFRAME:
            if (takt) {
                DWORD t = GetCurrentTime();
                if (t - tlast < TAKT) break;
                tlast = t;
            }
            r = doFrame(hdc); break;
        case CLOSE:      r = doClose(); break;
        case MODULESELECTED:
            mod->hModule = hInst;
            for (i = 0; i < 4; i++) mod->iControlID[i] = i + 1;
            break;
        }
        GlobalUnlock(sys->hModuleInfo);
    }
    GlobalUnlock(hSystem);
    return r;
}

int PASCAL LibMain(HANDLE hInstance, WORD wDataSeg, WORD wHeapSize, LPSTR lpCmdLine)
{
    hInst = hInstance;
    return 1;
}

int FAR PASCAL WEP(int bSystemExit) { return 1; }
