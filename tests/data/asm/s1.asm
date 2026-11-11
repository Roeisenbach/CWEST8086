jmp start
zombie:

mov ax, bx
sub ax, 0x2
add ax, 0x2000
push ax
sub ax, 0x2000
xor bx, bx
jmp preper

start:

takeover:
push ax
push ax
add ax, zombie
mov byte[bx], 0xbb
mov [bx+1], ax
mov word[bx+3], 0xe3ff
pop ax
mov [0x100], bx

preper:
stosw
push ss
pop es
mov si, ax
add si, thecopy
xor di,di
mov cl, 0xd
rep movsw
push ss
pop ds
mov bp, cs
sub bp, 0x3
mov es, bp
pop ax
push cs
pop ss
mov bh, 0x1
mov[bx+0x2], bp
add ah, 0x3
mov al, 0xa3
mov[bx], ax
mov di, ax
mov ax, 0x18ff
stosw
dec di
mov bp, 0x1000
xor si,si
LEA SP, [DI + 0x7CF]
call far[bx+si]

thecopy:
movsw
movsw
mov cl, 0x7
rep movsw
sub[bx], bp
mov di, [bx]
movsw
movsw
movsw
mov di, [bx]
inc di
xor si,si
call far[bx+si]
call far[bx+si]
db 0xEB, 0xFC
db 0xEB, 0xFA
