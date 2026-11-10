/* NELOAD.C - DMVBD: Lader fuer NE-DLLs (Windows 3.0 Real Mode)
   Laedt die Segmente, loest Relokationen auf (intern, Importe ueber
   Ordinal oder Name) und passt den Anfang exportierter Funktionen so an
   wie Windows im Real Mode (push ds/pop ax/nop -> mov ax,DGROUP).
   Importe zeigen auf die Funktionen in W16.C; nicht umgesetzte auf
   einen Stummel, der meldet und die Parameter abraeumt.
   Code: Claude, Prompts: rfka01 */

#include <stdio.h>
#include <string.h>
#include <dos.h>
#include <fcntl.h>
#include <io.h>
#include "BD.H"
#include "W16.H"

NEMOD g_mod;

#define RD16(p) (*(WORD FAR *)(p))
#define RD32(p) (*(DWORD FAR *)(p))

/* Stummel fuer nicht umgesetzte Importe: mov ax,nr / call far w16_unbek / retf n */
static void meld(const char *f, ...);
#define MAXSTUB 64
static BYTE stubs[MAXSTUB][12];
static WORD nstub;
static struct { BYTE mod; WORD ord; } stubid[MAXSTUB];
extern DWORD __far __loadds w16_unbek(WORD nr);   /* W16.C, Nummer in AX */
#pragma aux w16_unbek parm [ax]

/* Eintrag Nr. ord der Eintragstabelle -> Segmentnummer und Offset */
static int entry_of(BYTE FAR *f, WORD ne, WORD ord, WORD *sn, WORD *off)
{
    BYTE FAR *p = f + ne + RD16(f + ne + 0x04);
    WORD o = 1;
    while (p[0]) {
        BYTE cnt = p[0], ind = p[1], j;
        p += 2;
        if (ind == 0) { o += cnt; continue; }
        for (j = 0; j < cnt; j++, o++) {
            if (o == ord) {
                if (ind == 0xFF) { *sn = p[3]; *off = RD16(p + 4); }
                else { *sn = ind; *off = RD16(p + 1); }
                return 1;
            }
            p += ind == 0xFF ? 6 : 3;
        }
    }
    return 0;
}

static void FAR *imp_target(BYTE FAR *f, WORD ne, WORD modidx, WORD ordname, BYTE byname)
{
    WORD impt = ne + RD16(f + ne + 0x2a), modt = ne + RD16(f + ne + 0x28);
    BYTE FAR *mn = f + impt + RD16(f + modt + 2 * (modidx - 1));
    char name[16]; int i, mi = -1, ord = ordname;
    const W16IMP *t; WORD ab; void FAR *stubp;

    memset(name, 0, sizeof name);
    for (i = 0; i < mn[0] && i < 15; i++) name[i] = mn[1 + i];
    for (i = 0; w16mods[i]; i++) if (!strcmp(name, w16mods[i])) mi = i;
    if (byname) {
        BYTE FAR *fn = f + impt + ordname;
        meld("DMVBD: Import ueber Namen nicht unterstuetzt: %s.%.*Fs\n", name, fn[0], fn + 1);
        return 0;
    }
    if (mi >= 0)
        for (t = w16tab; t->mod != 255; t++)
            if (t->mod == mi && t->ord == ord) return (void FAR *)t->fn;
    /* Stummel */
    for (i = 0; i < nstub; i++)
        if (stubid[i].mod == (BYTE)mi && stubid[i].ord == ord) return (void FAR *)stubs[i];
    ab = (mi >= 0 && ord < w16nord[mi]) ? w16argb[mi][ord] : 255;
    if (ab == 255 || nstub >= MAXSTUB) {
        meld("DMVBD: unbekannter Import %s.%d\n", name, ord);
        return 0;
    }
    i = nstub++;
    stubid[i].mod = (BYTE)mi; stubid[i].ord = ord;
    stubs[i][0] = 0xB8; *(WORD *)&stubs[i][1] = i;          /* mov ax,i */
    stubs[i][3] = 0x9A;                                       /* call far */
    *(WORD *)&stubs[i][4] = FP_OFF(w16_unbek);
    *(WORD *)&stubs[i][6] = FP_SEG(w16_unbek);
    stubs[i][8] = 0xCA;                                       /* retf n */
    *(WORD *)&stubs[i][9] = ab;
    stubp = (void FAR *)stubs[i];
    return stubp;
}

/* Meldungen: still, solange ne_still gesetzt ist (Wechselbetrieb in Grafik) */
#include <stdarg.h>
int ne_still;
static void meld(const char *f, ...)
{
    va_list v;
    if (ne_still) return;
    va_start(v, f); vprintf(f, v); va_end(v);
}

/* Modul wieder freigeben (auch halb geladen) */
void ne_free(NEMOD *m)
{
    WORD i;
    for (i = 0; i < 16; i++) if (m->seg[i]) _dos_freemem(m->seg[i]);
    if (m->file) _dos_freemem(FP_SEG(m->file));
    memset(m, 0, sizeof *m);
    nstub = 0;
}

const char *w16_stubname(WORD i, WORD *ord)
{
    *ord = stubid[i].ord;
    return stubid[i].mod != 255 ? w16mods[stubid[i].mod] : "?";
}

int ne_load(NEMOD *m, const char *path)
{
    int fh; long len; WORD paras, sel, ne, i, k, n, align, segt;
    BYTE FAR *f;
    unsigned rd;

    memset(m, 0, sizeof *m);
    if (_dos_open(path, O_RDONLY, &fh)) { meld("DMVBD: %s nicht gefunden\n", path); return 0; }
    len = lseek(fh, 0, SEEK_END); lseek(fh, 0, SEEK_SET);
    paras = (WORD)((len + 15) >> 4);
    if (_dos_allocmem(paras, (unsigned *)&sel)) { meld("DMVBD: kein Speicher\n"); return 0; }
    f = (BYTE FAR *)MK_FP(sel, 0);
    for (i = 0; (long)i * 16384 < len; i++) {
        _dos_read(fh, (BYTE FAR *)MK_FP(sel + i * 1024, 0), 16384, &rd);
    }
    _dos_close(fh);
    m->file = f; m->flen = len;
    if (f[0] != 'M' || f[1] != 'Z') { meld("DMVBD: keine EXE/DLL\n"); return 0; }
    ne = RD16(f + 0x3c);
    if (f[ne] != 'N' || f[ne + 1] != 'E') { meld("DMVBD: kein NE-Format\n"); return 0; }
    if (f[ne + 0x0c] & 0xF0 & ~0x10) { /* Zielprozessor 286+ : trotzdem versuchen */ }
    m->nseg = RD16(f + ne + 0x1c);
    m->autodata = RD16(f + ne + 0x0e);
    m->heap = RD16(f + ne + 0x10);
    m->csip = RD32(f + ne + 0x14);
    m->rsrc = ne + RD16(f + ne + 0x24);
    align = RD16(f + ne + 0x32);
    segt = ne + RD16(f + ne + 0x22);
    if (m->nseg > 16) { meld("DMVBD: zu viele Segmente\n"); return 0; }

    /* Segmente laden */
    for (i = 0; i < m->nseg; i++) {
        BYTE FAR *e = f + segt + 8 * i;
        DWORD off = (DWORD)RD16(e) << align;
        WORD ln = RD16(e + 2), mina = RD16(e + 6);
        DWORD size = mina ? mina : 0x10000L;
        WORD s;
        if (i + 1 == m->autodata) size = 0xFFF0L;   /* lokaler Heap waechst wie unter Windows bis 64 KB */
        if (_dos_allocmem((WORD)((size + 15) >> 4), (unsigned *)&s)) { meld("DMVBD: kein Speicher\n"); return 0; }
        m->seg[i] = s;
        _fmemset(MK_FP(s, 0), 0, (WORD)size);
        if (RD16(e)) _fmemcpy(MK_FP(s, 0), f + off, ln ? ln : 0xFFFF);
    }
    m->dgroup = m->autodata ? m->seg[m->autodata - 1] : 0;
    m->hinst = m->dgroup;

    /* Relokationen */
    for (i = 0; i < m->nseg; i++) {
        BYTE FAR *e = f + segt + 8 * i;
        WORD fl = RD16(e + 4), ln = RD16(e + 2);
        BYTE FAR *r;
        if (!(fl & 0x100) || !RD16(e)) continue;
        r = f + ((DWORD)RD16(e) << align) + (ln ? ln : 0x10000L);
        n = RD16(r); r += 2;
        for (k = 0; k < n; k++, r += 8) {
            BYTE st = r[0], ty = r[1];
            WORD so = RD16(r + 2), a = RD16(r + 4), b = RD16(r + 6);
            WORD tseg, toff, add = ty & 4;
            void FAR *tp;
            switch (ty & 3) {
            case 0:                          /* intern */
                if ((a & 0xFF) == 0xFF) {    /* bewegliches Segment ueber Eintrag */
                    WORD sn, eo;
                    if (!entry_of(f, ne, b, &sn, &eo)) { meld("DMVBD: Eintrag %u fehlt\n", b); return 0; }
                    tseg = m->seg[sn - 1]; toff = eo;
                } else { tseg = m->seg[(a & 0xFF) - 1]; toff = b; }
                break;
            case 1: case 2:                  /* Import */
                tp = imp_target(f, ne, a, b, (ty & 3) == 2);
                if (!tp) return 0;
                tseg = FP_SEG(tp); toff = FP_OFF(tp); break;
            default:
                continue;                    /* OS-Fixups (Gleitkomma) */
            }
            for (;;) {
                BYTE FAR *p = (BYTE FAR *)MK_FP(m->seg[i], so);
                WORD nxt = RD16(p);
                switch (st) {
                case 0: *p = (BYTE)toff; break;
                case 2: *(WORD FAR *)p = tseg; break;
                case 3: *(WORD FAR *)p = toff + (add ? nxt : 0); *(WORD FAR *)(p + 2) = tseg; break;
                case 5: *(WORD FAR *)p = toff + (add ? nxt : 0); break;
                default: meld("DMVBD: Relokationsart %d\n", st); return 0;
                }
                if (add || nxt == 0xFFFF) break;
                so = nxt;
            }
        }
    }

    /* Eintragstabelle: Anfang exportierter Funktionen anpassen, MODULE suchen */
    {
        BYTE FAR *p = f + ne + RD16(f + ne + 0x04);
        WORD ord = 1, mod_ord = 0;
        BYTE FAR *nr;
        /* Ordinalzahl von MODULE aus den Namenstabellen */
        nr = f + ne + RD16(f + ne + 0x26);
        for (k = 0; k < 2; k++) {
            while (nr[0]) {
                if (nr[0] == 6 && !_fmemicmp(nr + 1, "MODULE", 6)) mod_ord = RD16(nr + 7);
                nr += nr[0] + 3;
            }
            nr = f + RD32(f + ne + 0x2c);
        }
        while (p[0]) {
            BYTE cnt = p[0], ind = p[1]; p += 2;
            for (k = 0; k < cnt; k++, ord++) {
                WORD sg, of;
                if (ind == 0) continue;
                if (ind == 0xFF) { sg = p[3]; of = RD16(p + 4); p += 6; }
                else { sg = ind; of = RD16(p + 1); p += 3; }
                {
                    BYTE FAR *c = (BYTE FAR *)MK_FP(m->seg[sg - 1], of);
                    if (c[0] == 0x1E && c[1] == 0x58 && c[2] == 0x90) {
                        c[0] = 0xB8; *(WORD FAR *)(c + 1) = m->dgroup;
                    }
                    if (ord == mod_ord) m->module = (void (FAR *)(void))c;
                }
            }
        }
        if (!m->module) { meld("DMVBD: keine Funktion MODULE\n"); return 0; }
    }
    return 1;
}

/* Ressource suchen: Typ (Zahl), Name (Zahl in LOWORD bei SEG 0, sonst Text).
   Namen werden auch ueber die Namentabelle (Typ 15) aufgeloest. */
static BYTE FAR *res_find(NEMOD *m, WORD type, WORD id, LPSTR name, WORD *len)
{
    BYTE FAR *f = m->file, FAR *rt = f + m->rsrc, FAR *p;
    WORD shift = RD16(rt), k, n;
    p = rt + 2;
    while (RD16(p)) {
        WORD t = RD16(p);
        n = RD16(p + 2); p += 8;
        for (k = 0; k < n; k++, p += 12) {
            WORD off = RD16(p), ln = RD16(p + 2), rid = RD16(p + 6);
            int ok;
            if (!(t & 0x8000) || (t & 0x7FFF) != type) continue;
            if (name) {
                BYTE FAR *s = rt + rid;
                ok = !(rid & 0x8000) && s[0] == _fstrlen(name) && !_fmemicmp(s + 1, name, s[0]);
            } else ok = (rid & 0x8000) && (rid & 0x7FFF) == id;
            if (ok) { *len = ln << shift; return f + ((DWORD)off << shift); }
        }
    }
    return 0;
}

BYTE FAR *ne_resource(NEMOD *m, WORD type, LPSTR name, WORD *len)
{
    BYTE FAR *r;
    WORD l, p;
    if (FP_SEG(name) == 0) return res_find(m, type, FP_OFF(name), 0, len);
    if (name[0] == '#') {
        WORD v = 0; LPSTR s = name + 1;
        while (*s >= '0' && *s <= '9') v = v * 10 + (*s++ - '0');
        return res_find(m, type, v, 0, len);
    }
    r = res_find(m, type, 0, name, len);
    if (r) return r;
    /* Namentabelle (Typ 15): Laenge, Typ, Nummer, Typname, Name */
    r = res_find(m, 15, 1, 0, &l);
    if (!r) return 0;
    for (p = 0; p + 6 <= l && RD16(r + p); p += RD16(r + p)) {
        BYTE FAR *e = r + p;
        BYTE FAR *nm = e + 6;
        nm += _fstrlen((char FAR *)nm) + 1;          /* Typname ueberspringen */
        if ((RD16(e + 2) & 0x7FFF) == type && !_fstricmp((char FAR *)nm, name))
            return res_find(m, type, RD16(e + 4) & 0x7FFF, 0, len);
    }
    return 0;
}
