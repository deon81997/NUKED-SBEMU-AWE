; SPDX-License-Identifier: GPL-2.0-or-later - part of NUKED SB-EMU, https://github.com/deon81997/NUKED-SBEMU-MUXED
; HMIMPU.COM [n|S] - the MPU-401 code of HMI SOS (Descent 1.4 HMIMDRV.386 "mpu401.com", byte-exact loops) in a DOS box:
; n times (default 3): init (mask the PIC but IRQ 1, FFh, wait DSR 0FFFFh, read ACK, wait DRR 0FFFFh, 3Fh, 0FFFFh reads,
; read), 64 MIDI bytes with the DRR wait (0FFFFh), then the driver's reset (mask, wait DSR 3FFFFh, FFh, loop WHILE DSR
; up to 0FFFFFh, read, EOI, unmask) - the one Descent runs at a song change. Prints the loop counts per step:
;   HMIMPU i=<init DSR loops> a=<ACK byte> s=<max DRR loops per byte> p=<pre-reset loops> w=<post-FFh loops> r=<byte>
; A real MPU-401 (ACK after some us): w=1. SC55W.VXD before beta.131-awe.3: w=1048575 (~6 s of trapped reads, PIC masked).
        cpu     386
        org     100h
        mov     word [cnt], 3
        mov     si, 81h
sk:     lodsb
        cmp     al, ' '
        je      sk
        mov     ah, al
        and     ah, 0DFh
        cmp     ah, 'S'
        je      spin
        sub     al, '0'
        cmp     al, 9
        ja      top
        mov     [cnt], al
top:    mov     dx, 331h
        ; ---- init (0x183) ----
        in      al, 21h
        mov     [msk], al
        or      al, 0FDh
        out     21h, al
        mov     al, 0FFh
        out     dx, al
        mov     ecx, 0FFFFh
i1:     in      al, dx
        test    al, 80h
        jz      i2
        loop    i1, ecx
i2:     mov     eax, 0FFFFh
        sub     eax, ecx
        mov     [vi], eax
        dec     dx
        in      al, dx
        mov     [va], al
        inc     dx
        mov     ecx, 0FFFFh
i3:     in      al, dx
        test    al, 40h
        jz      i4
        loop    i3, ecx
i4:     mov     al, 3Fh
        out     dx, al
        mov     ecx, 0FFFFh
i5:     in      al, dx
        loop    i5, ecx
        dec     dx
        in      al, dx
        inc     dx
        mov     al, 20h
        out     20h, al
        mov     al, [msk]
        out     21h, al
        ; ---- 64 bytes (0x297): note on/off pairs, ch 10 ----
        mov     dword [vs], 0
        mov     bx, 0
b1:     mov     ecx, 0FFFFh
b2:     in      al, dx
        test    al, 40h
        jz      b3
        loop    b2, ecx
b3:     mov     eax, 0FFFFh
        sub     eax, ecx
        cmp     eax, [vs]
        jbe     b4
        mov     [vs], eax
b4:     dec     dx
        mov     al, [msg+bx]
        out     dx, al
        inc     dx
        inc     bx
        and     bx, 7
        dec     word [bn]
        jnz     b1
        mov     word [bn], 64
        ; ---- reset (0x21d) ----
        in      al, 21h
        mov     [msk], al
        or      al, 0FDh
        out     21h, al
        mov     ecx, 3FFFFh
r1:     in      al, dx
        test    al, 80h
        jz      r2
        loop    r1, ecx
r2:     mov     eax, 3FFFFh
        sub     eax, ecx
        mov     [vp], eax
        mov     al, 0FFh
        out     dx, al
        mov     ecx, 0FFFFFh
r3:     in      al, dx
        test    al, 80h
        jnz     r4
        loop    r3, ecx
r4:     mov     eax, 0FFFFFh
        sub     eax, ecx
        mov     [vw], eax
        dec     dx
        in      al, dx
        mov     [vr], al
        mov     al, 20h
        out     20h, al
        mov     al, [msk]
        out     21h, al
        ; ---- print ----
        mov     di, obuf
        mov     si, t_i
        call    str
        mov     eax, [vi]
        call    dec32
        mov     si, t_a
        call    str
        mov     al, [va]
        call    hex8
        mov     si, t_s
        call    str
        mov     eax, [vs]
        call    dec32
        mov     si, t_p
        call    str
        mov     eax, [vp]
        call    dec32
        mov     si, t_w
        call    str
        mov     eax, [vw]
        call    dec32
        mov     si, t_r
        call    str
        mov     al, [vr]
        call    hex8
        mov     ax, 0A0Dh
        stosw
        mov     byte [di], '$'
        mov     dx, obuf
        mov     ah, 9
        int     21h
        dec     byte [cnt]
        jnz     top
        mov     ax, 4C00h
        int     21h
spin:   mov     dx, 331h                ; HMIMPU S: 2,000,000 status reads in a row (beta.131-awe.3: time slice given up)
        mov     ecx, 2000000
sp1:    in      al, dx
        loop    sp1, ecx
        mov     al, 0FFh
        out     dx, al
        dec     dx
        in      al, dx
        mov     dx, t_sp
        mov     ah, 9
        int     21h
        mov     ax, 4C00h
        int     21h
str:    lodsb
        or      al, al
        jz      .e
        stosb
        jmp     str
.e:     ret
dec32:  mov     ebx, 10
        xor     cx, cx
.d:     xor     edx, edx
        div     ebx
        push    dx
        inc     cx
        or      eax, eax
        jnz     .d
.p:     pop     ax
        add     al, '0'
        stosb
        loop    .p
        ret
hex8:   mov     ah, al
        shr     al, 4
        call    .n
        mov     al, ah
        and     al, 0Fh
.n:     add     al, '0'
        cmp     al, '9'
        jbe     .o
        add     al, 7
.o:     stosb
        ret
t_sp    db 'HMIMPU S 2000000 reads done', 13, 10, '$'
t_i     db 'HMIMPU i=', 0
t_a     db ' a=', 0
t_s     db ' s=', 0
t_p     db ' p=', 0
t_w     db ' w=', 0
t_r     db ' r=', 0
msg     db 99h, 24h, 40h, 89h, 24h, 0, 0FEh, 0FEh
bn      dw 64
cnt     dw 3
msk     db 0
vi      dd 0
va      db 0
vs      dd 0
vp      dd 0
vw      dd 0
vr      db 0
obuf    times 96 db 0
