"""rommap.py - SC-55 mk1 control ROM layout (as decoded in docs/SC55ROM.md), python side for the experiments."""
import struct, os
ROMS = os.environ.get('SC55ROMS', '/tmp/dl/spx/tmp/p52/NUKEDSB/FILES/ROMS')
D = open(os.path.join(ROMS, 'M1_ROM2.BIN'), 'rb').read()
BLK = [(0x10000, 0x1BD00, 0x1DEC0), (0x20000, 0x2BD00, 0x2DEC0)]
def inst(i):
    b = BLK[i // 224]; a = b[0] + (i % 224) * 0xD8; r = D[a:a + 0xD8]
    parts = []
    for p in range(2):
        q = r[32 + 92 * p:32 + 92 * (p + 1)]
        parts.append(dict(idx=q[2] << 8 | q[3], u1=q[1], pp=q[4:], addr=a + 36 + 92 * p))
    return dict(name=r[:12].decode('latin1').rstrip(), hdr=r[12:32], parts=parts, addr=a)
def part(i):
    b = BLK[i // 144]; a = b[1] + (i % 144) * 0x3C; r = D[a:a + 0x3C]
    return dict(name=r[:12].decode('latin1').rstrip(), breaks=list(r[12:28]), samples=list(struct.unpack('>16H', r[28:60])), addr=a)
def sample(i):
    b = BLK[i // 532]; a = b[2] + (i % 532) * 16; r = D[a:a + 16]
    off = r[1] << 16 | r[2] << 8 | r[3]; ae, sl, ll = struct.unpack('>HHH', r[4:10]); p, fv = struct.unpack('>HH', r[12:16])
    return dict(vol=r[0], off=off, ae=ae, len=sl, loop=ll, mode=r[10], root=r[11], pitch=p, fvol=fv, addr=a)
def variation(bank, prog):
    a = 0x30000 + bank * 256 + prog * 2; return D[a] << 8 | D[a + 1]
def sample_by_start(off):
    for i in range(1064):
        if sample(i)['off'] == off: return i
