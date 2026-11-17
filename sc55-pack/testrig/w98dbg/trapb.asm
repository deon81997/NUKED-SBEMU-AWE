; SPDX-License-Identifier: GPL-2.0-or-later - part of NUKED SB-EMU, https://github.com/deon81997/NUKED-SBEMU-MUXED
; TRAPB.COM [port] - trapped-port speed in a DOS box (w98.9 debug link cost): 128 x 65536 IN from the port
; (default 22Eh = DSP read status, SC55W.VXD answers it) timed with the BIOS tick; prints "TRAPB <ticks>".
; Loops: 128 x 65536 (about 0.4 % resolution on a Pentium II 450).
        org 100h
        mov     dx, 22Eh
        mov     si, 81h
        cmp     byte [80h], 0
        je      go
skip:   lodsb
        cmp     al, ' '
        je      skip
        cmp     al, '0'                 ; no digit (only a redirection's space): keep 22Eh
        jb      go
        xor     dx, dx
hex:    cmp     al, '0'
        jb      go
        sub     al, '0'
        cmp     al, 9
        jbe     dig
        and     al, 0DFh
        sub     al, 7
dig:    shl     dx, 4
        or      dl, al
        lodsb
        jmp     hex
go:     push    dx
        xor     ax, ax
        mov     es, ax
w0:     mov     bx, [es:46Ch]          ; start on a tick edge
w1:     cmp     bx, [es:46Ch]
        je      w1
        mov     bx, [es:46Ch]
        pop     dx
        mov     di, 128
l2:     xor     cx, cx
l1:     in      al, dx
        loop    l1
        dec     di
        jnz     l2
        mov     ax, [es:46Ch]
        sub     ax, bx
        ; print "TRAPB " + decimal ax
        push    ax
        mov     dx, msg
        mov     ah, 9
        int     21h
        pop     ax
        mov     bx, 10
        xor     cx, cx
d1:     xor     dx, dx
        div     bx
        push    dx
        inc     cx
        or      ax, ax
        jnz     d1
d2:     pop     dx
        add     dl, '0'
        mov     ah, 2
        int     21h
        loop    d2
        mov     dx, crlf
        mov     ah, 9
        int     21h
        mov     ax, 4C00h
        int     21h
msg     db 'TRAPB $'
crlf    db 13, 10, '$'
