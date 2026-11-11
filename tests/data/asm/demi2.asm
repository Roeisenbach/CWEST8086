mov bx, 0x200
loop:
cmp [bx], cx
je out
jmp loop
out:
jmp cx
