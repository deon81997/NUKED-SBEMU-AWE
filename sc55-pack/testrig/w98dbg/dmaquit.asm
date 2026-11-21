; SPDX-License-Identifier: GPL-2.0-or-later - part of NUKED SB-EMU (awe.7 repro)
; DMAQUIT.COM: SB at 240h, 8-bit DMA 3: starts auto-init 8-bit playback (DSP 1Ch) of a 4 KB buffer and exits to DOS
; WITHOUT stopping it, like a game that crashes or is killed. Then the DOS box closes: SC55W.VXD up to awe.6 kept
; reading that VM's DMA buffer from its timer after the VM was gone = exception 0E in convert().
; nasm -f bin -o DMAQUIT.COM dmaquit.asm
        org 100h
B equ 240h
        mov dx,B+6
        mov al,1
        out dx,al
        mov cx,100
w1:     in al,dx
        loop w1
        xor al,al
        out dx,al
        mov dx,B+0Eh
w2:     in al,dx
        test al,80h
        jz w2
        mov dx,B+0Ah
        in al,dx
        mov al,0D1h             ; speaker on
        call dsp
        ; buffer: physical address of buf, kept inside one 64 KB page
        mov ax,ds
        mov bx,ax
        shl ax,4
        shr bx,12
        add ax,buf
        adc bx,0
        mov si,ax
        neg ax
        cmp ax,4096
        jae okpg
        add si,ax               ; next 64 KB page starts here
        inc bx
okpg:   ; fill with a square wave
        mov di,si
        mov ax,ds
        shl ax,4
        sub di,ax               ; offset of the buffer in DS (DS*16 low word wraps the same way)
        mov cx,4096
        xor ah,ah
fill:   mov al,40h
        test cx,16
        jz f1
        mov al,0C0h
f1:     mov [di],al
        inc di
        loop fill
        mov al,7
        out 0Ah,al              ; mask ch 3
        out 0Ch,al              ; flip-flop
        mov al,59h              ; single, auto-init, read (playback), ch 3
        out 0Bh,al
        mov ax,si
        out 06h,al
        mov al,ah
        out 06h,al
        mov al,bl
        out 82h,al              ; page ch 3
        mov ax,4095
        out 07h,al
        mov al,ah
        out 07h,al
        mov al,3
        out 0Ah,al              ; unmask
        mov al,40h
        call dsp
        mov al,165              ; 11025 Hz
        call dsp
        mov al,48h
        call dsp
        mov al,0FFh
        call dsp
        mov al,07h              ; 2048 bytes per block
        call dsp
        mov al,1Ch              ; auto-init 8-bit
        call dsp
        mov ax,4C00h            ; exit, DMA still running
        int 21h
dsp:    push ax
        mov dx,B+0Ch
d1:     in al,dx
        test al,80h
        jnz d1
        pop ax
        out dx,al
        ret
buf:
