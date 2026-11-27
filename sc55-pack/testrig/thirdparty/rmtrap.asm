; SPDX-License-Identifier: GPL-2.0-or-later - part of NUKED SB-EMU, https://github.com/deon81997/NUKED-SBEMU
; RMTRAP.COM - test rig only: real-mode port trap check (JEMM + QPIEMU -> SBEMU). From V86 mode:
;   SB DSP reset at 226h -> 22Ah must read AAh, DSP version (E1h) must be 2.xx-4.xx, MPU-401 reset (FFh to 331h)
;   must answer FEh at 330h. Prints "RMTRAP PASS dsp=4.xx mpu=FE" (exit 0) or "RMTRAP FAIL <step>" (exit 1).
; Without QPIEMU (HIMEM only) the reads come from the bare bus (FFh) and it fails. Build: nasm -f bin -o RMTRAP.COM rmtrap.asm
org 100h
        mov dx, 226h
        mov al, 1
        out dx, al
        mov cx, 200
d1:     in al, dx               ; ~3 us delay
        loop d1
        xor al, al
        out dx, al
        mov dx, 22Eh
        mov cx, 0FFFFh
w1:     in al, dx
        test al, 80h
        jnz r1
        loop w1
        jmp f_reset
r1:     mov dx, 22Ah
        in al, dx
        cmp al, 0AAh
        jne f_reset
        mov al, 0E1h            ; DSP version
        call dspw
        jc f_ver
        call dspr
        jc f_ver
        mov [vmaj], al
        call dspr
        jc f_ver
        mov [vmin], al
        cmp byte [vmaj], 2      ; SB 2.0 / Pro (3.xx, SBEMU's default /T5) / 16 (4.xx)
        jb f_ver
        cmp byte [vmaj], 4
        ja f_ver
        mov dx, 331h            ; MPU-401 reset
        mov cx, 0FFFFh
w2:     in al, dx
        test al, 40h            ; DRR clear = can write
        jz m1
        loop w2
        jmp f_mpu
m1:     mov al, 0FFh
        out dx, al
        mov cx, 0FFFFh
w3:     in al, dx
        test al, 80h            ; DSR clear = data ready
        jz m2
        loop w3
        jmp f_mpu
m2:     dec dx
        in al, dx
        cmp al, 0FEh
        jne f_mpu
        mov al, 03Fh            ; UART mode (leave it as games do)
        inc dx
        out dx, al
        dec dx
        in al, dx
        mov al, [vmaj]
        add al, '0'
        mov [pv], al
        mov al, [vmin]
        call hex2
        mov [pv+2], ax
        mov dx, pass
        mov ah, 9
        int 21h
        mov ax, 4C00h
        int 21h
f_reset: mov dx, freset
        jmp fail
f_ver:  mov dx, fver
        jmp fail
f_mpu:  mov dx, fmpu
fail:   mov ah, 9
        int 21h
        mov ax, 4C01h
        int 21h
dspw:   mov ah, al              ; write AL to the DSP, CF on time-out
        mov dx, 22Ch
        mov cx, 0FFFFh
dsw1:    in al, dx
        test al, 80h
        jz dsw2
        loop dsw1
        stc
        ret
dsw2:    mov al, ah
        out dx, al
        clc
        ret
dspr:   mov dx, 22Eh            ; read a DSP byte into AL, CF on time-out
        mov cx, 0FFFFh
rdb1:    in al, dx
        test al, 80h
        jnz rdb2
        loop rdb1
        stc
        ret
rdb2:    mov dx, 22Ah
        in al, dx
        clc
        ret
hex2:   mov ah, al              ; AL -> two ASCII hex digits in AX (low byte first)
        shr al, 4
        call nib
        xchg al, ah
        and al, 0Fh
        call nib
        xchg al, ah
        ret
nib:    add al, '0'
        cmp al, '9'
        jbe nb1
        add al, 7
nb1:    ret
vmaj    db 0
vmin    db 0
pass    db 'RMTRAP PASS dsp='
pv      db '?.??', ' mpu=FE', 13, 10, '$'
freset  db 'RMTRAP FAIL dsp reset (no AAh at 22Ah)', 13, 10, '$'
fver    db 'RMTRAP FAIL dsp version (E1h)', 13, 10, '$'
fmpu    db 'RMTRAP FAIL mpu reset (no FEh at 330h)', 13, 10, '$'
