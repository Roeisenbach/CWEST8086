jmp start
zombie:

mov ax, bx
sub ax, 0x2
add cx, 0x2000
push cx
sub cx, 0x2000
xor bx, bx
jmp preper

start:

takeover:
push bx
mov cl, 0x10
lp:
loop lp
push ax
add ax, zombie
mov byte[bx], 0xbb
mov [bx+1], ax
mov word[bx+3], 0xe3ff
pop ax
mov [0x200], bx

preper:
push es
push ss
pop es
mov si, ax
add si, thecode
xor di,di
mov cl, 0x10
rep movsw
pop ds
xor si, si
lodsw
pop bx
add ax, bx
push ss
pop ds
mov bp ,cs
sub bp, 0x3
mov es, bp
push cs
pop ss
mov bh, 0x1
mov[bx+0x2], bp
add ah, 0x33
mov al, 0xa1
mov[bx], ax
mov di, ax
mov ax, 0xd401
stosw
mov ax, 0x18ff
stosw
sub di, 0x3
xor si, si
mov bp, 0xa00
LEA SP, [DI + 0xB8BF]
mov dx, 0x2400
mov ax, 0x8c10
call far[bx]

thecode:
movsw
movsw
mov cl, 0xb
rep movsw
sub sp, ax
add ax, 0x3b00
add dx, 0x100
sub[bx], bp
mov di, [bx]
movsw
movsw
mov di, [bx]
inc di
xor si, si
call far[bx]
add sp, dx
call far[bx + si]
