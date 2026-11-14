; SPDX-License-Identifier: GPL-2.0-or-later - part of NUKED SB-EMU (AWE test rig)
; aweload.com - CPU-load test for the AWE32 path: starts 16 looping EMU8000 voices on ROM samples at 620h and exits,
; so the voices keep sounding while another program (MIDPLAY on the SC-55) runs. nasm -f bin -o AWELOAD.COM aweload.asm
; Per channel: envelope off, all generators flat (no LFO/env movement), pitch 1:1, filter open, volume words 0, loop
; 1000h-9000h in the ROM, then DCYSUSV = 7F7Fh (sustain at max) starts the voice. Pointer = channel | register << 5.
        org 100h
        ; HWCF1/2/3 (Data1 reg 1, ch 29/30/31): enable the chip as AWE drivers do
        mov bx, 0A20h
        mov cx, 29 | 1 << 5
        mov ax, 0059h
        call wr
        mov cx, 30 | 1 << 5
        mov ax, 0020h
        call wr
        mov cx, 31 | 1 << 5
        mov ax, 0004h
        call wr
        xor di, di              ; channel
.ch:    mov si, tab
.e:     lodsw                   ; port (0 = end)
        or ax, ax
        jz .start
        mov bx, ax
        lodsw                   ; register number << 5
        mov cx, ax
        or cx, di
        lodsw                   ; value
        call wr
        jmp .e
.start: mov bx, 0A20h           ; DCYSUSV (Data1 reg 5) = 7F7Fh: envelope on -> the voice starts
        mov cx, 5 << 5
        or cx, di
        mov ax, 7F7Fh
        call wr
        inc di
        cmp di, 16
        jb .ch
        mov dx, msg
        mov ah, 9
        int 21h
        mov ax, 4C00h
        int 21h

wr:     ; BX = data port, CX = pointer, AX = word
        push ax
        mov dx, 0E22h
        mov ax, cx
        out dx, ax
        pop ax
        mov dx, bx
        out dx, ax
        ret

; port, reg<<5, value (dword registers: low word on the even port, high word on port+2)
tab:    dw 0A20h, 5 << 5, 0080h        ; DCYSUSV: envelope off while setting up
        dw 0A20h, 4 << 5, 8000h        ; ENVVOL
        dw 0A20h, 6 << 5, 8000h        ; ENVVAL
        dw 0A20h, 7 << 5, 7F7Fh        ; DCYSUS
        dw 0A22h, 4 << 5, 7F7Fh        ; ATKHLDV
        dw 0A22h, 5 << 5, 8000h        ; LFO1VAL
        dw 0A22h, 6 << 5, 7F7Fh        ; ATKHLD
        dw 0A22h, 7 << 5, 8000h        ; LFO2VAL
        dw 0E20h, 0 << 5, 0E000h       ; IP: initial pitch 1:1
        dw 0E20h, 1 << 5, 0FF00h       ; IFATN: cutoff FF, attenuation 0
        dw 0E20h, 2 << 5, 0000h        ; PEFE
        dw 0E20h, 3 << 5, 0000h        ; FMMOD
        dw 0E20h, 4 << 5, 0010h        ; TREMFRQ
        dw 0E20h, 5 << 5, 0010h        ; FM2FRQ2
        dw 0620h, 6 << 5, 1000h        ; PSST low: loop start 001000h
        dw 0622h, 6 << 5, 8000h        ; PSST high: pan 80h
        dw 0620h, 7 << 5, 9000h        ; CSL low: loop end 009000h
        dw 0622h, 7 << 5, 0000h        ; CSL high
        dw 0620h, 1 << 5, 0000h        ; PTRX low
        dw 0622h, 1 << 5, 4000h        ; PTRX high: pitch target 4000h
        dw 0620h, 0 << 5, 0000h        ; CPF low
        dw 0622h, 0 << 5, 4000h        ; CPF high: current pitch 4000h
        dw 0620h, 3 << 5, 0FFFFh       ; VTFT low: filter target open
        dw 0622h, 3 << 5, 0000h        ; VTFT high: volume target 0 (as awe/synth/test/scripts/t01_sine.awe)
        dw 0620h, 2 << 5, 0FFFFh       ; CVCF low: filter open
        dw 0622h, 2 << 5, 0000h        ; CVCF high: current volume 0
        dw 0A20h, 0 << 5, 1000h        ; CCCA low: start 001000h
        dw 0A22h, 0 << 5, 0000h        ; CCCA high: Q 0
        dw 0
msg     db 'AWELOAD: 16 AWE32 voices playing', 13, 10, '$'
