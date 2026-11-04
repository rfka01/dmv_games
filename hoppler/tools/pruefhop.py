#!/usr/bin/env python3
"""pruefhop.py - HOPPLERs Fassung von DMVGRAFs pruefdos.py (rfka01/dmv_goodies,
dmvgraf/tools, Stand a8f9a5e), erweitert fuer Turbo-Pascal-Spiele:
INT 21h 2Ch (Uhr aus den Bildwechseln), 0Bh/07h/08h (Tastatur, eine Taste
wird bis zum Holen vorgemerkt), 3Ch/40h (Datei anlegen und schreiben);
Skriptzeilen "warte n" (n Abfragen ohne Taste) und "zaehl name" (mit
--zaehl: Befehle bis hierher). Braucht simlib.so aus dmvgraf/tools
(DMVGRAF_TOOLS=Verzeichnis, sonst ../../../dmv_goodies/dmvgraf/tools).

Urspruenglicher Text:
pruefdos.py - Pruefstand: eine DOS-EXE mit DMVGRAF (gcc-ia16, small)
unter einem nachgebildeten MS-DOS auf der DMV (8086 in Unicorn).

  pruefdos.py PROG.EXE skript.txt [--mono] [--nomaus] [--arg "1234"]

- INT 21h: 02h Zeichen, 06h DL=FFh Taste ohne Warten (je Aufruf eine
  Skriptzeile wie GHWSIM: maus x y k / taste c / bild datei.ppm),
  3Dh/3Fh/3Eh/42h Dateien im aktuellen Verzeichnis, 48h/49h Speicher,
  51h/62h PSP, 4Ch Ende.
- 7220 an A0h/A1h/A2h ueber simlib.so (dieselbe Nachbildung wie
  GHWSIM.C, make simlib.so); K806 an 30h-33h, 8741 an 40h/41h.
- Hardware fuer GHW.INC/GZEIT.INC: --cpu 8088|v20 (PUSH 0 und AAD 16 an
  den Stellen im Kern), --fpu keiner|8087, --karte k235|k230|keine (13h,
  8259 an 90h/91h), --kopro keiner|k234|k238|weg (OUT D0h laesst den
  Z80 ab F110h laufen, Paket z80; OUT DFh startet den 68008 in Unicorn,
  der Z8001 kennt nur die Probe; weg: der Z80 kommt nicht zurueck),
  --kanal0 steht (bis OUT 14h), --bild echt (Bildwechsel alle 17825 us
  statt nach Abfragen), --rom M5|M6|C6 (Kennung bei 0FF7h).
- --zaehl: ausgefuehrte Befehle zaehlen und bei jeder Skriptzeile
  "bild" melden.

Die Bilder lassen sich mit denen der PC-Simulation vergleichen (gleiches
Skript, gleiche Spielnummer). Braucht das Python-Paket unicorn (2.x).

Code: Claude (Anthropic), Idee und Prompts: rfka01. Lizenz: MIT
"""
import argparse, ctypes, os, struct, sys
from unicorn import *
from unicorn.x86_const import *

HIER = os.environ.get('DMVGRAF_TOOLS', os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', '..', '..', 'dmv_goodies', 'dmvgraf', 'tools'))
sim = ctypes.CDLL(os.path.join(HIER, 'simlib.so'))

ap = argparse.ArgumentParser()
ap.add_argument('prog'); ap.add_argument('skript')
ap.add_argument('--mono', action='store_true')
ap.add_argument('--arg', default='')
ap.add_argument('--nomaus', action='store_true')
ap.add_argument('--max', type=int, default=4000)
ap.add_argument('--speicher', type=int, default=2, help='freie 64-KB-Bloecke')
ap.add_argument('--cpu', default='v20', choices=['8088', 'v20'])
ap.add_argument('--fpu', default='keiner', choices=['keiner', '8087'])
ap.add_argument('--karte', default='k235', choices=['k235', 'k230', 'keine'])
ap.add_argument('--kopro', default='keiner', choices=['keiner', 'k234', 'k238', 'weg'])
ap.add_argument('--kanal0', default='laeuft', choices=['laeuft', 'steht'])
ap.add_argument('--bild', default='abfrage', choices=['abfrage', 'echt'])
ap.add_argument('--rom', default='')
ap.add_argument('--zaehl', action='store_true', help='Befehle zaehlen, bei jedem bild melden')
ap.add_argument('--psp', type=lambda x: int(x, 16), default=0x0FF0, help='Segment des PSP (hex)')
a = ap.parse_args()

mu = Uc(UC_ARCH_X86, UC_MODE_16)
mu.mem_map(0, 0x100000)
d = open(a.prog, 'rb').read()
PSP = a.psp
LAD = PSP + 0x10
COM = d[:2] != b'MZ'
if COM:         # .COM (etwa Turbo Pascal 3): alles in einem Segment ab 100h
    mu.mem_write(PSP * 16 + 0x100, d)
    bild = b'\0' * 0x10000; mina = 0
    ss, sp, cs, ip = -0x10, 0xFFFE, -0x10, 0x100
else:
    (mag, cblp, cp, crlc, cpar, mina, maxa, ss, sp, cs_, ip, cs, lfa, ov) = \
        struct.unpack('<2sHHHHHHHHHHHHH', d[:28])
    assert crlc == 0
    gr = (cp - 1) * 512 + (cblp or 512) if cp else len(d)
    bild = d[cpar * 16:gr]
    mu.mem_write(LAD * 16, bild)
tail = (' ' + a.arg.upper()) if a.arg else ''
psp = bytearray(0x100)
psp[0:2] = b'\xcd\x20'
struct.pack_into('<H', psp, 2, 0x9FFF)          # Ende des Speichers
psp[0x80] = len(tail); psp[0x81:0x81 + len(tail)] = tail.encode()
psp[0x81 + len(tail)] = 13
mu.mem_write(PSP * 16, bytes(psp))
frei = LAD + len(bild) // 16 + mina + 1
if COM:         # DOS gibt einer .COM-Datei allen Speicher, bis sie ihn mit 4Ah kuerzt
    frei = 0xA000
mu.reg_write(UC_X86_REG_CS, LAD + cs); mu.reg_write(UC_X86_REG_IP, ip)
mu.reg_write(UC_X86_REG_SS, LAD + ss); mu.reg_write(UC_X86_REG_SP, sp)
mu.reg_write(UC_X86_REG_DS, PSP); mu.reg_write(UC_X86_REG_ES, PSP)

skript = open(a.skript).read().splitlines()
st = dict(sx=320, sy=200, sb=0, ende=False, n2=0, frei=frei, bl=0)
dateien = {}
out = []

st['warte'] = 0; st['vor'] = None
def zeile():
    taste = 0
    if st['warte'] > 0:
        st['warte'] -= 1; return 0
    if not skript:
        return 0x1B
    z = skript.pop(0).split()
    if z and z[0] == 'maus':
        st['sx'], st['sy'], st['sb'] = int(z[1]), int(z[2]), int(z[3])
    elif z and z[0] == 'taste':
        w = z[1]; taste = int(w, 16) if w.startswith('0x') else ord(w[0])
    elif z and z[0] == 'warte':
        st['warte'] = int(z[1]) - 1
    elif z and z[0] == 'zaehl':
        print('%s: %d Befehle' % (z[1], zl[0]), flush=True)
    elif z and z[0] == 'bild':
        gdc_flush(); sim.w_bild(z[1].encode())
        if a.zaehl:
            print('%s: %d Befehle' % (z[1], zl[0]), flush=True)
    return taste

# ---- 7220: Eintraege sammeln, RDAT/CURD beantworten (wie pruef68.py) ----
gdc = dict(eintrag=bytearray(), lese=bytearray(), vs=0, n=0)
def gdc_flush():
    e = gdc['eintrag']
    if e:
        while len(e) > 255 and e[0] & 0xE0 == 0x20:  # langes WDAT teilen
            l = bytes([255]) + bytes(e[:255]) + b'\0'
            sim.w_liste(l)
            e = bytearray(e[:1]) + e[255:]
        l = bytes([len(e)]) + bytes(e) + b'\0'
        if e[0] & 0xE4 == 0xA0 or e[0] == 0xE0:
            buf = ctypes.create_string_buffer(1024)
            nl = sim.w_rdat(l, buf)
            gdc['lese'] = bytearray(buf.raw[:nl])
        else:
            sim.w_liste(l)
        gdc['eintrag'] = bytearray()
        gdc['n'] += 1

maus = dict(aus=bytearray())
def maus_antwort(c):
    if c == 0:
        y = 399 - st['sy']
        maus['aus'] = bytearray([st['sx'] & 0xFF, st['sx'] >> 8, y & 0xFF, y >> 8, st['sb']])

# 8253 grob: jede Abfrage eines Anschlusses kostet 3 us. Kanal 2 wie
# unter DOS 3.3 (Modus 3, 27463), nach Steuerwort B4h Modus 2 (2 us je
# Schritt); Kanal 0 mit rund 56 Hz.
pit = dict(t=0, m2=False, latch={0: 0, 2: 0}, lo={0: True, 2: True})
if a.kanal0 == 'laeuft':
    pit['k0an'] = 0
pic = dict(maske=0xB8)
def pit_wert(k):
    t = pit['t']
    if k == 0:
        if 'k0an' not in pit:
            return 0x323
        return (0x323 - (t - pit['k0an']) // 17825) & 0xFFFF
    if pit['m2']:
        return (65536 - (t // 2) % 65536) & 0xFFFF
    return 27463 - t % 27463

def port_in(uc, port, size, ud):
    p = port & 0xFF
    v = 0xFF
    pit['t'] += 3
    if p in (0x80, 0x82):
        k = p - 0x80
        w = pit['latch'][k]
        v = w & 0xFF if pit['lo'][k] else w >> 8
        pit['lo'][k] = not pit['lo'][k]
        return v
    if p == 0xA0:
        gdc['vs'] += 1
        if a.bild == 'echt':
            vs = 0x20 if pit['t'] % 17825 < 1200 else 0
        else:
            vs = 0x20 if (gdc['vs'] // 7) & 1 else 0
        v = 0x04 | vs | (0x40 if (gdc['vs'] // 2) & 1 else 0) | (1 if gdc['lese'] else 0)
    elif p == 0x13:
        v = 0x02 if a.karte == 'keine' else 0x00
    elif p == 0x91:
        v = pic['maske'] if a.karte == 'k235' else 0x91
    elif p == 0xCE:
        v = 0xCE
    elif p == 0xA1:
        gdc_flush()
        v = gdc['lese'].pop(0) if gdc['lese'] else 0
    elif p in (0x40, 0x41):
        v = 0
    elif 0x30 <= p <= 0x33 and not a.nomaus:
        if p == 0x30:
            v = maus['aus'].pop(0) if maus['aus'] else 0
        elif p == 0x31:
            v = (1 if maus['aus'] else 0) | (8 if maus.get('f1') else 0)   # F1 = A0 des letzten Schreibens
        elif p == 0x33:
            v = 0x80
        else:
            v = 0
    return v

def port_out(uc, port, size, value, ud):
    p = port & 0xFF
    v = value & 0xFF
    if p == 0xA1:
        gdc_flush(); gdc['eintrag'] = bytearray([v])
        if v & 0xE4 == 0xA0 or v == 0xE0:
            gdc_flush()
    elif p in (0xA0, 0xA2):
        gdc['eintrag'].append(v)
    elif p == 0x31 and not a.nomaus:
        maus['f1'] = 1
        maus_antwort(v)
    elif p == 0x30:
        maus['f1'] = 0
    elif p == 0x83:                     # 8253 Steuerwort
        k = v >> 6
        if k in (0, 2) and (v & 0x30) == 0:
            pit['latch'][k] = pit_wert(k); pit['lo'][k] = True
        elif v == 0xB4:
            pit['m2'] = True
    elif p == 0x11:                     # ROM ueber 0-1FFFh: Kennbuchstabe
        st['rom'] = bytes(mu.mem_read(0x0FF7, 8))
        k = bytearray(st['rom'])
        k[2] = ord('M' if a.mono else 'C')
        if a.rom:
            k[0] = 8; k[2] = ord(a.rom[0]); k[4] = ord('.'); k[5] = ord('0')
            k[6] = ord(a.rom[1]); k[7] = ord('.')
        mu.mem_write(0x0FF7, bytes(k))
    elif p == 0x10 and 'rom' in st:
        mu.mem_write(0x0FF7, st.pop('rom'))
    elif p == 0x91 and a.karte == 'k235':
        pic['maske'] = v
    elif p == 0x14 and 'k0an' not in pit:
        pit['k0an'] = pit['t']
    elif p == 0xD0 and a.karte == 'k235':
        z80lauf()

def R(r): return mu.reg_read(r)
def W(r, v): mu.reg_write(r, v & 0xFFFF)
def flag(bit, an):
    f = R(UC_X86_REG_EFLAGS)
    mu.reg_write(UC_X86_REG_EFLAGS, (f | (1 << bit)) if an else (f & ~(1 << bit)))
def stop():
    mu.emu_stop(); st['ende'] = True
def lin(seg, off): return (seg << 4) + off

def intr(uc, no, ud):
    if no == 0x20:
        stop(); return
    if no != 0x21:
        print('INT %02Xh bei %04X:%04X' % (no, R(UC_X86_REG_CS), R(UC_X86_REG_IP)))
        stop(); return
    ax = R(UC_X86_REG_AX); ah = ax >> 8
    bx, cx, dx, ds = R(UC_X86_REG_BX), R(UC_X86_REG_CX), R(UC_X86_REG_DX), R(UC_X86_REG_DS)
    flag(0, 0)
    if ah == 0x4C:
        stop(); return
    elif ah == 0x02:
        out.append(chr(dx & 0xFF))
    elif ah == 0x06 and dx & 0xFF != 0xFF:
        out.append(chr(dx & 0xFF))
    elif ah == 0x09:
        q = lin(ds, dx)
        while mu.mem_read(q, 1)[0] != 0x24:
            out.append(chr(mu.mem_read(q, 1)[0])); q += 1
    elif ah == 0x40 and bx in (1, 2):
        out.append(bytes(mu.mem_read(lin(ds, dx), cx)).decode('latin-1')); W(UC_X86_REG_AX, cx)
    elif ah == 0x4A:            # Block des Programms kuerzen oder verlaengern
        if R(UC_X86_REG_ES) == PSP and PSP + bx <= 0xA000:
            st['frei'] = PSP + bx
        else:
            W(UC_X86_REG_AX, 8); W(UC_X86_REG_BX, 0xA000 - PSP); flag(0, 1)
    elif ah in (0x25, 0x1A, 0x49):
        pass
    elif ah in (0x51, 0x62):
        W(UC_X86_REG_BX, PSP)
    elif ah == 0x35:
        W(UC_X86_REG_BX, 0); mu.reg_write(UC_X86_REG_ES, 0)
    elif ah == 0x30:
        W(UC_X86_REG_AX, 0x1E03)
    elif ah == 0x19:
        W(UC_X86_REG_AX, ax & 0xFF00)
    elif ah == 0x44:
        W(UC_X86_REG_DX, 0x80 if bx < 3 else 0)
    elif ah == 0x00:
        stop(); return
    elif ah == 0x2C:            # Uhrzeit aus den Bildwechseln (50 je s)
        t = gdc['vs'] // 14 * 2
        W(UC_X86_REG_CX, ((t // 360000) % 24) << 8 | (t // 6000) % 60)
        W(UC_X86_REG_DX, ((t // 100) % 60) << 8 | t % 100)
    elif ah == 0x0B or (ah == 0x06 and dx & 0xFF == 0xFF) or ah in (0x07, 0x08):
        if st['vor'] is None:
            st['n2'] += 1
            if st['n2'] > a.max:
                stop(); return
            st['vor'] = zeile()
        t = st['vor']
        if ah == 0x0B:
            W(UC_X86_REG_AX, (ax & 0xFF00) | (0xFF if t else 0)); return
        st['vor'] = None
        W(UC_X86_REG_AX, (ax & 0xFF00) | t)
        flag(6, t == 0)
    elif ah == 0x3D:
        n = bytes(mu.mem_read(lin(ds, dx), 80)).split(b'\0')[0].decode()
        try:
            h = 5 + len(dateien); dateien[h] = open(n, 'rb'); W(UC_X86_REG_AX, h)
        except OSError:
            W(UC_X86_REG_AX, 2); flag(0, 1)
    elif ah == 0x3C:            # anlegen
        n = bytes(mu.mem_read(lin(ds, dx), 80)).split(b'\0')[0].decode()
        h = 5 + len(dateien); dateien[h] = open(n, 'w+b'); W(UC_X86_REG_AX, h)
    elif ah == 0x40 and bx in dateien:
        dateien[bx].write(bytes(mu.mem_read(lin(ds, dx), cx))); W(UC_X86_REG_AX, cx)
    elif ah == 0x3F:
        b = dateien[bx].read(cx) if bx in dateien else b''
        mu.mem_write(lin(ds, dx), b); W(UC_X86_REG_AX, len(b))
    elif ah == 0x3E:
        if bx in dateien:
            dateien.pop(bx).close()
    elif ah == 0x42:            # Lage setzen: AL 0 Anfang, 1 hier, 2 Ende
        if bx in dateien:
            f = dateien[bx]
            v = cx << 16 | dx
            if v & 0x80000000:
                v -= 1 << 32
            p = f.seek(v, ax & 3)
            W(UC_X86_REG_AX, p & 0xFFFF); W(UC_X86_REG_DX, p >> 16)
        else:
            W(UC_X86_REG_AX, 6); flag(0, 1)
    elif ah == 0x48:
        if st['bl'] < a.speicher and st['frei'] + bx <= 0xA000:
            W(UC_X86_REG_AX, st['frei']); st['frei'] += bx; st['bl'] += 1
        else:
            W(UC_X86_REG_AX, 8); W(UC_X86_REG_BX, max(0, 0xA000 - st['frei'])); flag(0, 1)
    else:
        print('INT 21h AH=%02Xh nicht nachgebildet' % ah)
        stop()

# ---- Z80 und Koprozessor (OUT D0h vom 8088) ----
kopro = dict(laeufe=0)
def k68():
    from unicorn.m68k_const import UC_M68K_REG_PC, UC_M68K_REG_A7, UC_CPU_M68K_M68000
    m = Uc(UC_ARCH_M68K, UC_MODE_BIG_ENDIAN)
    m.ctl_set_cpu_model(UC_CPU_M68K_M68000)
    m.mem_map(0, 0x100000)
    m.mem_map(0xFFFFF000, 0x1000)
    m.mem_write(0, bytes(mu.mem_read(0, 0x100000)))
    ssp, pc = struct.unpack('>II', bytes(m.mem_read(0, 8)))
    m.reg_write(UC_M68K_REG_A7, ssp); m.reg_write(UC_M68K_REG_PC, pc)
    fertig = []
    def schreib(uc, acc, adr, size, val, ud):
        if adr & 0xFFFFF == 0xFFFDF:
            fertig.append(1); uc.emu_stop()
    m.hook_add(UC_HOOK_MEM_WRITE, schreib)
    try:
        m.emu_start(pc, 0xFFFFFFFF, count=5000000)
    except UcError as e:
        print('68008: Fehler', e)
    if not fertig:
        print('68008 kam nicht zurueck - die Maschine stuende'); stop(); return
    mu.mem_write(0, bytes(m.mem_read(0, 0x100000)))
PROBE = bytes.fromhex('0000C00000000008600400 00E80711FC0036000011C0FFDF4E722700C85A6E0800003A8600DF7A00'.replace(' ', ''))
def kstart():                           # OUT DFh vom Z80
    kopro['laeufe'] += 1
    if a.kopro == 'k234':
        k68()
    elif a.kopro == 'k238':
        if bytes(mu.mem_read(0, 40)) == PROBE:
            mu.mem_write(0, b'\x5a')
        else:
            print('Z8001: nur die Probe nachgebildet')

def z80lauf():
    if a.kopro == 'weg':
        return
    import z80
    zm = z80.Z80Machine()
    zm.set_memory_block(0, bytes(mu.mem_read(0, 0x10000)))
    zs = dict(aus=False)
    def zein(port):
        return 0xFF
    def zaus(port, v):
        port &= 0xFF
        if port == 0xD0:
            zs['aus'] = True
            zm.ticks_to_stop = 0
        elif port == 0xDF:
            mu.mem_write(0, bytes(zm.memory[0:0x10000]))
            kstart()
            zm.set_memory_block(0, bytes(mu.mem_read(0, 0x10000)))
    zm.set_input_callback(zein)
    zm.set_output_callback(zaus)
    zm.pc = 0xF110
    zm.sp = 0xFF00
    n = 0
    while not zs['aus'] and n < 200000 and not st['ende']:
        zm.ticks_to_stop = 1                # je Befehl, damit nach OUT D0h Schluss ist
        zm.run()
        n += 1
    if not zs['aus']:
        print('Z80 kam nicht zurueck (PC %04X) - die Maschine stuende' % zm.pc); stop(); return
    mu.mem_write(0, bytes(zm.memory[0:0x10000]))

# ---- 8088/V20 und 8087 an den Stellen in GHW.INC ----
def cpu_hooks():
    m = bytes(mu.mem_read(0, 0xA0000))
    def alle(muster):
        i = m.find(muster)
        while i >= 0:
            yield i
            i = m.find(muster, i + 1)
    def setze(adr, f):
        mu.hook_add(UC_HOOK_CODE, f, None, adr, adr)
    def ip(n):
        W(UC_X86_REG_IP, R(UC_X86_REG_IP) + n)
    for i in alle(b'\x8b\xdc\x6a\x00'):           # PUSH 0: 8088 = JPE $+2
        if a.cpu == '8088':
            setze(i + 2, lambda uc, ad, sz, ud: ip(2))
    for i in alle(b'\xb8\x02\x01\xd5\x10'):       # AAD 16: V20 rechnet mit 10
        if a.cpu == 'v20':
            def aadv(uc, ad, sz, ud):
                ax = R(UC_X86_REG_AX); W(UC_X86_REG_AX, ((ax >> 8) * 10 + (ax & 0xFF)) & 0xFF); ip(2)
            setze(i + 3, aadv)
    for i in alle(b'\xdb\xe3\xb9'):                # FNINIT
        setze(i, lambda uc, ad, sz, ud: ip(2))
    for i in alle(b'\xdd\x3e'):                     # FNSTSW [mem]
        if m[i + 4:i + 5] == b'\xb9':
            def sw(uc, ad, sz, ud):
                if a.fpu == '8087':
                    o = struct.unpack('<H', bytes(mu.mem_read(ad + 2, 2)))[0]
                    mu.mem_write(lin(R(UC_X86_REG_DS), o), b'\0\0')
                ip(4)
            setze(i, sw)
    for i in alle(b'\xd9\x3e'):                     # FNSTCW [mem]
        if m[i + 4:i + 5] == b'\xb9':
            def cw(uc, ad, sz, ud):
                if a.fpu == '8087':
                    o = struct.unpack('<H', bytes(mu.mem_read(ad + 2, 2)))[0]
                    mu.mem_write(lin(R(UC_X86_REG_DS), o), b'\xff\x03')
                ip(4)
            setze(i, cw)
cpu_hooks()
zl = [0]
if a.zaehl:
    def zhook(uc, ad, sz, ud):
        zl[0] += 1
    mu.hook_add(UC_HOOK_CODE, zhook)

mu.hook_add(UC_HOOK_INSN, port_in, None, 1, 0, UC_X86_INS_IN)
mu.hook_add(UC_HOOK_INSN, port_out, None, 1, 0, UC_X86_INS_OUT)
mu.hook_add(UC_HOOK_INTR, intr)
sim.w_init(0 if a.mono else 1)
sim.w_graf(1)       # Grafik schaltet das Programm selbst ein (PRAM)
while not st['ende']:
    try:
        mu.emu_start(lin(R(UC_X86_REG_CS), R(UC_X86_REG_IP)), 0x100000,
                     count=2000000)
    except UcError as e:
        print('Fehler', e, '%04X:%04X' % (R(UC_X86_REG_CS), R(UC_X86_REG_IP))); break
    if os.environ.get('GPROT'):
        print('PC %04X:%04X GDC %d Tasten %d' % (R(UC_X86_REG_CS), R(UC_X86_REG_IP), gdc['n'], st['n2']), flush=True)
gdc_flush()
print(''.join(out)[-400:])
print('GDC-Eintraege:', gdc['n'])

if a.zaehl:
    print('Ende: %d Befehle' % zl[0])
