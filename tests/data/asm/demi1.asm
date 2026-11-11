mov bx, 0x100
loop:
cmp [bx], cx
je out
jmp loop
out:
jmp cx
