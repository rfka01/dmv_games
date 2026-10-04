# Baut TOPPLER.IMG (360 KB FAT12) mit dem Bootsektor einer vorhandenen DMV-Diskette
import struct, sys, time, os
tmpl, out, label = sys.argv[1], sys.argv[2], sys.argv[3]
files = sys.argv[4:]
img = bytearray(open(tmpl, 'rb').read(512)) + bytearray(368640 - 512)
bps, spc, res, nf, re, ts, md, spf = struct.unpack('<HBHBHHBH', img[11:24])
fat = bytearray(spf * bps)
fat[0:3] = bytes([md, 0xFF, 0xFF])
def setfat(c, v):
    o = c * 3 // 2
    if c & 1:
        fat[o] = (fat[o] & 0x0F) | ((v << 4) & 0xF0); fat[o+1] = (v >> 4) & 0xFF
    else:
        fat[o] = v & 0xFF; fat[o+1] = (fat[o+1] & 0xF0) | ((v >> 8) & 0x0F)
rd = (res + nf * spf) * bps; ds = rd + re * 32
csz = spc * bps
t = time.localtime()
dt = ((t.tm_year - 1980) << 9) | (t.tm_mon << 5) | t.tm_mday
tm = (t.tm_hour << 11) | (t.tm_min << 5) | (t.tm_sec // 2)
ent = 0
def dirent(name, ext, attr, clus, size):
    global ent
    e = struct.pack('<8s3sB10sHHHI', name.ljust(8).encode(), ext.ljust(3).encode(), attr, b'\0'*10, tm, dt, clus, size)
    img[rd + ent*32: rd + ent*32 + 32] = e; ent += 1
dirent(label[:8], label[8:11], 0x08, 0, 0)
nxt = 2
for f in files:
    data = open(f, 'rb').read()
    base = os.path.basename(f).upper()
    n, _, x = base.partition('.')
    nc = (len(data) + csz - 1) // csz
    first = nxt if nc else 0
    for i in range(nc):
        c = nxt + i
        setfat(c, c + 1 if i < nc - 1 else 0xFFF)
        img[ds + (c-2)*csz: ds + (c-2)*csz + csz] = data[i*csz:(i+1)*csz].ljust(csz, b'\0')
    nxt += nc
    dirent(n, x, 0x20, first, len(data))
for i in range(nf):
    img[(res + i*spf)*bps:(res + (i+1)*spf)*bps] = fat
open(out, 'wb').write(img)
used = (nxt - 2) * csz
print(f'{out}: {len(files)} Dateien, {used} Bytes belegt, frei {(ts - ds//bps)//spc*csz - used}')
