#!/usr/bin/env python3
"""Minimal Win16 environment to run After Dark (Windows 3.0) modules in
unicorn. Prototype for DMVBD: loads an NE DLL, resolves imports to
Python stubs, runs LibEntry and Module(msg, hDC, hSystem), and renders
the screen DC to PNG. Logs every API call (name, args) so the needed
subset and the blit traffic per frame can be measured.

Usage: win16sim.py MODULE.AD [frames] [outprefix]
"""
import struct, sys, re, collections, os
import numpy as np
from unicorn import *
from unicorn.x86_const import *
from PIL import Image

SPEC = {}
def load_specs():
    for mod, f in (('GDI', 'gdi.exe16'), ('USER', 'user.exe16'), ('KERNEL', 'krnl386.exe16'),
                   ('KEYBOARD', 'keyboard.drv16'), ('SOUND', 'sound.drv16')):
        d = {}
        for l in open(f'/tmp/claude-0/wine/{f}.spec'):
            m = re.match(r'\s*(\d+)\s+(pascal|stub|equate|variable)\S*\s*(?:-\S+\s+)*(\w+)?\s*(?:\(([^)]*)\))?', l)
            if m:
                args = (m.group(4) or '').split()
                d[int(m.group(1))] = (m.group(3) or '?', args, m.group(2))
        SPEC[mod] = d
load_specs()
ASZ = {'word': 2, 's_word': 2, 'long': 4, 'ptr': 4, 'str': 4, 'segptr': 4, 'segstr': 4}

SEG_STUB = 0xF000   # import thunks: INT FEh / RETF n
SEG_STACK = 0x9000
GLOBAL_BASE = 0x40000

# Windows standard VGA 16-colour palette (index -> RGB)
PAL16 = [(0,0,0),(128,0,0),(0,128,0),(128,128,0),(0,0,128),(128,0,128),(0,128,128),(128,128,128),
         (192,192,192),(255,0,0),(0,255,0),(255,255,0),(0,0,255),(255,0,255),(0,255,255),(255,255,255)]


class NE:
    def __init__(self, path):
        self.b = b = open(path, 'rb').read()
        self.ne = ne = struct.unpack_from('<H', b, 0x3c)[0]
        h = lambda o, f='<H': struct.unpack_from(f, b, ne + o)[0]
        self.nseg, self.nmod = h(0x1c), h(0x1e)
        self.align = h(0x32)
        self.autodata = h(0x0e)
        self.heap = h(0x10)
        self.csip = h(0x14, '<I')
        segt, modt, impt = ne + h(0x22), ne + h(0x28), ne + h(0x2a)
        self.impt = impt
        self.mods = []
        for i in range(self.nmod):
            o = struct.unpack_from('<H', b, modt + 2 * i)[0]
            self.mods.append(b[impt + o + 1:impt + o + 1 + b[impt + o]].decode())
        self.segs = [struct.unpack_from('<HHHH', b, segt + 8 * s) for s in range(self.nseg)]
        # entry table
        self.entries = {}
        p = ne + h(0x04); ordn = 1
        while True:
            cnt, ind = b[p], b[p + 1]; p += 2
            if cnt == 0: break
            for i in range(cnt):
                if ind == 0: pass
                elif ind == 0xFF:
                    fl, _, seg, off = struct.unpack_from('<BHBH', b, p); p += 6
                    self.entries[ordn] = (seg, off, fl)
                else:
                    fl, off = struct.unpack_from('<BH', b, p); p += 3
                    self.entries[ordn] = (ind, off, fl)
                ordn += 1
        # names
        self.names = {}
        for tbl in (ne + h(0x26), h(0x2c, '<I')):
            p = tbl
            while b[p]:
                n = b[p + 1:p + 1 + b[p]].decode('latin1'); o = struct.unpack_from('<H', b, p + 1 + b[p])[0]
                self.names[n.upper()] = o; p += 3 + b[p]
        # resources
        self.res = {}
        rt = ne + h(0x24); shift = struct.unpack_from('<H', b, rt)[0]; p = rt + 2
        while True:
            t = struct.unpack_from('<H', b, p)[0]
            if t == 0: break
            n = struct.unpack_from('<H', b, p + 2)[0]; p += 8
            for i in range(n):
                off, ln, fl, rid = struct.unpack_from('<HHHH', b, p); p += 12
                key = (t & 0x7fff if t & 0x8000 else t, rid & 0x7fff if rid & 0x8000 else
                       b[rt + rid + 1:rt + rid + 1 + b[rt + rid]].decode('latin1').upper())
                self.res[key] = b[off << shift:(off << shift) + (ln << shift)]
        # name table (resource type 15): maps names to numeric ids
        nt = [v for (t, k), v in self.res.items() if t == 15]
        for d in nt:
            p = 0
            while p + 6 <= len(d):
                ln, ty, rid = struct.unpack_from('<HHH', d, p)
                if ln == 0: break
                rest = d[p + 6:p + ln]
                parts = rest.split(b'\0')
                name = parts[1].decode('latin1').upper() if len(parts) > 1 else ''
                if (ty & 0x7fff, rid & 0x7fff) in self.res:
                    self.res[(ty & 0x7fff, name)] = self.res[(ty & 0x7fff, rid & 0x7fff)]
                p += ln


class Sim:
    def __init__(self, path, mono=False):
        self.m = NE(path)
        self.mono = mono
        self.uc = uc = Uc(UC_ARCH_X86, UC_MODE_16)
        uc.mem_map(0, 0x100000)
        self.log = []
        self.calls = collections.Counter()
        self.blits = []
        self.tick = 0
        self.handles = {}
        self.nexth = 0x100
        self.gnext = GLOBAL_BASE
        self.screen = np.zeros((400, 640), np.uint8)
        self.thunks = []
        self.load()
        uc.hook_add(UC_HOOK_INTR, self.intr)

    # --- memory helpers
    def rd16(self, a): return struct.unpack('<H', self.uc.mem_read(a, 2))[0]
    def wr16(self, a, v): self.uc.mem_write(a, struct.pack('<H', v & 0xffff))
    def far(self, p): return ((p >> 16) << 4) + (p & 0xffff)
    def cstr(self, p):
        a = self.far(p); s = b''
        while True:
            c = self.uc.mem_read(a, 1)[0]
            if not c or len(s) > 200: break
            s += bytes([c]); a += 1
        return s.decode('latin1')

    def newh(self, obj):
        h = self.nexth; self.nexth += 2; self.handles[h] = obj; return h

    def galloc(self, size):
        a = self.gnext; self.gnext += (size + 15) & ~15
        h = self.newh({'kind': 'global', 'addr': a, 'size': size})
        return h, a

    # --- loader
    def load(self):
        m, uc = self.m, self.uc
        self.segbase = {}
        base = 0x10000
        for i, (off, ln, fl, mina) in enumerate(m.segs, 1):
            size = mina or 0x10000
            if i == m.autodata: size = min(0x10000, size + m.heap + 0x800)
            self.segbase[i] = base
            if off:
                ln = ln or 0x10000
                uc.mem_write(base, m.b[off << m.align:(off << m.align) + ln])
            base += (size + 0xFFF) & ~0xFFF
        self.dseg = self.segbase[m.autodata] >> 4
        # thunk segment
        self.stubs = []
        # relocations
        for i, (off, ln, fl, mina) in enumerate(m.segs, 1):
            if not (fl & 0x100) or not off: continue
            r = (off << m.align) + (ln or 0x10000)
            n = struct.unpack_from('<H', m.b, r)[0]
            for k in range(n):
                st, ty, so, a, b2 = struct.unpack_from('<BBHHH', m.b, r + 2 + 8 * k)
                tt = ty & 3; additive = ty & 4
                if tt == 0:   # internal
                    if a == 0xFF:
                        seg, eo, _ = m.entries[b2]; tseg, toff = self.segbase[seg] >> 4, eo
                    else:
                        tseg, toff = self.segbase[a] >> 4, b2
                elif tt in (1, 2):
                    mod = m.mods[a - 1]
                    if tt == 1: fn = SPEC.get(mod, {}).get(b2, ('#%d' % b2, [], 'stub'))
                    else:
                        nm = m.b[m.impt + b2 + 1:m.impt + b2 + 1 + m.b[m.impt + b2]].decode()
                        fn = (nm, [], 'stub')
                    tseg, toff = SEG_STUB, self.thunk(mod, b2 if tt == 1 else fn[0], fn)
                else:
                    continue  # OS fixups (floating point) - not needed here
                self.patch(self.segbase[i], so, st, tseg, toff, additive)
        # prolog patch of exported far functions (push ds/pop ax/nop -> mov ax,dgroup)
        for o, (seg, eo, fl) in m.entries.items():
            if seg in self.segbase:
                a = self.segbase[seg] + eo
                if bytes(uc.mem_read(a, 3)) == b'\x1e\x58\x90':
                    uc.mem_write(a, b'\xb8' + struct.pack('<H', self.dseg))
        uc.mem_write(SEG_STUB * 16 + 0xFFF0, b'\xf4')   # halt for returns

    def patch(self, base, so, st, tseg, toff, additive):
        while True:
            a = base + so
            nxt = self.rd16(a)
            if st == 2: self.wr16(a, tseg)
            elif st == 3: self.wr16(a, toff + (nxt if additive else 0)); self.wr16(a + 2, tseg)
            elif st == 5: self.wr16(a, toff + (nxt if additive else 0))
            elif st == 0: self.uc.mem_write(a, bytes([toff & 0xff]))
            else: raise Exception('src type %d' % st)
            if additive or nxt == 0xFFFF: break
            so = nxt

    def thunk(self, mod, key, fn):
        for i, (m2, k2, _) in enumerate(self.thunks):
            if (m2, k2) == (mod, key): return i * 8
        i = len(self.thunks); self.thunks.append((mod, key, fn))
        nb = sum(ASZ.get(x, 2) for x in fn[1]) if fn[2] == 'pascal' else 0
        self.uc.mem_write(SEG_STUB * 16 + i * 8, b'\xcd\xfe\xca' + struct.pack('<H', nb))
        return i * 8

    # --- calls into the module
    def call(self, seg, off, args, ds=None):
        uc = self.uc
        sp = 0xFFF0
        for v in args: sp -= 2; self.wr16(SEG_STACK * 16 + sp, v)
        sp -= 4; self.wr16(SEG_STACK * 16 + sp, 0xFFF0); self.wr16(SEG_STACK * 16 + sp + 2, SEG_STUB)
        uc.reg_write(UC_X86_REG_SS, SEG_STACK); uc.reg_write(UC_X86_REG_SP, sp)
        uc.reg_write(UC_X86_REG_DS, ds if ds is not None else self.dseg)
        uc.reg_write(UC_X86_REG_CS, seg); uc.reg_write(UC_X86_REG_IP, off)
        uc.emu_start(seg * 16 + off, SEG_STUB * 16 + 0xFFF0, count=20_000_000)
        return uc.reg_read(UC_X86_REG_AX)

    def libentry(self):
        seg, off = self.m.csip >> 16, self.m.csip & 0xffff
        u = self.uc
        u.reg_write(UC_X86_REG_DI, 0x50)   # hInstance
        u.reg_write(UC_X86_REG_CX, self.m.heap)
        u.reg_write(UC_X86_REG_ES, 0); u.reg_write(UC_X86_REG_SI, 0)
        return self.call(self.segbase[seg] >> 4, off, [])

    def module(self, msg, hdc, hsys):
        seg, off, _ = self.m.entries[self.m.names['MODULE']]
        return self.call(self.segbase[seg] >> 4, off, [msg, hdc, hsys])

    # --- API dispatch
    def intr(self, uc, intno, ud):
        if intno == 0x21: return self.dos(uc)
        if intno != 0xFE: raise Exception('INT %x at %04x:%04x segs %s' % (intno, uc.reg_read(UC_X86_REG_CS), uc.reg_read(UC_X86_REG_IP), [hex(x>>4) for x in self.segbase]))
        ip = uc.reg_read(UC_X86_REG_IP)
        mod, key, fn = self.thunks[(ip - 2) // 8]
        name, argt = fn[0], fn[1]
        sp = SEG_STACK * 16 + uc.reg_read(UC_X86_REG_SP)
        # stack: [ret ip, ret cs, last arg ... first arg]
        vals = []; p = sp + 4
        for t in reversed(argt):
            if ASZ.get(t, 2) == 4: vals.append(struct.unpack('<I', uc.mem_read(p, 4))[0]); p += 4
            else:
                v = self.rd16(p); vals.append(v - 0x10000 if t == 's_word' and v & 0x8000 else v); p += 2
        vals.reverse()
        self.calls[f'{mod}.{name}'] += 1
        if os.environ.get('TRACE'): print('API', mod, name, vals)
        f = getattr(self, 'api_' + name, None)
        if f is None:
            self.log.append(('UNIMPL', mod, name, vals)); r = 0
        else:
            r = f(*vals)
        if r is None: r = 0
        uc.reg_write(UC_X86_REG_AX, r & 0xffff); uc.reg_write(UC_X86_REG_DX, (r >> 16) & 0xffff)

    def dos(self, uc):
        ah = uc.reg_read(UC_X86_REG_AH)
        self.calls['DOS.%02X' % ah] += 1
        if ah == 0x2C:   # time
            t = self.tick // 10
            uc.reg_write(UC_X86_REG_CX, (12 << 8) | (t // 6000 % 60)); uc.reg_write(UC_X86_REG_DX, ((t // 100 % 60) << 8) | (t % 100))
        elif ah == 0x2A: uc.reg_write(UC_X86_REG_CX, 1990); uc.reg_write(UC_X86_REG_DX, 0x0101)
        elif ah == 0x30: uc.reg_write(UC_X86_REG_AX, 0x1E03)
        else: self.log.append(('DOS', '%02X' % ah))

    # KERNEL
    def api_LocalInit(self, seg, s, e): return 1
    def api_LocalAlloc(self, fl, n):
        if not hasattr(self, 'lh'): self.lh = 0x8000
        r = self.lh; self.lh += (n + 3) & ~1; return r
    def api_LocalLock(self, h): return h
    def api_LocalUnlock(self, h): return 1
    def api_LocalFree(self, h): return 0
    def api_GetVersion(self): return 0x0003
    def api_GlobalLock(self, h):
        o = self.handles.get(h)
        return 0 if not o or o.get('kind') != 'global' else ((o['addr'] >> 4) << 16)
    def api_GlobalUnlock(self, h): return 0
    def api_OutputDebugString(self, s): self.log.append(('DBG', self.cstr(s)))
    def api_GetDOSEnvironment(self): return (0x8000 << 16)
    def api_FatalAppExit(self, a, s): raise Exception('FatalAppExit ' + self.cstr(s))
    # USER
    def api_GetCurrentTime(self): return self.tick
    def api_GetTickCount(self): return self.tick
    def api_SetRect(self, p, l, t, r, b):
        a = self.far(p); self.uc.mem_write(a, struct.pack('<hhhh', l, t, r, b)); return 1
    def rect(self, p): return struct.unpack('<hhhh', self.uc.mem_read(self.far(p), 8))
    def api_IntersectRect(self, d, a, b):
        l1, t1, r1, b1 = self.rect(a); l2, t2, r2, b2 = self.rect(b)
        l, t, r, bb = max(l1, l2), max(t1, t2), min(r1, r2), min(b1, b2)
        if l >= r or t >= bb: l = t = r = bb = 0
        self.uc.mem_write(self.far(d), struct.pack('<hhhh', l, t, r, bb)); return int(l < r)
    def api_GetKeyState(self, k): return 0
    def api_PeekMessage(self, *a): return 0
    def api_PostMessage(self, *a): return 1
    def api_GetCapture(self): return 0
    def api_LoadBitmap(self, hinst, name):
        key = name & 0xffff if name >> 16 == 0 else self.cstr(name).upper()
        d = self.m.res[(2, key)]
        hs, w, h, pl, bpp = struct.unpack_from('<IiiHH', d, 0)
        nc = struct.unpack_from('<I', d, 32)[0] or (1 << bpp)
        pal = [tuple(d[hs + 4 * k:hs + 4 * k + 3][::-1]) for k in range(nc)]
        px = d[hs + 4 * nc:]; stride = ((w * bpp + 31) // 32) * 4
        arr = np.zeros((h, w), np.uint8)
        for y in range(h):
            row = px[(h - 1 - y) * stride:(h - y) * stride]
            for x in range(w):
                v = (row[x // 2] >> (4 * (1 - x % 2))) & 15 if bpp == 4 else (row[x // 8] >> (7 - x % 8)) & 1
                c = pal[v]
                arr[y, x] = PAL16.index(c) if c in PAL16 else 15 * (sum(c) > 384)
        return self.newh({'kind': 'bitmap', 'w': w, 'h': h, 'bpp': bpp, 'px': arr, 'name': key})
    def api_FillRect(self, hdc, p, hbr):
        l, t, r, b = self.rect(p); px = self.dcpx(hdc)
        px[max(t, 0):max(b, 0), max(l, 0):max(r, 0)] = self.handles[hbr]['color']
        self.blits.append(('FillRect', r - l, b - t)); return 1
    def api_InvertRect(self, hdc, p):
        l, t, r, b = self.rect(p); px = self.dcpx(hdc)
        px[max(t, 0):max(b, 0), max(l, 0):max(r, 0)] ^= 15
        self.blits.append(('InvertRect', r - l, b - t))
    # GDI
    STOCK = {0: 15, 1: 8, 2: 7, 3: 8, 4: 0, 5: None}
    def api_GetStockObject(self, i):
        return self.newh({'kind': 'brush', 'color': self.STOCK.get(i, 0) or 0, 'stock': i})
    def api_CreateSolidBrush(self, c):
        rgb = (c & 255, (c >> 8) & 255, (c >> 16) & 255)
        i = min(range(16), key=lambda k: sum((a - b) ** 2 for a, b in zip(PAL16[k], rgb)))
        return self.newh({'kind': 'brush', 'color': i})
    def api_CreateCompatibleDC(self, hdc): return self.newh({'kind': 'dc', 'bmp': None})
    def api_DeleteDC(self, h): self.handles.pop(h, None); return 1
    def api_DeleteObject(self, h): self.handles.pop(h, None); return 1
    def api_SelectObject(self, hdc, h):
        dc = self.handles[hdc]; o = self.handles.get(h, {})
        if o.get('kind') == 'bitmap': old = dc.get('bmph', 0); dc['bmph'] = h; dc['bmp'] = o; return old or 1
        return 1
    def api_GetObject(self, h, n, p):
        o = self.handles[h]
        if o['kind'] == 'bitmap':
            bits = struct.pack('<hhhhBBI', 0, o['w'], o['h'], (o['w'] * o['bpp'] + 15) // 16 * 2, 1, o['bpp'], 0)
            self.uc.mem_write(self.far(p), bits[:n]); return min(n, len(bits))
        return 0
    def dcpx(self, hdc):
        if hdc == self.hscreen: return self.screen
        return self.handles[hdc]['bmp']['px']
    def api_BitBlt(self, hd, x, y, w, h, hs, xs, ys, rop):
        d = self.dcpx(hd); s = self.dcpx(hs) if hs else None
        nm = None
        if hs and self.handles.get(hs, {}).get('bmp'): nm = self.handles[hs]['bmp'].get('name')
        self.blits.append(('BitBlt', '%06X' % rop, w, h, hd == self.hscreen, nm))
        # clip
        x0, y0, x1, y1 = max(x, 0), max(y, 0), min(x + w, d.shape[1]), min(y + h, d.shape[0])
        if x0 >= x1 or y0 >= y1: return 1
        dv = d[y0:y1, x0:x1]
        sv = s[ys + y0 - y:ys + y1 - y, xs + x0 - x:xs + x1 - x] if s is not None else None
        r = rop >> 16
        if r == 0xCC: dv[...] = sv
        elif r == 0x88: dv &= sv
        elif r == 0xEE: dv |= sv
        elif r == 0x66: dv ^= sv
        elif r == 0x44: dv[...] = sv & (~dv & 15)
        elif r == 0x22: dv &= (~sv & 15)
        elif r == 0x33: dv[...] = ~sv & 15
        elif r == 0x00: dv[...] = 0
        elif r == 0xFF: dv[...] = 15
        elif r == 0x55: dv ^= 15
        else: self.log.append(('ROP?', '%06X' % rop))
        return 1

    def png(self, fn):
        rgb = np.array(PAL16, np.uint8)[self.screen]
        Image.fromarray(rgb).save(fn)


def main():
    path = sys.argv[1]; frames = int(sys.argv[2]) if len(sys.argv) > 2 else 200
    out = sys.argv[3] if len(sys.argv) > 3 else '/tmp/claude-0/w16'
    s = Sim(path)
    print('LibEntry ->', s.libentry())
    # AD_SYSTEM / AD_MODULE
    hmod, amod = s.galloc(64); hsys, asys = s.galloc(64)
    ctl = []
    for i in range(1, 5):
        d = s.m.res.get((1000, i))
        ctl.append(0)
    # control defaults: resource 1000 'iControlValue' unknown layout -> use sensible values
    ctl = [int(v) for v in os.environ.get("CTL", "10,1,1,0").split(",")]
    s.uc.mem_write(amod, struct.pack('<H hh 4h 4h H H I', 0, 640, 400, *ctl, 1, 2, 3, 4, 0x50, 0, 0))
    s.hscreen = s.newh({'kind': 'dc', 'screen': True})
    s.uc.mem_write(asys, struct.pack('<hhhhhhhhhhhhhhhhHIHh', 0, 0, 0, 640, 400, 4, 1, 1, 96, 96, 0x100,
                                     0, 0, 0, 0, 0, hmod, 0, 0, 0))
    for msg in (0, 1):
        print('msg', msg, '->', s.module(msg, s.hscreen, hsys))
    for f in range(frames):
        s.tick += 18
        n0 = len(s.blits)
        s.module(2, s.hscreen, hsys)
        if f in (0, 1, 50, frames - 1):
            print('frame', f, collections.Counter(b[5] for b in s.blits[n0:] if len(b) > 5), '... total', len(s.blits) - n0)
        if f % 50 == 49: s.png(f'{out}_{f+1:04d}.png')
    s.module(3, s.hscreen, hsys)
    print(s.calls.most_common())
    print([l for l in s.log][:20])

if __name__ == '__main__':
    main()
