    PUSH CS
    POP SS
    MOV SP, 0xFF09
    MOV BX, 0x0009
    DIV BX
    ADD SP, DX
    MOV AX, 0xCD87
label1:
    SUB SP, 0x0007
    PUSH AX
    JMP label1