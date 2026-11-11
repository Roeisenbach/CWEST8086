    JMP label1
    MOV CX, AX
    AND CX, 0x0007
    INC CX
    PUSH DS
    PUSH ES
    POP DS
    POP ES
    PUSH CS
    MOV [0x0050], CX
    MOV BX, 0x0200
    MOV SP, SI
    AND SP, 0xFE00
    ADD SP, 0x5FA3
    MOV [BX], SP
    MOV [BX+0x02], CS
    SUB [BX+0x02], CX
    NOP
    ADD SP, 0x0500
    SHL CX, 1
    SHL CX, 1
    SHL CX, 1
    SHL CX, 1
    MOV [BX+0x20], CX
    MOV WORD [0x0022], 0x1FFF
    MOV CX, 0x0010
    MOV SI, 0x0014
    MOV DI, [BX]
    SUB DI, [BX+0x20]
    SUB DI, 0x0206
    MOV AX, 0x90CC
    MOV DX, AX
    INT 0x86
    INT 0x86
    MOV AX, [0x0100]
    MOV DX, 0x7800
    MOV BP, 0x7D00
    MOVSW
    MOVSW
    MOVSW
    MOVSW
    MOVSW
    MOVSW
    MOVSW
    XOR SI, SI
    SUB DI, 0x0007
    CALL FAR [BX]
    NOP
    NOP
    NOP
    NOP
    NOP
    NOP
    NOP
    NOP
    NOP
    NOP
label1:
    MOV CX, AX
    AND CX, 0x0007
    INC CX
    PUSH DS
    PUSH ES
    POP DS
    POP ES
    PUSH CS
    POP SS
    MOV [0x0050], CX
    MOV BX, 0x0200
    MOV SP, SI
    AND SP, 0xFE00
    ADD SP, 0x5FA3
    MOV [BX], SP
    MOV [BX+0x02], CS
    SUB [BX+0x02], CX
    NOP
    NOP
    ADD SP, 0x0500
    SHL CX, 1
    SHL CX, 1
    SHL CX, 1
    SHL CX, 1
    MOV [BX+0x20], CX
    MOV WORD [0x0022], 0x1FFF
    MOV CX, 0x0010
    MOV SI, 0x0014
    MOV DI, [BX]
    SUB DI, [BX+0x20]
    SUB DI, 0x0206
    MOV AX, 0x90CC
    MOV DX, AX
    INT 0x86
    INT 0x86
    MOV AX, [0x0100]
    MOV DX, 0x7800
    MOV BP, 0x7D00
    MOVSW
    MOVSW
    MOVSW
    MOVSW
    MOVSW
    MOVSW
    MOVSW
    XOR SI, SI
    SUB DI, 0x0007
    CALL FAR [BX]
    CALL FAR [BX]
    CALL FAR [BX]
    CALL FAR [BX]
    NOP
    NOP
    NOP
    NOP
    MOV CX, AX
    AND CX, 0x0007
    INC CX
    PUSH DS
    PUSH ES
    POP DS
    POP ES
    PUSH CS
    POP SS
    MOV [0x0050], CX
    MOV BX, 0x0200
    MOV SP, SI
    AND SP, 0xFE00
    ADD SP, 0x5FA3
    MOV [BX], SP
    MOV [BX+0x02], CS
    SUB [BX+0x02], CX
    NOP
    NOP
    ADD SP, 0x0500
    SHL CX, 1
    SHL CX, 1
    SHL CX, 1
    SHL CX, 1
    MOV [BX+0x20], CX
    MOV WORD [0x0022], 0x1FFF
    MOV CX, 0x0010
    MOV SI, 0x0014
    MOV DI, [BX]
    SUB DI, [BX+0x20]
    SUB DI, 0x0206
    MOV AX, 0x90CC
    MOV DX, AX
    INT 0x86
    INT 0x86
    MOV AX, [0x0100]
    MOV DX, 0x7800
    MOV BP, 0x7D00
    MOVSW
    MOVSW
    MOVSW
    MOVSW
    MOVSW
    MOVSW
    MOVSW
    XOR SI, SI
    SUB DI, 0x0007
    CALL FAR [BX]
    CALL FAR [BX]
    CALL FAR [BX]
    CALL FAR [BX]
    NOP
    NOP
    NOP
    NOP