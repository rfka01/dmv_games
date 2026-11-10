/* BD.C - DMVBD "Before Dawn": Abspieler fuer After-Dark-kompatible
   Bildschirmschoner-Module (Windows 3.0 Real Mode) auf der NCR DMV.

   Aufruf: DMVBD modul.ad [/M | /C] [/N] [/1=n /2=n /3=n /4=n]
     /M Mono, /C Farbe (sonst Kennbuchstabe des ROM: C oder M)
     /N ohne Startschirm
     /1.. /4  Einstellungen des Moduls (iControlValue[0..3])
   Eine Taste beendet.
   Code: Claude, Prompts: rfka01 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <conio.h>
#include <dos.h>
#include "BD.H"

extern void w16_report(void);
extern void w16_done(void);
extern void w16_localbase(WORD off);

/* LibEntry: DS = DGROUP, DI = hInstance, CX = Heap, ES:SI = 0 */
extern WORD callentry(WORD off, WORD seg, WORD dgroup, WORD heap, WORD hinst);
#pragma aux callentry = \
    "push bp" "push ds" "push es" \
    "push dx" "push ax" \
    "mov di,si" "xor si,si" "mov es,si" \
    "mov ds,bx" "mov bp,sp" \
    "call dword ptr ss:[bp]" \
    "add sp,4" "pop es" "pop ds" "pop bp" \
    parm [ax] [dx] [bx] [cx] [si] value [ax] modify [bx cx dx si di];

typedef short (__far __pascal *MODFN)(short msg, HDC hdc, HANDLE hsys);

/* After-Dark-Strukturen (MODULE.H des Entwicklerpakets) */
#pragma pack(1)
typedef struct {
    short iOperatingMode, iProcessorType, bCoProcessor;
    POINT ptScreenSize;
    short iBitsPerPixel;
    POINT ptAspect, ptDotsPerInch;
    short iADVersion;
    RECT  rDemoRect;
    short iSaverMode;
    HANDLE hModuleInfo;
    LPSTR lpszErrorMessage;
    WORD  hADWnd;
    short iRunner;
} AD_SYSTEM;
typedef struct {
    WORD  hDrawRgn;
    POINT ptRgnSize;
    short iControlValue[4];
    short iControlID[4];
    HANDLE hModule;
    WORD  hPalette;
    LPVOID lpLogPalette;
} AD_MODULE;
#pragma pack()

static int romletter(void)
{
    /* ROM ab 0000h einblenden (OUT 11h), Kennbuchstabe 0FF9h, OUT 10h */
    BYTE c;
    _disable();
    outp(0x11, 0);
    c = *(BYTE FAR *)MK_FP(0, 0x0FF9);
    outp(0x10, 0);
    _enable();
    return c;
}

static DWORD now_ms(void)
{
    union REGS r;
    r.h.ah = 0x2C; intdos(&r, &r);
    return ((DWORD)r.h.ch * 3600L + r.h.cl * 60L + r.h.dh) * 1000L + r.h.dl * 10L;
}

/* ---- Startschirm: DMVBD.PIC (DMVGRAF-Format, LZSS) aus dem Verzeichnis
   von DMVBD.EXE, dazu ein zwinkernder Stern. Taste oder 8 s: weiter. */
#include "STERNBG.H"
extern int __cdecl gpic_datei(const char *name, int art, int xw, int y, char *arbeit);
#define ST_X 528                         /* Mitte des Sterns, wie mktitel.py */
#define ST_Y 34
#define ST_N 21

static void stern(LPBITMAP scr, int mono, int k)
{
    static BYTE bits[3 * 4 * ST_N];
    static const BYTE arm[6] = { 0, 2, 4, 7, 10, 4 };
    BITMAP bm;
    int x, y, c = ST_N / 2, L = arm[k], np = mono ? 1 : 3;
    _fmemset(&bm, 0, sizeof bm);
    drv_bitmap_init(&bm, ST_N, ST_N, (BYTE FAR *)bits);
    memset(bits, 0, sizeof bits);
    for (y = 0; y < ST_N; y++)
        for (x = 0; x < ST_N; x++) {
            int dx = x - c, dy = y - c, col = -1, p;
            if ((dy == 0 && abs(dx) <= L) || (dx == 0 && abs(dy) <= L * 4 / 5)) col = 7;
            if (k >= 3 && abs(dx) == abs(dy) && abs(dx) <= L / 3) col = 5;   /* Glitzer, cyan */
            if (k >= 2 && abs(dx) <= 1 && abs(dy) <= 1 && (dx == 0 || dy == 0)) col = 3;  /* gelbe Mitte */
            if (k == 5) {                                /* Zwinkern: Lid zu */
                col = (dy == 0 && abs(dx) <= 6) || (dy == 1 && abs(dx) <= 4 && abs(dx) >= 2) ? 7 : -1;
            }
            if (col < 0) {                               /* Himmel des Bildes */
                if (mono) col = (int)(bg_mono[y] >> x) & 1;
                else col = (int)((bg_farbe[0][y] >> x) & 1) | (int)((bg_farbe[1][y] >> x) & 1) << 1
                         | (int)((bg_farbe[2][y] >> x) & 1) << 2;
            } else if (mono) col = 1;
            if (!col) continue;
            for (p = 0; p < np; p++)
                if (col & (1 << p)) bits[(y * np + p) * 4 + (x >> 3)] |= 1 << (x & 7);
        }
    BitBlt(scr, ST_X - c, ST_Y - c, &bm, 0, 0, ST_N, ST_N, 0xCC0020L, 0, 0);
}

/* Startschirm zeigen; 1, wenn er steht. Das Modul wird danach geladen
   und eingerichtet, waehrend das Bild stehen bleibt (drv_keep). */
static BITMAP st_scr;
static int st_mono, st_i;
static DWORD st_t0;

static int startschirm(const char *exe, int mono)
{
    static char arbeit[5312];
    char name[80], *p;
    strncpy(name, exe, 66); name[66] = 0;
    p = strrchr(name, '\\'); if (!p) p = strrchr(name, ':');
    strcpy(p ? p + 1 : name, "DMVBD.PIC");
    Enable(&st_scr, 0, 0, 0, 0);
    if (gpic_datei(name, 1 | (mono ? 2 : 4), 0, 0, arbeit)) return 0;
    st_mono = mono; st_t0 = now_ms();
    stern(&st_scr, mono, 0);
    return 1;
}

/* Stern zwinkern lassen, bis eine Taste kommt oder 8 s seit dem Anfang
   vergangen sind */
static void startschirm_warten(void)
{
    static const BYTE folge[] = { 0, 1, 2, 3, 4, 3, 2, 3, 4, 3, 2, 1, 0, 0, 1, 2, 5, 5, 2, 1 };
    DWORD t = 0;
    while (!kbhit() && now_ms() - st_t0 < 8000) {
        if (now_ms() - t >= 120) { t = now_ms(); stern(&st_scr, st_mono, folge[st_i]); st_i = (st_i + 1) % sizeof folge; }
    }
    if (kbhit()) getch();
}

/* ---- Einstellungen des Moduls (Ressourcen Typ 1000, Nr. 1..4) -------
   Aus den Modulen erschlossen (After Dark 1.0):
   Byte 0 Art, ab 2 Titel (20 Zeichen), ab 22 Zahl n, ab 24 Vorgabe,
   Namen zu 16 Zeichen ab 32.
   Art 2  Schieber: Vorgabe = Stellung in %, Bereich ab 48/50; das Modul
          bekommt den Wert im Bereich.
   Art 0/1 mit n > 0: Schieber mit Beschriftung: n Namen, danach n
          Obergrenzen (z. B. 33 67 100); Vorgabe ist ein Wert; Eintrag k
          bedeutet den Wert 0 bzw. die Obergrenze von k-1 (so liest es
          TOASTERS.AD: 0 hell, 33 mittel, 67 dunkel).
   Art 3  Auswahlliste: Vorgabe = Nummer des Eintrags (ab 0).
   Art 5  und Art 0 ohne Namen: Kaestchen an/aus (Art 0: an bei Farbe). */
static BYTE FAR *einst(int i) { WORD len; return ne_resource(&g_mod, 1000, (LPSTR)MK_FP(0, i + 1), &len); }
static int  e_art(BYTE FAR *r) { int a = *(WORD FAR *)r; return (a == 0 || a == 1) && *(WORD FAR *)(r + 22) ? 1 : a; }
static int  l_n(BYTE FAR *r) { int n = *(WORD FAR *)(r + 22); return n > 16 ? 16 : n; }
static char FAR *l_name(BYTE FAR *r, int k) { return (char FAR *)r + 32 + 16 * k; }
static short l_wert(BYTE FAR *r, int k) { return k ? *(short FAR *)(r + 32 + 16 * l_n(r) + 2 * (k - 1)) : 0; }

static int f_istr(const char *a, const char FAR *b)      /* a Anfang von b? */
{
    for (; *a; a++, b++) if ((*a & 0xDF) != (*b & 0xDF)) return 0;
    return 1;
}

static void einst_zeigen(void)
{
    int i, k;
    for (i = 0; i < 4; i++) {
        BYTE FAR *r = einst(i);
        if (!r || !r[2]) continue;
        printf("  /%d  %-20.20Fs ", i + 1, (char FAR *)r + 2);
        switch (e_art(r)) {
        case 2: printf("Schieber %d..%d\n", *(short FAR *)(r + 48), *(short FAR *)(r + 50)); break;
        case 1: case 3:
            printf(e_art(r) == 1 ? "Stufen:" : "Liste:");
            for (k = 0; k < l_n(r); k++) printf(" %d=%.16Fs", k + 1, l_name(r, k));
            printf("\n"); break;
        default: printf("an/aus (1/0)\n"); break;
        }
    }
}

/* Wert fuer Einstellung i: Vorgabe des Moduls oder Angabe a ("/i=a") */
static short einst_wert(int i, const char *a, int mono)
{
    BYTE FAR *r = einst(i);
    int k;
    if (!r) return a ? atoi(a) : 0;
    switch (e_art(r)) {
    case 2: {
        long mn = *(short FAR *)(r + 48), mx = *(short FAR *)(r + 50), pos = *(short FAR *)(r + 24), v;
        v = a ? atol(a) : mn + (pos * (mx - mn) + 50) / 100;
        return (short)(v < mn ? mn : v > mx ? mx : v); }
    case 1: case 3:
        if (!a) return *(short FAR *)(r + 24);
        if (*a >= '0' && *a <= '9') k = atoi(a) - 1;
        else for (k = l_n(r) - 1; k > 0 && !f_istr(a, l_name(r, k)); k--) ;
        if (k < 0) k = 0; if (k >= l_n(r)) k = l_n(r) - 1;
        return e_art(r) == 3 ? k : l_wert(r, k);
    case 5:
        return a ? (short)(atoi(a) != 0) : *(short FAR *)(r + 24);
    default:
        if (a) return (short)(atoi(a) != 0);
        return (r[2] && !mono) ? 1 : 0;
    }
}

/* ---- ein Modul laufen lassen ------------------------------------------
   Rueck: 2 Taste (Ende), 1 Zeit um (naechstes Modul), 0 Fehler */
static long  frames;
static DWORD t_ges;

static int lauf(short *ctl, int mono, int *titel, DWORD dauer)
{
    HANDLE hsys, hmod;
    BYTE FAR *ps, FAR *pm;
    AD_SYSTEM FAR *sys;
    AD_MODULE FAR *mod;
    MODFN fn;
    DWORD t0;
    int i, r = 1;

    drv_keep = *titel;                      /* Startschirm stehen lassen */
    w16_localbase(*(WORD FAR *)(g_mod.file + *(WORD FAR *)(g_mod.file + 0x3C)
                 + *(WORD FAR *)(g_mod.file + *(WORD FAR *)(g_mod.file + 0x3C) + 0x22)
                 + 8 * (g_mod.autodata - 1) + 6));
    w16_init();
    if (!callentry((WORD)g_mod.csip, g_mod.seg[(WORD)(g_mod.csip >> 16) - 1], g_mod.dgroup, g_mod.heap, g_mod.hinst))
        return 0;
    hmod = w16_galloc(sizeof(AD_MODULE), &pm);
    hsys = w16_galloc(sizeof(AD_SYSTEM), &ps);
    mod = (AD_MODULE FAR *)pm; sys = (AD_SYSTEM FAR *)ps;
    mod->ptRgnSize.x = 640; mod->ptRgnSize.y = 400;
    for (i = 0; i < 4; i++) { mod->iControlValue[i] = ctl[i]; mod->iControlID[i] = i + 1; }
    mod->hModule = g_mod.hinst;
    sys->iOperatingMode = 0; sys->iProcessorType = 0; sys->bCoProcessor = 0;
    sys->ptScreenSize.x = 640; sys->ptScreenSize.y = 400;
    sys->iBitsPerPixel = mono ? 1 : 4;
    sys->ptAspect.x = 10; sys->ptAspect.y = 13;
    sys->ptDotsPerInch.x = 96; sys->ptDotsPerInch.y = 72;
    sys->iADVersion = 0x100;
    sys->iSaverMode = 0;
    sys->hModuleInfo = hmod;

    fn = (MODFN)g_mod.module;
    g_tick = 0; g_tick0 = now_ms();
    i = fn(0, w16_screen_dc(), hsys);        /* INITIALIZE: unter dem Startschirm */
    if (*titel) { startschirm_warten(); drv_clear(); *titel = 0; }
    drv_keep = 0;
    t0 = now_ms();
    if (i == 0) {                            /* BLANK: Rueckgabe zaehlt nicht (Module liefern Zufall) */
        fn(1, w16_screen_dc(), hsys);
        for (;;) {
            DWORD t = now_ms();
            if (kbhit()) {                   /* E: Einstellungen, sonst Ende */
                int c = getch();
                r = (c == 'e' || c == 'E') ? 3 : 2;
                break;
            }
            g_tick = t - t0;
            if (dauer && g_tick >= dauer) break;
            if (fn(2, w16_screen_dc(), hsys) == 5) { r = 2; break; }   /* WAKE_UP */
            frames++;
        }
    } else r = 0;
    fn(3, w16_screen_dc(), hsys);
    t_ges += now_ms() - t0;
    return r;
}

/* Liste der .AD-Dateien im Verzeichnis dir (mit \ am Ende oder leer) */
#define MAXAD 32
static char adname[MAXAD][13];
static int  nad;
static int namecmp(const void *a, const void *b) { return strcmp((const char *)a, (const char *)b); }
static void ad_liste(const char *dir)
{
    char m[80];
    struct find_t f;
    strcpy(m, dir); strcat(m, "*.AD");
    nad = 0;
    if (_dos_findfirst(m, _A_NORMAL | _A_RDONLY, &f) == 0)
        do { if (nad < MAXAD) strcpy(adname[nad++], f.name); } while (_dos_findnext(&f) == 0);
    qsort(adname, nad, 13, namecmp);
}

/* ---- Einstellungen merken: DMVBD.INI neben DMVBD.EXE ------------------
   eine Zeile je Modul: NAME.AD w1 w2 w3 w4 (die Werte, die das Modul
   bekommt) */
static char ini[80];
static const char *nur_name(const char *p)
{
    const char *n = strrchr(p, '\\');
    if (!n) n = strrchr(p, ':');
    return n ? n + 1 : p;
}
static int ini_lesen(const char *mod, short *w)
{
    FILE *f = fopen(ini, "r");
    char z[100], n[14];
    int a, b, c, d, ok = 0;
    if (!f) return 0;
    while (fgets(z, sizeof z, f))
        if (sscanf(z, "%13s %d %d %d %d", n, &a, &b, &c, &d) == 5 && !stricmp(n, mod)) {
            w[0] = a; w[1] = b; w[2] = c; w[3] = d; ok = 1;
        }
    fclose(f);
    return ok;
}
static void ini_schreiben(const char *mod, short *w)
{
    static char alt[40][100];
    FILE *f = fopen(ini, "r");
    int n = 0, i;
    char nm[14];
    if (f) {
        while (n < 40 && fgets(alt[n], 100, f))
            if (!(sscanf(alt[n], "%13s", nm) == 1 && !stricmp(nm, mod))) n++;
        fclose(f);
    }
    f = fopen(ini, "w");
    if (!f) return;
    for (i = 0; i < n; i++) fputs(alt[i], f);
    fprintf(f, "%s %d %d %d %d\n", mod, w[0], w[1], w[2], w[3]);
    fclose(f);
}

/* Einstellungsseite (Textbetrieb): Ziffer waehlt, + und - aendern,
   Eingabe uebernimmt und merkt, Esc verwirft. 1 = geaendert */
static int einstellen(const char *mod, short *ctl, int mono)
{
    short w[4];
    int sel = 0, i, k, c;
    memcpy(w, ctl, sizeof w);
    for (;;) {
        printf("\x1A");                         /* Schirm loeschen */
        printf("DMVBD - Einstellungen fuer %s\n\n", mod);
        for (i = 0; i < 4; i++) {
            BYTE FAR *r = einst(i);
            if (!r || !r[2]) continue;
            printf(" %c %d  %-20.20Fs  ", sel == i ? '>' : ' ', i + 1, (char FAR *)r + 2);
            switch (e_art(r)) {
            case 2: printf("%d   (%d..%d)\n", w[i], *(short FAR *)(r + 48), *(short FAR *)(r + 50)); break;
            case 1:
                for (k = l_n(r) - 1; k > 0 && l_wert(r, k) > w[i]; k--) ;
                printf("%.16Fs\n", l_name(r, k)); break;
            case 3: printf("%.16Fs\n", l_name(r, w[i] < l_n(r) ? w[i] : 0)); break;
            default: printf("%s\n", w[i] ? "an" : "aus"); break;
            }
        }
        printf("\n Ziffer: waehlen   + / -: aendern   Eingabe: uebernehmen   Esc: zurueck\n");
        c = getch();
        if (c == 27) return 0;
        if (c == 13) { memcpy(ctl, w, sizeof w); ini_schreiben(mod, w); return 1; }
        if (c >= '1' && c <= '4' && einst(c - '1')) sel = c - '1';
        if (c == '+' || c == '-') {
            BYTE FAR *r = einst(sel);
            int d = c == '+' ? 1 : -1;
            if (!r) continue;
            switch (e_art(r)) {
            case 2: {
                short mn = *(short FAR *)(r + 48), mx = *(short FAR *)(r + 50);
                int st = (mx - mn) > 40 ? (mx - mn) / 20 : 1;
                w[sel] += d * st; if (w[sel] < mn) w[sel] = mn; if (w[sel] > mx) w[sel] = mx;
                break; }
            case 1:
                for (k = l_n(r) - 1; k > 0 && l_wert(r, k) > w[sel]; k--) ;
                k += d; if (k < 0) k = 0; if (k >= l_n(r)) k = l_n(r) - 1;
                w[sel] = l_wert(r, k); break;
            case 3:
                k = w[sel] + d; if (k < 0) k = 0; if (k >= l_n(r)) k = l_n(r) - 1;
                w[sel] = k; break;
            default: w[sel] = !w[sel]; break;
            }
        }
    }
}

int main(int argc, char **argv)
{
    char pfad[80], dir[80];
    const char *path = 0, *setc[4] = { 0, 0, 0, 0 };
    int i, k, force = 0, mono, titel = 1, zeigen = 0, erstes = 1, fehler = 0, tsr = 0;
    short ctl[4];
    DWORD dauer = 0;

    for (i = 1; i < argc; i++)               /* /T: von DMVBDTSR gestartet */
        if ((argv[i][0] == '/' || argv[i][0] == '-') && (argv[i][1] & 0xDF) == 'T') tsr = 1;
    if (!tsr) printf("DMVBD - Before Dawn, Bildschirmschoner fuer die NCR DMV\n");
    for (i = 1; i < argc; i++) {
        char *a = argv[i];
        if (a[0] == '/' || a[0] == '-') {
            char c = a[1] & 0xDF;
            if (c == 'M') force = 1;
            else if (c == 'C') force = 2;
            else if (c == 'N') titel = 0;
            else if (c == 'R') {             /* /R:n oder /R=n Minuten, ohne Zahl 2 */
                const char *q = a + 2; int n;
                if (*q == ':' || *q == '=') q++;
                n = atoi(q); if (n <= 0) n = 2;
                dauer = (DWORD)n * 60000L;
            }
            else if (a[1] == '?' || c == 'E') zeigen = 1;
            else if (a[1] >= '1' && a[1] <= '4' && a[2] == '=') setc[a[1] - '1'] = a + 3;
        } else path = a;
    }
    if (tsr && !path && !dauer) dauer = 2 * 60000L;   /* ohne Modul: alle reihum */
    if (!path && !dauer) {
        printf("Aufruf: DMVBD [modul[.AD]] [/M|/C] [/N] [/R:n] [/?] [/1=w ... /4=w]\n"
               "  /R:n  alle n Minuten das naechste Modul aus dem Verzeichnis\n"
               "  /?    Einstellungen des Moduls zeigen\n"
               "  /n=w  Einstellung n: Zahl, bei Listen auch Nummer oder Name\n");
        return 1;
    }
    /* Verzeichnis und Name; ohne Endung .AD anhaengen */
    dir[0] = 0;
    if (path) {
        char *b, *c;
        strncpy(pfad, path, sizeof pfad - 4); pfad[sizeof pfad - 4] = 0;
        b = strrchr(pfad, '\\'); c = strrchr(pfad, ':');
        if (c > b) b = c;
        if (!strchr(b ? b : pfad, '.')) strcat(pfad, ".AD");
        if (b) { k = (int)(b - pfad) + 1; memcpy(dir, pfad, k); dir[k] = 0; }
    }
    /* /T: Module ohne Verzeichnisangabe neben DMVBD.EXE suchen (COMMAND
       startet DMVBD im aktuellen Verzeichnis, wo die Module meist fehlen) */
    if (tsr && !dir[0]) {
        const char *n = nur_name(argv[0]);
        k = (int)(n - argv[0]);
        if (k > 0 && k < 60) {
            memcpy(dir, argv[0], k); dir[k] = 0;
            if (path) {
                char t[16];
                strncpy(t, pfad, 15); t[15] = 0;
                strcpy(pfad, dir); strcat(pfad, t);
            }
        }
    }
    mono = force ? force == 1 : romletter() == 'M';
    drv_mono = mono;
    strncpy(ini, argv[0], 66); ini[66] = 0;
    strcpy((char *)nur_name(ini), "DMVBD.INI");

    k = -1;                                  /* Stelle in der Liste */
    if (dauer) {
        ad_liste(dir);
        if (!nad) { printf("DMVBD: keine .AD-Dateien\n"); return 1; }
        k = 0;
        if (path) {
            const char *n = strrchr(pfad, '\\'); n = n ? n + 1 : (strrchr(pfad, ':') ? strrchr(pfad, ':') + 1 : pfad);
            for (i = 0; i < nad; i++) if (!stricmp(adname[i], n)) k = i;
        }
    }
    for (;;) {
        int r;
        if (k >= 0) { strcpy(pfad, dir); strcat(pfad, adname[k]); }
        if (!ne_load(&g_mod, pfad)) {
            ne_free(&g_mod);
            if (erstes && !dauer) return 1;
            r = 0;
        } else {
            if (zeigen) { printf("%s:\n", pfad); einst_zeigen(); return 0; }
            /* Vorrang: /1.. (nur fuer das genannte Modul), dann DMVBD.INI,
               dann die Vorgaben des Moduls */
            {
                short gemerkt[4];
                int hat = ini_lesen(nur_name(pfad), gemerkt);
                for (i = 0; i < 4; i++) {
                    const char *a = erstes && path ? setc[i] : 0;
                    ctl[i] = a || !hat ? einst_wert(i, a, mono) : gemerkt[i];
                }
            }
            if (titel) titel = startschirm(argv[0], mono);
            ne_still = 1;
            r = lauf(ctl, mono, &titel, dauer);
            if (r == 3) {                    /* Taste E: Einstellungen, dann neu */
                w16_done();
                einstellen(nur_name(pfad), ctl, mono);
                for (i = 0; i < 4; i++) setc[i] = 0;
            }
            ne_free(&g_mod);
            w16_reset();
            if (r == 3) { erstes = 0; continue; }
        }
        erstes = 0;
        if (r == 2 || !dauer) break;
        fehler = r ? 0 : fehler + 1;
        if (fehler >= nad) break;            /* keines laeuft */
        k = (k + 1) % nad;
    }
    w16_done();
    if (tsr) return 0;                      /* Textschirm ist geloescht (g_text), dann der Prompt */
    printf("%ld Bilder in %lu ms\n", frames, t_ges);
    w16_report();
    return 0;
}
