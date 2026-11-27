#!/usr/bin/env python3
"""mkfloppy.py RELEASE.zip OUTDIR TOOLDIR: NUKED SB-EMU release zip -> 1.44 MB install disk images DISK1.IMG ..
One archive stream (NSBPAK2: per file u16 namelen, name, u32 size, u32 xzlen, xz) split over DISKn.PAK volumes
(NSBVOL2 header: u8 disk, u8 ndisks, u16 0, u32 set id, u32 stream offset); files may span disks."""
import sys, os, io, zipfile, lzma, struct, subprocess, zlib
rel, out, tool = sys.argv[1:4]
CAP = 2847 * 512 - 1024                       # FAT12 1.44 MB data area, minus slack
clu = lambda n: -(-n // 512) * 512
z = zipfile.ZipFile(rel)
st = io.BytesIO(); files = [i for i in z.infolist() if not i.is_dir()]
st.write(b'NSBPAK2\0' + struct.pack('<H', len(files)))
for i in files:
    name = i.filename.split('/', 1)[1].replace('/', '\\'); data = z.read(i)
    bin_ = name.upper().endswith(('.EXE', '.DLL', '.VXD', '.DRV', '.SYS', '.COM', '.386', '.OBJ'))
    filt = ([{'id': lzma.FILTER_X86}] if bin_ else []) + [{'id': lzma.FILTER_LZMA2, 'preset': 9 | lzma.PRESET_EXTREME, 'dict_size': 1 << 22}]
    xz = lzma.compress(data, format=lzma.FORMAT_XZ, check=lzma.CHECK_CRC32, filters=filt)
    nb = name.encode('ascii'); st.write(struct.pack('<H', len(nb)) + nb + struct.pack('<II', len(data), len(xz)) + xz)
S = st.getvalue(); setid = zlib.crc32(S)
inst = {n: open(os.path.join(tool, n), 'rb').read() for n in ('INSTALL.BAT', 'INST2.BAT', 'UNPAK.EXE', 'FLOPPY.TXT')}
inst['HDPMI32I.EXE'] = z.read(next(n for n in z.namelist() if n.upper().endswith('FILES/HDPMI/HDPMI32I.EXE')))
caps = []; left = len(S)
while left > 0:
    c = CAP - 20 - (sum(clu(len(b)) for b in inst.values()) if not caps else 0); caps.append(min(c, left)); left -= c
N = len(caps); os.makedirs(out, exist_ok=True); off = 0
for k, c in enumerate(caps, 1):
    vol = b'NSBVOL2\0' + bytes([k, N]) + b'\0\0' + struct.pack('<II', setid, off) + S[off:off + c]; off += c
    fl = {f'DISK{k}.PAK': vol}
    if k == 1: fl.update(inst)
    img = os.path.join(out, f'DISK{k}.IMG'); open(img, 'wb').close()
    subprocess.run(['mformat', '-i', img, '-f', '1440', '-v', f'NUKEDSB {k}', '-C', '::'], check=True)
    for n, b in fl.items():
        tmp = os.path.join(out, '_' + n); open(tmp, 'wb').write(b)
        subprocess.run(['mcopy', '-i', img, tmp, '::' + n], check=True); os.remove(tmp)
    print(f'DISK{k}.IMG  pak {len(vol)//1024:5d} K')
print(f'{N} disks, {len(files)} files, {sum(i.file_size for i in files)//1024} K unpacked, stream {len(S)//1024} K, set {setid:08x}')
open(os.path.join(out, 'NDISKS'), 'w').write(str(N))
