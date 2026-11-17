#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later - part of NUKED SB-EMU (testrig/sbset)
"""keys.py ZIP [EXE...] - SETTINGS (SBSET) driven by keys in QEMU (MS-DOS 7.1, store-dos71), beta.130-awe.8 AWE32 page.
SETUP C installs the pack (+ AWE32.RAW from store-personal into ROMS), SBOPTS.TXT starts with options SBSET has no
field for. Keys: MIDI synth -> AWE32 (Left), Enter = AWE32 options, port 640, RAM 4 MB, reverb/chorus off, MPU-401 =
AWE32 GM; Esc, S. Then SBSET /LINE, SBSET /PAGE:AWE (reload shown), F3 style on the page, SC55.BAT (SBEMU start line),
MPUTEST + SBDIAG, MENU's status (AWE32 + own GM). Out: /tmp/sbsetkeys/{*.txt screens, LINE.TXT, SBOPTS.TXT, SB.TXT, DIAG.TXT}; last line PASS/FAIL."""
import os, shutil, socket, subprocess, sys, time, zipfile
W = '/tmp/sbsetkeys'; IMG = os.environ.get('DOS71', '/home/claude/vms/dos71/msdos71.img')
ROM = os.environ.get('AWEROM', '/home/claude/tmp/p52/NUKEDSB/FILES/ROMS/AWE32.RAW')
OFF = '@@32256'

def sh(*a): subprocess.run(a, check=True)

def build(zp, exes):
    shutil.rmtree(W, ignore_errors=True); os.makedirs(W + '/c')
    zipfile.ZipFile(zp).extractall(W + '/c')
    for e in exes: shutil.copy(e, W + '/c/NUKEDSB/FILES/SC55/' + os.path.basename(e).upper())
    shutil.copy(ROM, W + '/c/NUKEDSB/FILES/ROMS/AWE32.RAW')
    open(W + '/c/T.BAT', 'w', newline='\r\n').write(r'''@ECHO OFF
C:
CD \NUKEDSB
CALL SETUP C > NUL
ECHO /A:220 /I:5 /D:1 /H:5 /T:5 /P:330 /VSCLPF:1 /FOO:BAR> C:\SC55\SBOPTS.TXT
C:\HDPMI\HDPMI32I.EXE -r > NUL
CD \SC55
ECHO STAGE1
SBSET.EXE C:\SC55
SBSET.EXE C:\SC55 /LINE > C:\LINE.TXT
COPY C:\SC55\SBOPTS.TXT C:\OPTS.TXT > NUL
ECHO STAGE2
SBSET.EXE C:\SC55 /PAGE:AWE
CALL C:\SC55\SC55.BAT > C:\SB.TXT
SBDIAG START > NUL
MPUTEST > NUL
SBDIAG > C:\DIAG.TXT
ECHO ALLDONE
SC55MENU.EXE C:\SC55
''')
    open(W + '/c/AUTOEXEC.BAT', 'w', newline='\r\n').write('@ECHO OFF\nPATH C:\\DOS;C:\\SC55\nCALL C:\\T.BAT\n')
    shutil.copy(IMG, W + '/hd.img')
    for n in ('NUKEDSB', 'T.BAT', 'AUTOEXEC.BAT'): sh('mcopy', '-o', '-s', '-i', W + '/hd.img' + OFF, W + '/c/' + n, '::/')

class Q:
    def __init__(s):
        s.p = subprocess.Popen(['qemu-system-i386', '-machine', 'pc', '-cpu', 'pentium3', '-m', '64', '-vga', 'std',
            '-display', 'none', '-drive', 'file=%s/hd.img,format=raw,if=ide,cache=writeback' % W, '-boot', 'c',
            '-audiodev', 'wav,id=snd,path=%s/main.wav,timer-period=2000' % W, '-device', 'AC97,audiodev=snd',
            '-monitor', 'unix:%s/mon,server,nowait' % W], stdout=open(W + '/qemu.log', 'w'), stderr=subprocess.STDOUT)
        for _ in range(50):
            try: s.m = socket.socket(socket.AF_UNIX); s.m.connect(W + '/mon'); break
            except OSError: time.sleep(0.2)
        s.cmd('info status')
    def cmd(s, c):
        s.m.sendall((c + '\n').encode()); time.sleep(0.15)
        s.m.settimeout(0.3)
        try:
            while s.m.recv(65536): pass
        except OSError: pass
    def screen(s):
        f = W + '/scr.bin'
        if os.path.exists(f): os.remove(f)
        s.cmd('pmemsave 0xb8000 4000 "%s"' % f)
        for _ in range(20):
            if os.path.exists(f) and os.path.getsize(f) == 4000: break
            time.sleep(0.1)
        b = open(f, 'rb').read()
        return '\n'.join(bytes(b[r * 160 + c * 2] for c in range(80)).decode('cp437') for r in range(25))
    def wait(s, txt, secs=120):
        t = time.time()
        while time.time() - t < secs:
            sc = s.screen()
            if txt in sc: return sc
            time.sleep(0.5)
        raise SystemExit('timeout waiting for %r\n%s' % (txt, s.screen()))
    def key(s, *ks, gap=0.35):   # gray arrows (E0 keys) arrive twice in QEMU's DOS: keypad keys (NumLock off) instead
        kp = {'down': 'kp_2', 'up': 'kp_8', 'left': 'kp_4', 'right': 'kp_6'}
        for k in ks: s.cmd('sendkey ' + kp.get(k, k)); time.sleep(gap)
    def save(s, name):
        sc = s.screen(); open('%s/%s.txt' % (W, name), 'w').write(sc); return sc

def main():
    zp, exes = sys.argv[1], sys.argv[2:]
    build(zp, exes); q = Q(); ok = True
    try:
        q.wait('SBEMU Settings', 240); q.save('1-main')
        q.key(*['down'] * 6); q.key('left')                  # MIDI synth -> AWE32 / AWE64 (wraps from the SC-55)
        sc = q.save('2-synth-awe'); ok &= 'AWE32 620h, 512 KB, fx on' in sc
        q.key('ret'); sc = q.wait('AWE32 options', 10); q.save('3-awepage')
        ok &= 'auto: AWE32.RAW' in sc; print('ROM shown', 'auto: AWE32.RAW' in sc)
        q.key('down', 'right')                               # port 640
        q.key('down', 'right', 'right', 'right')             # 4 MB (AWE64 Gold)
        q.key('down', 'down', 'right')                       # (ROM: auto) reverb + chorus off
        q.key('down', 'left', 'left', 'left')                # MPU-401: SC-55 -> MT-32 itself -> GS -> GM
        sc = q.save('4-awepage-set')
        for t in ('640h', '4 MB (AWE64 Gold)', 'Off (dry)', 'AWE32 itself, GM'): ok &= t in sc; print(t, t in sc)
        q.cmd('screendump %s/4.ppm' % W)
        q.key('f3'); time.sleep(1); q.save('5-awepage-f3')   # the next style (F3) on the new page
        q.cmd('screendump %s/5.ppm' % W)
        q.key('esc'); q.key('s'); time.sleep(1); q.save('6-saved'); q.key('esc')
        sc = q.wait('AWE32 options', 60); q.save('7-reload')
        for t in ('640h', '4 MB (AWE64 Gold)', 'Off (dry)', 'AWE32 itself, GM', 'auto: AWE32.RAW'): ok &= t in sc
        q.key('esc')
        sc = q.wait('AWE32 +', 400); q.save('8-menu'); ok &= 'AWE32 + own GM' in sc; print('MENU status', 'AWE32 + own GM' in sc)
    finally:
        q.cmd('quit'); time.sleep(1); q.p.kill()
    for n, src in (('LINE.TXT', 'LINE.TXT'), ('OPTS.TXT', 'OPTS.TXT'), ('SB.TXT', 'SC55/SBLOG.TXT'), ('DIAG.TXT', 'DIAG.TXT')):
        subprocess.run('mtype -i %s/hd.img%s ::/%s | tr -d "\\r" > %s/%s' % (W, OFF, src, W, n), shell=True)
    line = open(W + '/OPTS.TXT').read().strip(); print('SBOPTS.TXT:', line)
    want = ['/AWE:640', '/AWERAM:4096', '/AWEMPU:GM', '/AWEFX:0', '/VSCLPF:1', '/FOO:BAR']
    for t in want: ok &= (' ' + t) in ' ' + line + ' '
    ok &= '/AWEROM' not in line and open(W + '/LINE.TXT').read().strip() == line
    sb = open(W + '/SB.TXT', errors='replace').read()
    for l in sb.splitlines():
        if 'AWE' in l or 'MPU' in l: print('SB:', l)
    ok &= 'address 640, 4096 KB RAM, no reverb/chorus' in sb and 'plays on the AWE32' in sb
    dg = open(W + '/DIAG.TXT', errors='replace').read()
    for l in dg.splitlines():
        if 'AWE' in l: print('DIAG:', l)
    # (SBDIAG's AWE32 line needs rendered audio: QEMU's AC97 here reports output 0 Hz - checked on 86Box instead)
    print('PASS' if ok else 'FAIL')
    return 0 if ok else 1
sys.exit(main())
