# Floppy install set (1.44 MB disks)

`mkfloppy.py RELEASE.zip OUTDIR TOOLDIR` packs a release zip onto DISKn.IMG images (beta.134-awe.1: 5 disks).
One archive stream (one xz stream per file, x86 BCJ for binaries, CRC32) split over DISKn.PAK volumes, so files
may span disks (2 MB ROMs); disk 1 also has INSTALL.BAT, INST2.BAT (copied to C:\NUKEDSB\FLOPPY so floppies can be
swapped), UNPAK.EXE, HDPMI32I.EXE, FLOPPY.TXT. UNPAK asks for each disk; on a CRC/read error it names the disk,
waits for a good copy and redoes that file. FLOPPY.TXT.in: @VER@ @N@ @MB@ @WHAT@.
Installed files are byte-identical to the zip (plain DOS shortens DOWNLOADS.TXT and CDROM-D810.TXT).

Build UNPAK.EXE (needs https://github.com/tukaani-project/xz-embedded in X, 0BSD):

    F="-O2 -Wall -I$X/userspace -I$X/linux/include/linux -DXZ_DEC_X86"
    i586-pc-msdosdjgpp-gcc $F -march=i386 -s -o UNPAK.EXE unpak.c $X/linux/lib/xz/xz_{crc32,dec_stream,dec_lzma2,dec_bcj}.c

FLOPPY.TXT = FLOPPY.TXT.in with @VER@ and @N@ filled in (CRLF). Test: `floptest.sh OUTDIR` (86Box automation build,
MS-DOS 7.1: A:\INSTALL C with disk swaps, GOOD=dir also tests a bad disk + retry), then `cmpinst.py ZIP /tmp/flop/hd.img`.
Personal sets (zip + store-personal ROMs/drivers) are private: never commit or attach their images.
