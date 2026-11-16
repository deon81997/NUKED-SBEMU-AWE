; SPDX-License-Identifier: GPL-2.0-or-later - part of NUKED SB-EMU (AWE test rig, awe.3)
; awetime.com - EMU8000 at 620h: how long detection, an AWEUTIL-style init and a sample-RAM upload take
; (Win98 DOS box: SC55W.VXD mirror vs stall reads), plus read-back checks. nasm -f bin -o AWETIME.COM awetime.asm
; Output: "MHZ n", "DET us n ok n" (100 x ID/HWCF1/HWCF2), "RD us100 n" (10000 ID reads: 1/100 us each),
; "INIT us n reads n" (32 voices + INIT1-4 twice, 2 WC waits of 1024 samples), "UP us n" (512 KB upload),
; "VER ok n" (64 words read back), "SIZE us n kb n" (DRAM sizing as the Linux/AWE drivers), "WC d n" (WC over 100 ms)
        cpu 586
        org 100h
%define PTR 0E22h
%define D0 620h
%define D1 0A20h
%define D2 0A22h
%define D3 0E20h
start:
        ; --- CPU clock: TSC over 18 BIOS ticks (~989 ms)
        push 40h
        pop es
        mov ax, [es:6Ch]
.w0:    cmp ax, [es:6Ch]
        je .w0
        rdtsc
        mov [t0], eax
        mov [t0+4], edx
        mov bx, [es:6Ch]
        add bx, 18
.w1:    cmp bx, [es:6Ch]
        jne .w1
        rdtsc
        sub eax, [t0]
        sbb edx, [t0+4]
        mov ecx, 988600
        div ecx
        mov [mhz], eax
        mov si, s_mhz
        call puts
        call putn
        call nl

        ; --- detection x100
        call tstart
        mov cx, 100
        xor bp, bp
.det:   mov ax, 0E0h            ; ID
        mov bx, D3
        call rdreg
        and al, 0Fh
        cmp al, 0Ch
        jne .d1
        mov ax, 3Dh             ; HWCF1
        mov bx, D1
        call rdreg
        and al, 7Eh
        cmp al, 58h
        jne .d1
        mov ax, 3Eh             ; HWCF2
        mov bx, D1
        call rdreg
        and al, 3
        cmp al, 3
        jne .d1
        inc bp
.d1:    loop .det
        call tstop
        mov si, s_det
        call puts
        call putn
        mov si, s_ok
        call puts
        movzx eax, bp
        call putn
        call nl

        ; --- 10000 reads of ID
        mov dx, PTR
        mov ax, 0E0h
        out dx, ax
        call tstart
        mov cx, 10000
        mov dx, D3
.rd:    in ax, dx
        loop .rd
        call tstop
        mov si, s_rd
        call puts
        call putn               ; total us for 10000 = 1/100 us per read
        call nl

        ; --- AWEUTIL-style init
        mov dword [nreads], 0
        call tstart
        mov ax, 3Dh
        mov bx, D1
        mov cx, 59h
        call wrreg              ; HWCF1
        mov ax, 3Eh
        mov cx, 20h
        call wrreg              ; HWCF2
        mov ax, 3Fh
        xor cx, cx
        call wrreg              ; HWCF3 = 0
        xor di, di
.v:     mov ax, di
        or ax, 5 << 5
        mov bx, D1
        mov cx, 80h
        call wrreg              ; DCYSUSV = 80h: envelope off
        mov si, vregs
.vr:    lodsw
        or ax, ax
        jz .vd
        mov bx, ax
        lodsw
        or ax, di
        xor cx, cx
        call wrreg
        jmp .vr
.vd:    inc di
        cmp di, 32
        jb .v
        mov si, init1
        call initpass
        call wcwait
        mov si, init1
        call initpass
        call wcwait
        mov ax, 3Fh
        mov bx, D1
        mov cx, 4
        call wrreg              ; HWCF3 = 4
        call tstop
        mov si, s_init
        call puts
        call putn
        mov si, s_reads
        call puts
        mov eax, [nreads]
        call putn
        call nl

        ; --- 512 KB upload: SMALW = 200000h, 262144 x SMLD
        call tstart
        mov ax, 36h             ; SMALW (Data1 r1 ch22): lo A20, hi A22
        mov bx, D1
        xor cx, cx
        call wrreg
        mov dx, D2
        mov ax, 20h
        out dx, ax
        mov dx, PTR
        mov ax, 3Ah             ; SMLD
        out dx, ax
        mov dx, D1
        xor ebx, ebx
.up:    lea eax, [ebx*2+ebx+1]
        out dx, ax
        inc ebx
        cmp ebx, 40000h
        jb .up
        call tstop
        mov si, s_up
        call puts
        call putn
        call nl

        ; --- verify 64 words
        mov eax, 200000h
        call setsmalr
        xor ebx, ebx
        xor bp, bp
.ver:   in ax, dx
        lea ecx, [ebx*2+ebx+1]
        cmp ax, cx
        jne .vn
        inc bp
.vn:    inc ebx
        cmp ebx, 64
        jb .ver
        mov si, s_ver
        call puts
        movzx eax, bp
        call putn
        call nl

        ; --- DRAM sizing (Linux emu8000 size_dram style): 32K-word steps until the read-back fails or wraps
        call tstart
        mov eax, 200000h
        mov cx, 1234h
        call ramw
        xor esi, esi            ; detected words
.sz:    add esi, 8000h
        cmp esi, 0E00000h
        jae .szd
        lea eax, [esi+200000h]
        mov cx, 0AAAAh
        call ramw
        lea eax, [esi+200000h]
        call ramr
        cmp ax, 0AAAAh
        jne .szd
        mov eax, 200000h
        call ramr
        cmp ax, 1234h
        je .sz
.szd:   call tstop
        push esi
        mov si, s_size
        call puts
        call putn
        mov si, s_kb
        call puts
        pop eax
        shr eax, 9
        call putn
        call nl

        ; --- WC over ~110 ms (2 ticks): ~4850 at 44.1 kHz
        mov ax, 3Bh
        mov dx, PTR
        out dx, ax
        mov ax, [es:6Ch]
.t0:    cmp ax, [es:6Ch]
        je .t0
        mov dx, D2
        in ax, dx
        mov bp, ax
        mov ax, [es:6Ch]
        add ax, 2
.t1:    cmp ax, [es:6Ch]
        jne .t1
        in ax, dx
        sub ax, bp
        movzx eax, ax
        mov si, s_wc
        call puts
        call putn
        call nl
        mov ax, 4C00h
        int 21h

; write a RAM word: EAX = address, CX = value
ramw:   push cx
        push eax
        mov ax, 36h
        mov bx, D1
        pop ecx
        push ecx
        call wrreg              ; SMALW lo
        mov dx, D2
        pop eax
        shr eax, 16
        out dx, ax              ; SMALW hi
        mov dx, PTR
        mov ax, 3Ah
        out dx, ax
        mov dx, D1
        pop ax
        out dx, ax
        ret
; read a RAM word: EAX = address -> AX
ramr:   call setsmalr
        in ax, dx
        ret
; SMALR = EAX, one dummy SMLD read; leaves DX = D1 with the pointer on SMLD
setsmalr:
        push eax
        mov dx, PTR
        mov ax, 34h
        out dx, ax
        mov dx, D1
        pop eax
        out dx, ax
        shr eax, 16
        mov dx, D2
        out dx, ax
        mov dx, PTR
        mov ax, 3Ah
        out dx, ax
        mov dx, D1
        in ax, dx
        inc dword [nreads]
        ret

; INIT1-4 for 32 channels (SI ignored: zeros, the timing is what counts)
initpass:
        push di
        mov si, initregs
.ip:    lodsw
        or ax, ax
        jz .ipd
        mov bx, ax
        lodsw
        xor di, di
.ic:    push ax
        or ax, di
        mov cx, di
        shl cx, 8
        call wrreg
        pop ax
        inc di
        cmp di, 32
        jb .ic
        jmp .ip
.ipd:   pop di
        ret

; wait 1024 samples on WC
wcwait: mov dx, PTR
        mov ax, 3Bh
        out dx, ax
        mov dx, D2
        in ax, dx
        inc dword [nreads]
        mov bx, ax
.ww:    in ax, dx
        inc dword [nreads]
        sub ax, bx
        cmp ax, 1024
        jb .ww
        ret

; AX = pointer value, BX = data port, CX = value
wrreg:  push dx
        mov dx, PTR
        out dx, ax
        mov dx, bx
        mov ax, cx
        out dx, ax
        pop dx
        ret
; AX = pointer value, BX = data port -> AX
rdreg:  push dx
        mov dx, PTR
        out dx, ax
        mov dx, bx
        in ax, dx
        pop dx
        inc dword [nreads]
        ret

tstart: rdtsc
        mov [t0], eax
        mov [t0+4], edx
        ret
tstop:  rdtsc                   ; -> EAX = microseconds since tstart
        sub eax, [t0]
        sbb edx, [t0+4]
        div dword [mhz]
        ret

puts:   push eax                ; keeps EAX (the number printed next)
.l:     lodsb
        or al, al
        jz .e
        mov dl, al
        mov ah, 2
        int 21h
        jmp .l
.e:     pop eax
        ret
nl:     push si
        mov si, s_nl
        call puts
        pop si
        ret
putn:   pushad                  ; EAX decimal
        mov ebx, 10
        xor cx, cx
.p1:    xor edx, edx
        div ebx
        push dx
        inc cx
        or eax, eax
        jnz .p1
.p2:    pop dx
        add dl, '0'
        mov ah, 2
        int 21h
        loop .p2
        popad
        ret

; per voice: port, register << 5 (channel or'ed in); 0 = end. Data0 dwords as two words
vregs:  dw D1, 4 << 5, D1, 6 << 5, D1, 7 << 5, D2, 4 << 5, D2, 5 << 5, D2, 6 << 5, D2, 7 << 5
        dw D3, 0 << 5, D3, 1 << 5, D3, 2 << 5, D3, 3 << 5, D3, 4 << 5, D3, 5 << 5
        dw D0, 1 << 5, 622h, 1 << 5, D0, 0 << 5, 622h, 0 << 5, D0, 3 << 5, 622h, 3 << 5, D0, 2 << 5, 622h, 2 << 5
        dw D0, 6 << 5, 622h, 6 << 5, D0, 7 << 5, 622h, 7 << 5, D1, 0, D2, 0, 0
initregs: dw D1, 2 << 5, D2, 2 << 5, D1, 3 << 5, D2, 3 << 5, 0
init1:
s_mhz:  db "MHZ ", 0
s_det:  db "DET us ", 0
s_ok:   db " ok ", 0
s_rd:   db "RD us100 ", 0
s_init: db "INIT us ", 0
s_reads: db " reads ", 0
s_up:   db "UP us ", 0
s_ver:  db "VER ok ", 0
s_size: db "SIZE us ", 0
s_kb:   db " kb ", 0
s_wc:   db "WC d ", 0
s_nl:   db 13, 10, 0
t0:     dd 0, 0
mhz:    dd 0
nreads: dd 0
