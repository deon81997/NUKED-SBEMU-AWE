; SPDX-License-Identifier: GPL-2.0-or-later - part of NUKED SB-EMU (AWE test rig)
; awedet.com - real-mode EMU8000 probe at 620h (word and byte I/O), as AWE drivers detect it
; prints "ID ok", "HWCF1 ok", "HWCF2 ok", "BYTE ok", "RAM ok" or FAIL lines
        org 100h
start:
        ; 1) Data3 reg 7 (ID): Pointer = 00E0h, IN AX,E20h -> low nibble 0Ch
        mov dx, 0E22h
        mov ax, 00E0h
        out dx, ax
        mov dx, 0E20h
        in ax, dx
        and al, 0Fh
        cmp al, 0Ch
        mov si, m_id
        call result
        ; 2) HWCF1: Data1 reg 1 ch 29 -> Pointer 3Dh, IN AX,A20h; & 7Eh == 58h
        mov dx, 0E22h
        mov ax, 003Dh
        out dx, ax
        mov dx, 0A20h
        in ax, dx
        and al, 7Eh
        cmp al, 58h
        mov si, m_h1
        call result
        ; 3) HWCF2: ch 30 -> 3Eh; & 03h == 03h
        mov dx, 0E22h
        mov ax, 003Eh
        out dx, ax
        mov dx, 0A20h
        in ax, dx
        and al, 03h
        cmp al, 03h
        mov si, m_h2
        call result
        ; 4) the same ID read with byte I/O: OUT E22h,E0h / OUT E23h,0 / IN AL,E20h
        mov dx, 0E22h
        mov al, 0E0h
        out dx, al
        inc dx
        xor al, al
        out dx, al
        mov dx, 0E20h
        in al, dx
        and al, 0Fh
        cmp al, 0Ch
        mov si, m_by
        call result
        ; 5) sample RAM: SMALW (Data1 ch 22, dword: lo word A20h, hi word A22h) = 200000h,
        ;    SMLD (Data1 ch 26, word) <- 1234h, 5678h; SMALR (ch 20) = 200000h; SMRD (Data2 ch 26) read twice
        mov dx, 0E22h
        mov ax, 0036h           ; ch 22 | reg 1 << 5
        out dx, ax
        mov dx, 0A20h
        xor ax, ax
        out dx, ax              ; low word
        mov dx, 0A22h
        mov ax, 0020h
        out dx, ax              ; high word -> 00200000h
        mov dx, 0E22h
        mov ax, 003Ah           ; ch 26 SMLD
        out dx, ax
        mov dx, 0A20h
        mov ax, 1234h
        out dx, ax
        mov ax, 5678h
        out dx, ax
        mov dx, 0E22h
        mov ax, 0034h           ; ch 20 SMALR
        out dx, ax
        mov dx, 0A20h
        xor ax, ax
        out dx, ax
        mov dx, 0A22h
        mov ax, 0020h
        out dx, ax
        mov dx, 0E22h
        mov ax, 003Ah           ; ch 26: SMLD on Data1 reads the left stream (SMALR)
        out dx, ax
        mov dx, 0A20h
        in ax, dx               ; first read after setting the address: the buffered word
        in ax, dx
        mov bx, ax
        in ax, dx
        mov cx, ax
        mov [w1], bx
        mov [w2], cx
        ; right stream (SMARR = 0, ROM): Data2 ch 26 = SMRD, three reads -> w3 w4 w5 (printed, compared between emulators)
        mov dx, 0A22h
        in ax, dx
        mov [w3], ax
        in ax, dx
        mov [w4], ax
        in ax, dx
        mov [w5], ax
        cmp bx, 1234h
        jne .bad
        cmp cx, 5678h
.bad:   mov si, m_ram
        call result
        mov ax, [w1]
        call hex
        mov ax, [w2]
        call hex
        mov ax, [w3]
        call hex
        mov ax, [w4]
        call hex
        mov ax, [w5]
        call hex
        mov dx, crlf
        mov ah, 9
        int 21h
        mov ax, 4C00h
        int 21h

result: ; ZF = pass, SI = name$
        pushf
        mov dx, si
        mov ah, 9
        int 21h
        popf
        mov dx, s_ok
        je .p
        mov dx, s_fail
.p:     mov ah, 9
        int 21h
        ret
hex:    ; AX -> 4 hex digits + space
        mov cx, 4
.l:     rol ax, 4
        push ax
        and al, 0Fh
        add al, '0'
        cmp al, '9'
        jbe .d
        add al, 7
.d:     mov dl, al
        mov ah, 2
        int 21h
        pop ax
        loop .l
        mov dl, ' '
        mov ah, 2
        int 21h
        ret
w1 dw 0
w2 dw 0
w3 dw 0
w4 dw 0
w5 dw 0
m_id  db 'ID $'
m_h1  db 'HWCF1 $'
m_h2  db 'HWCF2 $'
m_by  db 'BYTE $'
m_ram db 'RAM $'
s_ok  db 'ok', 13, 10, '$'
s_fail db 'FAIL', 13, 10, '$'
crlf  db 13, 10, '$'
