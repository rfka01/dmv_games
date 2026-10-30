/* MAHJONGG.C - Mahjongg-Solitaer fuer die NCR Decision Mate V.
 *
 *   MAHJONGG [spielnummer] [steine.til]
 *
 * Steinsaetze im TIL-Format von Nels Anderson (42 Steine, 40 x 40 Punkte,
 * 16 EGA-Farben), Vorgabe DMVSTEIN.TIL. Bedienung mit der Maus K806 oder
 * mit den Cursortasten.
 *
 * Grafik, Maus und Ton ueber DMVGRAF (rfka01/dmv_goodies, dmvgraf/):
 * die Steine sind Bilder im Bildspeicher der Bibliothek (GBLOAD), jeder
 * geaenderte Bereich wird mit GLAYER/GLPUT/GLDRAW von hinten nach vorn
 * zusammengesetzt; der gewaehlte Stein ist schattiert (GB_SCHAT).
 *
 * K&R-C; int hat 16 Bit; externe Namen in 7 Zeichen eindeutig.
 * Code: Claude (Anthropic), Idee und Prompts: rfka01. Lizenz: MIT
 */
#include "DMVGRAF.H"
#include "mj.h"

#define NST     144             /* Steine im Aufbau */
#define NART    42              /* Steinbilder in der TIL-Datei */
#define STW     40              /* Steinbild: Breite, Hoehe */
#define LAGEX   4               /* Versatz je Lage nach rechts (4er-Raster) */
#define LAGEY   4               /* Versatz je Lage nach oben */
#define STATY   384             /* Zeile mit Tasten und Meldungen */
#define KOPFY   16              /* darueber: Zeile mit Spiel, Steinen, Paaren */
#define ZEIGER  NART          /* Bildnummer des Mauszeigers */

static int hwart;               /* HW_FARBE | HW_MAUS */
static int fx0, fx1, fy0, fy1;  /* Steinflaeche, fuer Treffer */
static int hx, hy, ox, oy;      /* ganzer Steinabstand, Brettecke */

/* Aufbau (Schildkroete) in halben Steinen */
static char pz[NST], pc[NST], pr[NST];
static int tpx[NST], tpy[NST];  /* Bildschirmlage (linke obere Ecke) */
static int ord[NST];            /* Zeichenreihenfolge */
static int typ[NST];            /* Steinart 0-41, -1 = abgeraeumt */
static char mark[NST];          /* schattiert */

static int sel = -1;            /* gewaehlter Stein */
static UW spiel;                /* Spielnummer */
static int rest;                /* Steine auf dem Brett */
static char ua[72], ub[72], uta[72], utb[72];       /* Rueckgaengig */
static int un;
static int flist[NST];          /* Arbeitsliste freier Steine */
#define fl flist

static int mx = 320, my = 200;  /* Zeiger (Spitze) */
static char stat[82];           /* Statuszeile */

/* ------------------------------------------------------------------ */
/* Zufall: 16-Bit-Xorshift, auf allen Rechnern gleich                  */

static UW rnd_s = 1;

static int rnd()
{
        UW x;

        x = rnd_s;
        x = M16(x ^ (x << 7));
        x = M16(x ^ (x >> 9));
        x = M16(x ^ (x << 8));
        rnd_s = x;
        return (int)(x & 0x7FFF);
}

static int rndn(n)
int n;
{
        return rnd() % n;
}

/* ------------------------------------------------------------------ */
/* Aufbau                                                             */

static int nlay;

static int setz(z, c2, r2)
int z, c2, r2;
{
        pz[nlay] = z;
        pc[nlay] = c2;
        pr[nlay] = r2;
        nlay++;
        return 0;
}

static int nachb();

static int aufbau()
{
        static char von[8] = { 1, 3, 2, 1, 1, 2, 3, 1 };
        static char bis[8] = { 12, 10, 11, 12, 12, 11, 10, 12 };
        int r, c, z, i, j, t, a, b;

        nlay = 0;
        for (r = 0; r < 8; r++)
                for (c = von[r]; c <= bis[r]; c++)
                        setz(0, c * 2, r * 2);
        setz(0, 0, 7);
        setz(0, 26, 7);
        setz(0, 28, 7);
        for (z = 1; z <= 3; z++)
                for (r = z; r <= 7 - z; r++)
                        for (c = 3 + z; c <= 10 - z; c++)
                                setz(z, c * 2, r * 2);
        setz(4, 13, 7);
        /* Wir sehen von oben, etwas von links vorn auf den Turm: die
           Steine einer Lage liegen dicht an dicht,
           linke und vordere Kante eines Steins sind nur zu sehen, wo
           daneben bzw. davor auf derselben Lage nichts liegt. Hoehere
           Lagen sind um LAGEX nach rechts und LAGEY nach oben versetzt.
           Brett mittig. */
        /* Abstand = Flaeche samt einer Rahmenlinie: benachbarte Steine
           teilen sich die schwarze Kante, keiner deckt die Flaeche des
           anderen */
        hx = fx1 - fx0 + 2;
        if (hx < 24 || hx > 40) hx = 32;
        hy = fy1 - fy0 + 2;
        if (hy < 24 || hy > 40) hy = 34;
        ox = ((640 - (14 * hx + STW + 4 * LAGEX)) / 2) & ~7;
        oy = KOPFY + LAGEY * 4
             + (STATY - KOPFY - (7 * hy + STW + LAGEY * 4)) / 2;
        for (i = 0; i < NST; i++) {
                tpx[i] = ox + (pc[i] * hx) / 2 + pz[i] * LAGEX;
                tpy[i] = oy + (pr[i] * hy) / 2 - pz[i] * LAGEY;
                ord[i] = i;
        }
        nachb();
        /* Reihenfolge: Lage, dann Spalte von rechts, dann von oben. Die
           Kanten liegen links und vorn (unten) am Stein; der linke
           Nachbar und der Stein davor werden danach gezeichnet und
           decken sie zu, auch bei den halb versetzten Endsteinen */
        for (i = 1; i < NST; i++) {
                t = ord[i];
                a = pz[t] * 1024 + (31 - pc[t]) * 32 + pr[t];
                for (j = i - 1; j >= 0; j--) {
                        b = pz[ord[j]] * 1024 + (31 - pc[ord[j]]) * 32
                            + pr[ord[j]];
                        if (b <= a)
                                break;
                        ord[j + 1] = ord[j];
                }
                ord[j + 1] = t;
        }
        return 0;
}

static int iabs(a)
int a;
{
        return a < 0 ? -a : a;
}

/* Nachbarn, einmal in aufbau() berechnet: bis zu vier Steine darueber,
   je zwei links und rechts; -1 = keiner. Damit ist "frei" billig, was
   auf dem Z8001 beim Mischen den Unterschied macht. */
static char nob[NST][4], nli[NST][2], nre[NST][2];   /* FFh = keiner */
#define KEIN(x) (((x) & 0xFF) == 0xFF)

static int nachb()
{
        register int j, dr, dc, zi, ci;
        int i, a, l, r, ri;

        for (i = 0; i < NST; i++) {
                for (a = 0; a < 4; a++)
                        nob[i][a] = -1;
                nli[i][0] = nli[i][1] = nre[i][0] = nre[i][1] = -1;
                a = l = r = 0;
                zi = pz[i];
                ci = pc[i];
                ri = pr[i];
                for (j = 0; j < NST; j++) {
                        dr = pr[j] - ri;
                        if (dr >= 2 || dr <= -2)
                                continue;
                        dc = pc[j] - ci;
                        if (pz[j] == zi + 1) {
                                if (dc < 2 && dc > -2 && a < 4)
                                        nob[i][a++] = j;
                        } else if (pz[j] == zi) {
                                if (dc == -2 && l < 2)
                                        nli[i][l++] = j;
                                if (dc == 2 && r < 2)
                                        nre[i][r++] = j;
                        }
                }
        }
        return 0;
}

/* Stein i ist frei, wenn nichts darauf liegt und eine Seite offen ist */
static int istfr(i, da)
int i;
char *da;
{
        int k, l, r;

        if (!da[i])
                return 0;
        for (k = 0; k < 4; k++)
                if (!KEIN(nob[i][k]) && da[nob[i][k] & 0xFF])
                        return 0;
        l = r = 0;
        for (k = 0; k < 2; k++) {
                if (!KEIN(nli[i][k]) && da[nli[i][k] & 0xFF])
                        l = 1;
                if (!KEIN(nre[i][k]) && da[nre[i][k] & 0xFF])
                        r = 1;
        }
        return !l || !r;
}

static char da[NST];            /* Stein liegt noch */

static int dasetz()
{
        int i;

        for (i = 0; i < NST; i++)
                da[i] = typ[i] >= 0;
        return 0;
}

static int frei(i)
int i;
{
        return istfr(i, da);
}

static int passt(a, b)
int a, b;
{
        if (a == b)
                return 1;
        if (a >= 34 && a <= 37 && b >= 34 && b <= 37)
                return 1;               /* Jahreszeiten */
        if (a >= 38 && b >= 38)
                return 1;               /* Blumen */
        return 0;
}

/* Mischen: die Paare werden in einer Reihenfolge abgeraeumt, die das
   Spiel loest, und rueckwaerts verteilt. Jedes Spiel ist also loesbar. */
static int mische(nr)
UW nr;
{
        static int pa[72], pb[72];
        int i, k, n, a, b, t, ok, vers;
        char weg[NST];

        rnd_s = nr ? nr : 1;
        for (i = 0; i < 20; i++)
                rnd();
        k = 0;
        for (i = 0; i < 34; i++) {
                pa[k] = i; pb[k] = i; k++;
                pa[k] = i; pb[k] = i; k++;
        }
        for (t = 34; t <= 38; t += 4) {
                int s[4];
                for (i = 0; i < 4; i++)
                        s[i] = t + i;
                for (i = 3; i > 0; i--) {
                        n = rndn(i + 1);
                        a = s[i]; s[i] = s[n]; s[n] = a;
                }
                pa[k] = s[0]; pb[k] = s[1]; k++;
                pa[k] = s[2]; pb[k] = s[3]; k++;
        }
        for (i = 71; i > 0; i--) {
                n = rndn(i + 1);
                a = pa[i]; pa[i] = pa[n]; pa[n] = a;
                a = pb[i]; pb[i] = pb[n]; pb[n] = a;
        }
        for (vers = 0; vers < 50; vers++) {
                for (i = 0; i < NST; i++)
                        weg[i] = 1;
                ok = 1;
                for (k = 0; k < 72 && ok; k++) {
                        n = 0;
                        for (i = 0; i < NST; i++)
                                if (istfr(i, weg))
                                        fl[n++] = i;
                        if (n < 2) {
                                ok = 0;
                                break;
                        }
                        a = rndn(n);
                        b = rndn(n - 1);
                        if (b >= a)
                                b++;
                        typ[fl[a]] = pa[k];
                        typ[fl[b]] = pb[k];
                        weg[fl[a]] = 0;
                        weg[fl[b]] = 0;
                }
                if (ok)
                        break;
        }
        for (i = 0; i < NST; i++)
                mark[i] = 0;
        sel = -1;
        un = 0;
        rest = NST;
        dasetz();
        return 0;
}

/* Zahl freier Paare */
static int paare()
{
        int i, j, n;
        int k;

        k = 0;
        for (i = 0; i < NST; i++)
                if (typ[i] >= 0 && frei(i))
                        fl[k++] = i;
        n = 0;
        for (i = 0; i < k; i++)
                for (j = i + 1; j < k; j++)
                        if (passt(typ[fl[i]], typ[fl[j]]))
                                n++;
        return n;
}

/* ------------------------------------------------------------------ */
/* Steine laden, Zeiger                                               */

/* Zeiger 16 x 16 im DMVGRAF-Format 4: je Zeile Maske, Gruen, Rot, Blau
   (je 2 Byte, Bit 0 = links); Kern schwarz, Rand weiss */
static UW zkern[16] = {
        0x0000, 0x0002, 0x0006, 0x000E, 0x001E, 0x003E, 0x007E, 0x00FE,
        0x01FE, 0x003E, 0x0036, 0x0062, 0x0060, 0x00C0, 0x00C0, 0x0000 };
static UW zrand[16] = {
        0x0003, 0x0005, 0x0009, 0x0011, 0x0021, 0x0041, 0x0081, 0x0101,
        0x0201, 0x03C1, 0x0049, 0x0095, 0x0091, 0x0120, 0x0120, 0x00C0 };

static int lade(name)
char *name;
{
        static char zb[16 * 8];
        int r[6], y, i, k;

        /* Farbe 0 durchsichtig; je Zeile vom ersten bis zum letzten
           sichtbaren Punkt gefuellt, ein Punkt schwarzer Rand. Die
           uebrigen Farben wie die Vorgabe von DMVGRAF (hellgrau weiss,
           dunkelgrau Raster schwarz/blau bzw. mono, braun Raster gelb/rot) */
        gbmap(0, -1, 0, 2);
        /* 12 (dunkelgruen, gruener Drache): Raster gruen/schwarz; mono
           bleibt es schwarz wie Gruen */
        if (hwart & HW_FARBE)
                gbmap(12, 2, 0, 1);
        k = gbload(0, name, STW, STW, NART, GB_NIB, 0);
        if (k)
                return k == GE_DATEI ? 1 : k == GE_KURZ ? 2 : 3;
        /* Steinflaeche: sichtbarer Bereich ohne die Kanten links und unten */
        gbinfo(0, r);
        fx0 = r[2] + LAGEX;
        fy0 = r[3];
        fx1 = r[4];
        fy1 = r[5] - LAGEY;
        for (y = 0; y < 16; y++)
                for (i = 0; i < 2; i++) {
                        k = y * 8 + i;
                        zb[k] = ((zkern[y] | zrand[y]) >> (8 * i)) & 0xFF;
                        zb[k + 2] = zb[k + 4] = zb[k + 6]
                                = (zrand[y] >> (8 * i)) & 0xFF;
                }
        gbdef(ZEIGER, 16, 16, GB_EBN, zb);
        return 0;
}

/* ------------------------------------------------------------------ */
/* Zeichnen                                                           */

/* Bereich x0 <= x < x1, y0 <= y < y1 des Bretts neu zeichnen */
static int zrect(x0, y0, x1, y1)
int x0, y0, x1, y1;
{
        register int k, i;
        int wx0, wx1;

        if (y1 > STATY)
                y1 = STATY;
        if (y0 < KOPFY)
                y0 = KOPFY;
        if (x0 >= x1 || y0 >= y1)
                return 0;
        glayer(x0, y0, x1 - 1, y1 - 1);
        /* selbst vorsortieren (DMVGRAF rundet waagrecht auf Worte):
           spart je Stein einen Aufruf */
        wx0 = x0 & ~15;
        wx1 = ((x1 - 1) | 15) + 1;
        for (k = 0; k < NST; k++) {
                i = ord[k];
                if (typ[i] < 0 || tpx[i] >= wx1 || tpx[i] + STW <= wx0
                    || tpy[i] >= y1 || tpy[i] + STW <= y0)
                        continue;
                glput(typ[i], tpx[i], tpy[i],
                      mark[i] ? GB_SCHAT : GB_DURCH);
        }
        gldraw();
        return 0;
}

static int zstein(i)
int i;
{
        return zrect(tpx[i], tpy[i], tpx[i] + STW, tpy[i] + STW);
}

/* ------------------------------------------------------------------ */
/* Statuszeile                                                        */

static int sapp(s)
char *s;
{
        int n;

        for (n = 0; stat[n]; n++)
                ;
        while (*s && n < 80)
                stat[n++] = *s++;
        stat[n] = 0;
        return 0;
}

static int napp(v)
UW v;
{
        char b[8];
        int i;

        i = 7;
        b[i] = 0;
        do {
                b[--i] = '0' + (int)(v % 10);
                v /= 10;
        } while (v && i > 0);
        return sapp(&b[i]);
}

static int zeile(y)              /* stat mittig auf 80 Zeichen, ersetzend */
int y;
{
        int n, l, k;

        for (n = 0; stat[n]; n++)
                ;
        l = n < 80 ? (80 - n) / 2 : 0;
        for (k = n; k >= 0; k--)
                stat[k + l] = stat[k];
        for (k = 0; k < l; k++)
                stat[k] = ' ';
        n += l;
        while (n < 80)
                stat[n++] = ' ';
        stat[80] = 0;
        gmode(0);
        gcolor(7);
        gpoint(0, y);
        gtext(stat, 0);
        return 0;
}

/* oben Spiel, Steine, Paare; unten die Tasten oder eine Meldung */
static int status(msg)
char *msg;
{
        stat[0] = 0;
        sapp("Spiel ");
        napp(spiel);
        sapp("    Steine ");
        napp((UW)rest);
        sapp("    Paare ");
        napp((UW)paare());
        zeile(0);
        stat[0] = 0;
        sapp(msg ? msg
             : "H Hinweis  U Zur\201ck  N Neu  R Nochmal  ? Hilfe  ESC Ende");
        zeile(STATY);
        return 0;
}

/* ------------------------------------------------------------------ */
/* Spiel                                                              */

static int warte(n)
int n;
{
        gwait(n);
        return 0;
}

static int treffer(x, y)        /* oberster Stein unter dem Punkt */
int x, y;
{
        int k, i;

        for (k = NST - 1; k >= 0; k--) {
                i = ord[k];
                if (typ[i] < 0)
                        continue;
                if (x >= tpx[i] + fx0 && x <= tpx[i] + fx1
                    && y >= tpy[i] + fy0 && y <= tpy[i] + fy1)
                        return i;
        }
        return -1;
}

static int zeiger(x, y)         /* Zeiger versetzen (DMVGRAF sichert
                                   den Hintergrund) */
int x, y;
{
        if (x < 0) x = 0;
        if (x > 639) x = 639;
        if (y < 0) y = 0;
        if (y > STATY - 16) y = STATY - 16;     /* Zeiger ueber der Statuszeile */
        if (y < KOPFY) y = KOPFY;               /* und unter der Kopfzeile */
        mx = x;
        my = y;
        gzeig(ZEIGER, mx, my);
        return 0;
}

static int abraeu(a, b)
int a, b;
{
        ua[un] = a; ub[un] = b;
        uta[un] = typ[a]; utb[un] = typ[b];
        un++;
        typ[a] = -1;
        typ[b] = -1;
        mark[a] = mark[b] = 0;
        da[a] = da[b] = 0;
        rest -= 2;
        zstein(a);
        zstein(b);
        return 0;
}

static int tonw(h, l)           /* Ton, notfalls warten, bis der 8741 frei ist */
int h, l;
{
        int n;

        for (n = 0; n < 100 && ton(h, l) == 1; n++)
                gwait(1);
        return 0;
}

static int melodie(gew)
int gew;
{
        if (gew) {
                tonw(25, 8); tonw(29, 8); tonw(32, 8); tonw(37, 20);
        } else
                tonw(13, 15);
        return 0;
}

static int lage()               /* Ende des Spiels? */
{
        if (rest == 0) {
                status("Gewonnen!   N Neues Spiel  R Nochmal  ESC Ende");
                melodie(1);
        } else if (paare() == 0) {
                status("Keine freien Paare mehr.  U Zur\201ck  R Nochmal  N Neu");
                melodie(0);
        } else
                status((char *)0);
        return 0;
}

static int klick(x, y)
int x, y;
{
        int i, a;

        i = treffer(x, y);
        if (i < 0) {
                if (sel >= 0) {
                        a = sel;
                        sel = -1;
                        mark[a] = 0;
                        zstein(a);
                }
                return 0;
        }
        if (!frei(i)) {
                ton(5, 3);
                return 0;
        }
        if (sel == i) {
                sel = -1;
                mark[i] = 0;
                zstein(i);
                return 0;
        }
        if (sel >= 0 && passt(typ[sel], typ[i])) {
                a = sel;
                sel = -1;
                abraeu(a, i);
                ton(30, 2);
                lage();
                return 0;
        }
        if (sel >= 0) {
                a = sel;
                mark[a] = 0;
                zstein(a);
        }
        sel = i;
        mark[i] = 1;
        zstein(i);
        return 0;
}

static int hstart;

static int hinweis(selbst)      /* selbst: Paar gleich abraeumen (Test) */
int selbst;
{
        int i, j, k, n, a, b;

        k = 0;
        for (i = 0; i < NST; i++)
                if (typ[i] >= 0 && frei(i))
                        fl[k++] = i;
        n = 0;
        a = b = -1;
        for (i = 0; i < k && a < 0; i++) {
                int ii = (i + hstart) % (k ? k : 1);
                for (j = 0; j < k; j++)
                        if (j != ii && passt(typ[fl[ii]], typ[fl[j]])) {
                                a = fl[ii];
                                b = fl[j];
                                break;
                        }
        }
        hstart++;
        if (a < 0) {
                ton(13, 10);
                return 0;
        }
        if (sel >= 0) {
                mark[sel] = 0;
                zstein(sel);
                sel = -1;
        }
        if (selbst) {
                abraeu(a, b);
                lage();
                return 0;
        }
        for (n = 0; n < 3; n++) {
                mark[a] = mark[b] = 1;
                zstein(a); zstein(b);
                warte(15);
                mark[a] = mark[b] = 0;
                zstein(a); zstein(b);
                warte(8);
        }
        return 0;
}

static int zurueck()
{
        int a, b;

        if (un == 0)
                return 0;
        if (sel >= 0) {
                mark[sel] = 0;
                zstein(sel);
                sel = -1;
        }
        un--;
        a = ua[un] & 0xFF; b = ub[un] & 0xFF;
        typ[a] = uta[un];
        typ[b] = utb[un];
        da[a] = da[b] = 1;
        rest += 2;
        zstein(a);
        zstein(b);
        lage();
        return 0;
}

/* naechster freier Stein in Richtung dx, dy vom Zeiger aus */
static int springe(dx, dy)
int dx, dy;
{
        int i, cx, cy, ex, ey, d, best, bi;

        best = 32000;
        bi = -1;
        cx = mx;
        cy = my;
        for (i = 0; i < NST; i++) {
                if (typ[i] < 0 || !frei(i))
                        continue;
                ex = tpx[i] + 22 - cx;
                ey = tpy[i] + 17 - cy;
                if (dx && ex * dx <= 4)
                        continue;
                if (dy && ey * dy <= 4)
                        continue;
                d = dx ? iabs(ex) + 3 * iabs(ey) : iabs(ey) + 3 * iabs(ex);
                if (d < best) {
                        best = d;
                        bi = i;
                }
        }
        if (bi >= 0)
                zeiger(tpx[bi] + 22, tpy[bi] + 17);
        return 0;
}

static int bild()               /* ganzes Brett zeichnen */
{
        zrect(0, 0, 640, STATY);
        lage();
        return 0;
}

static int neu(nr)
UW nr;
{
        spiel = nr;
        status("Mische ...");
        mische(nr);
        bild();
        return 0;
}

/* ------------------------------------------------------------------ */
/* Hilfe: welche Steine ein Paar bilden                               */

#define HX0 176                 /* erste Steinspalte */
#define HDX 44                  /* Abstand der Steine */

static int htext(x, y, t, c)
int x, y, c;
char *t;
{
        gcolor(c);
        gpoint(x, y);
        gtext(t, 0);
        return 0;
}

/* Klammer unter vier Steinen ab Spalte 0: alle passen zueinander */
static int hklam(y)
int y;
{
        int k, x;

        gcolor(6);
        gpoint(HX0 + 20, y);
        gline(HX0 + 3 * HDX + 20, y);
        for (k = 0; k < 4; k++) {
                x = HX0 + k * HDX + 20;
                gpoint(x, y);
                gline(x, y - 4);
        }
        return 0;
}

static int hilfe()
{
        static char reihe[4][9] = {
                { 0, 1, 2, 3, 4, 5, 6, 7, 8 },
                { 10, 11, 12, 13, 14, 15, 16, 17, 18 },
                { 20, 21, 22, 23, 24, 25, 26, 27, 28 },
                { 30, 31, 32, 33, -1, 29, 19, 9, -1 } };
        static char *name[4] = { "Kreise 1-9", "Bambus 1-9", "Zeichen 1-9",
                                 "Winde, Drachen" };
        int r, k, t, y, x, b, alt;

        glayer(0, 0, 639, STATY - 1);
        for (r = 0; r < 4; r++)
                for (k = 0; k < 9; k++)
                        if ((t = reihe[r][k]) >= 0)
                                glput(t, HX0 + k * HDX, 24 + r * 44, GB_DURCH);
        for (k = 0; k < 4; k++) {
                glput(34 + k, HX0 + k * HDX, 206, GB_DURCH);
                glput(38 + k, HX0 + k * HDX, 264, GB_DURCH);
        }
        /* Beispiele: gleiche Steine, zwei Jahreszeiten, keine Paar */
        glput(4, 16, 330, GB_DURCH);
        glput(4, 56, 330, GB_DURCH);
        glput(34, 224, 330, GB_DURCH);
        glput(37, 264, 330, GB_DURCH);
        glput(34, 432, 330, GB_DURCH);
        glput(38, 472, 330, GB_DURCH);
        gldraw();
        stat[0] = 0;                    /* nur die untere Zeile */
        sapp("Hilfe - weiter mit einer Taste oder einem Mausknopf");
        zeile(STATY);
        gmode(3);
        htext(8, 4, "Welche Steine bilden ein Paar?", 7);
        htext(264, 4, "Zwei gleiche (jeder Stein viermal)", 3);
        for (r = 0; r < 4; r++)
                htext(8, 36 + r * 44, name[r], 7);
        htext(8, 218, "Jahreszeiten", 7);
        htext(8, 276, "Blumen", 7);
        htext(HX0 + 4 * HDX + 8, 218, "je einmal, jede passt", 6);
        htext(HX0 + 4 * HDX + 8, 234, "zu jeder Jahreszeit", 6);
        htext(HX0 + 4 * HDX + 8, 276, "je einmal, jede passt", 6);
        htext(HX0 + 4 * HDX + 8, 292, "zu jeder Blume", 6);
        hklam(250);
        hklam(308);
        htext(104, 342, "Paar", 2);
        htext(312, 342, "Paar", 2);
        htext(520, 342, "kein Paar", 4);
        gcolor(4);                      /* Kreuz ueber dem falschen Paar */
        if (!(hwart & HW_FARBE))
                gmode(2);               /* mono: schwarz auf hellem Stein */
        gpoint(436, 332);
        gline(508, 366);
        gpoint(436, 366);
        gline(508, 332);
        alt = 7;
        for (;;) {
                if ((hwart & HW_MAUS) && mpos(&x, &y, &b)) {
                        if (b & ~alt)
                                break;
                        alt = b;
                }
                if (gkey())
                        break;
                gwait(1);
        }
        while ((hwart & HW_MAUS) && mpos(&x, &y, &b) && b)
                gwait(1);
        bild();
        return 0;
}

static UW zufall;               /* laeuft beim Warten mit */

static int spielen()
{
        int k, x, y, b, alt, lx, ly, bewegt;

        alt = 0;
        lx = ly = -1;
        gzeig(ZEIGER, mx, my);
        for (;;) {
                zufall++;
                bewegt = 0;
                if (hwart & HW_MAUS) {
                        if (mpos(&x, &y, &b)) {
                                if (x != lx || y != ly) {
                                        zeiger(x, y);
                                        bewegt = 1;
                                }
                                lx = x;
                                ly = y;
                                if ((b & 1) && !(alt & 1))
                                        klick(mx, my);
                                if ((b & 2) && !(alt & 2))
                                        hinweis(0);
                                alt = b;
                        }
                }
                k = gkey();
                if (k >= 'a' && k <= 'z')
                        k -= 32;
                switch (k) {
                case 0:
                        if (!bewegt)    /* nur warten, wenn nichts los ist */
                                gwait(1);
                        break;
                case K_LINKS:  springe(-1, 0); break;
                case K_RECHTS: springe(1, 0); break;
                case K_HOCH:   springe(0, -1); break;
                case K_RUNTER: springe(0, 1); break;
                case ' ':
                case K_CR:     klick(mx, my); break;
                case 'H':      hinweis(0); break;
                case '?':      hilfe(); break;
                case 'U':      zurueck(); break;
                case 0x04:     hinweis(1); break;   /* Ctrl-D: Test */
                case 'R':      neu(spiel); break;
                case 'N':
                        neu((UW)((zufall * 31421 + spiel * 6927) % 65000
                                 + 1));
                        break;
                case K_ESC:
                case 'Q':
                        return 0;
                }
        }
}

static int zahl(s)              /* Spielnummer oder -1 */
char *s;
{
        long v;

        v = 0;
        if (!*s)
                return -1;
        while (*s) {
                if (*s < '0' || *s > '9')
                        return -1;
                v = v * 10 + (*s++ - '0');
                if (v > 65000L)
                        return -1;
        }
        return (int)v;
}

int main(argc, argv)
int argc;
char **argv;
{
        char *datei;
        UW nr;
        int i, v, r, f, m, sy, cv, bios;

        datei = "DMVSTEIN.TIL";
        nr = 0;
        bios = 0;
        for (i = 1; i < argc; i++) {
                if (argv[i][0] == '/' && (argv[i][1] == 'B'
                                          || argv[i][1] == 'b')) {
                        bios = 1;       /* GDC nur ueber BIOS 66/67 */
                        continue;
                }
                v = zahl(argv[i]);
                if (v > 0)
                        nr = (UW)v;
                else
                        datei = argv[i];
        }
        gputs("DMV-Mahjongg 0.10 - lade Steine aus ");
        gputs(datei);
        gputs(" ...\n");
        if (ginfo(&f, &m, &sy, &cv)) {
                gputs("Kein DMV-BIOS mit Grafikfunktionen gefunden.\n");
                return 1;
        }
        hwart = (f ? HW_FARBE : 0) | (m ? HW_MAUS : 0);
        if (bios)
                gdirekt(0);
        r = lade(datei);
        if (r) {
                gputs(r == 1 ? "Datei nicht gefunden: "
                      : r == 2 ? "Datei zu kurz (kein TIL-Satz): "
                      : "Kein Bildspeicher (CP/M-8000 ab v015 mit K208): ");
                gputs(datei);
                gputs("\n");
                return 1;
        }
        if (nr == 0)                    /* ohne Nummer: zufaellig */
                nr = (UW)(grnd() % 65000 + 1);
        gputs("Mische Spiel ");
        stat[0] = 0;
        napp(nr);
        gputs(stat);
        gputs(" ...\n");
        aufbau();
        spiel = nr;
        mische(nr);
        ginit();
        gorig(1);                       /* y von oben wie der Bildspeicher */
        rest = NST;
        status("Das Brett wird aufgebaut ...");
        bild();
        spielen();
        gexit();
        gputs("Auf Wiedersehen.\n");
        return 0;
}
