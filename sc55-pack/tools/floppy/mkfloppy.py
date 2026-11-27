#!/usr/bin/env python3
"""mkfloppy.py RELEASE.zip OUTDIR TOOLDIR: NUKED SB-EMU release zip -> 1.44 MB install disk images (DISK1.IMG ..)."""
import sys, os, io, zipfile, lzma, struct, subprocess, shutil
rel, out, tool = sys.argv[1:4]
CAP = 2847 * 512                      # FAT12 1.44 MB data area
clu = lambda n: -(-n // 512) * 512
z = zipfile.ZipFile(rel); ver = os.path.basename(rel).replace('NUKED-SB-EMU-1.0.0-', '').replace('-usb.zip', '')
items = []
for i in z.infolist():
    if i.is_dir(): continue
    name = i.filename.split('/', 1)[1].replace('/', '\\')
    data = z.read(i)
    bin_ = name.upper().endswith(('.EXE', '.DLL', '.VXD', '.DRV', '.SYS', '.COM', '.386', '.OBJ'))
    filt = ([{'id': lzma.FILTER_X86}] if bin_ else []) + [{'id': lzma.FILTER_LZMA2, 'preset': 9 | lzma.PRESET_EXTREME, 'dict_size': 1 << 22}]
    xz = lzma.compress(data, format=lzma.FORMAT_XZ, check=lzma.CHECK_CRC32, filters=filt)
    items.append((name, data, xz))
inst = {n: open(os.path.join(tool, n), 'rb').read() for n in ('INSTALL.BAT', 'INST2.BAT', 'UNPAK.EXE', 'FLOPPY.TXT')}
inst['HDPMI32I.EXE'] = z.read('NUKEDSB/FILES/HDPMI/HDPMI32I.EXE')
res1 = sum(clu(len(b)) for b in inst.values()) + 2 * 512
ent = lambda it: 2 + len(it[0]) + 8 + len(it[2])
disks = []
for it in sorted(items, key=lambda it: -len(it[2])):     # first fit decreasing
    for d in disks:
        if d['used'] + ent(it) + clu(1) <= d['cap']: d['f'].append(it); d['used'] += ent(it); break
    else:
        disks.append(dict(cap=CAP - (res1 if not disks else 512) - 512, used=12 + ent(it), f=[it]))
N = len(disks); os.makedirs(out, exist_ok=True)
for k, d in enumerate(disks, 1):
    pak = io.BytesIO(); pak.write(b'NSBPAK1\0' + bytes([k, N]) + struct.pack('<H', len(d['f'])))
    for name, data, xz in sorted(d['f']):
        nb = name.encode('ascii'); pak.write(struct.pack('<H', len(nb)) + nb + struct.pack('<II', len(data), len(xz)) + xz)
    files = {f'DISK{k}.PAK': pak.getvalue()}
    if k == 1: files.update(inst)
    img = os.path.join(out, f'DISK{k}.IMG'); open(img, 'wb').close()
    subprocess.run(['mformat', '-i', img, '-f', '1440', '-v', f'NUKEDSB {k}', '-C', '::'], check=True)
    for n, b in files.items():
        tmp = os.path.join(out, '_' + n); open(tmp, 'wb').write(b)
        subprocess.run(['mcopy', '-i', img, tmp, '::' + n], check=True); os.remove(tmp)
    free = subprocess.run(['minfo', '-i', img, '::'], capture_output=True, text=True).stdout
    print(f'DISK{k}.IMG  {len(d["f"]):3d} files  pak {len(files[f"DISK{k}.PAK"])//1024:5d} K')
print(f'{N} disks, {sum(len(i[1]) for i in items)//1024} K unpacked, version {ver}')
open(os.path.join(out, 'NDISKS'), 'w').write(str(N))
