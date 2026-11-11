    JMP label1
    MOV CX, 0x0012
    XOR DI, DI
    MOV SI, AX
    ADD SI, 0x0111
    REP MOVSW
    PUSH DS
    PUSH ES
    POP DS
    POP ES
    PUSH CS
    POP SS
    MOV CX, [0x0050]
    INC CX
    MOV AX, 0x1FFF
    MOV [0x0100], AX
    MOV BX, 0x0300
    NOP
    NOP
    NOP
    MOV SP, [0x0200]
    NOP
    NOP
    ADD SP, 0x4000
    MOV [BX], SP
    MOV [BX+0x02], CS
    SUB [BX+0x02], CX
    ADD SP, 0x0500
    SHL CX, 1
    SHL CX, 1
    SHL CX, 1
    SHL CX, 1
    MOV [BX+0x20], CX
    MOV CX, 0x0009
    NOP
    NOP
    NOP
    MOV SI, 0x0014
    MOV DI, [BX]
    SUB DI, [BX+0x20]
    NOP
    SUB DI, 0x0206
    MOV AX, 0xCCCC
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
    NOP
    XOR SI, SI
    SUB DI, 0x0007
    CALL FAR [BX]
    NOP
    NOP
    NOP
    NOP
    REP MOVSW
    SUB SP, DX
    SUB [BX], BP
    MOV DI, [BX]
    SUB DI, [BX+0x20]
    MOV CL, 0x09
    MOVSW
    XOR SI, SI
    DEC DI
    CALL FAR [BX]
    NOP
    CALL FAR [BX]
    CALL FAR [BX]
    CALL FAR [BX]
    NOP
label1:
    NOP
    MOV CX, 0x0012
    XOR DI, DI
    MOV SI, AX
    ADD SI, 0x0111
    REP MOVSW
    PUSH DS
    PUSH ES
    POP DS
    POP ES
    PUSH CS
    POP SS
    MOV CX, [0x0050]
    INC CX
    MOV AX, 0x1FFF
    MOV [0x0100], AX
    MOV BX, 0x0300
    MOV SP, [0x0200]
    NOP
    NOP
    ADD SP, 0x4000
    MOV [BX], SP
    MOV [BX+0x02], CS
    SUB [BX+0x02], CX
    ADD SP, 0x0500
    SHL CX, 1
    SHL CX, 1
    SHL CX, 1
    SHL CX, 1
    MOV [BX+0x20], CX
    MOV CX, 0x0009
    MOV SI, 0x0014
    MOV DI, [BX]
    SUB DI, [BX+0x20]
    NOP
    SUB DI, 0x0206
    MOV AX, 0xCCCC
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
    REP MOVSW
    SUB SP, DX
    SUB [BX], BP
    MOV DI, [BX]
    SUB DI, [BX+0x20]
    MOV CL, 0x09
    MOVSW
    XOR SI, SI
    DEC DI
    CALL FAR [BX]
    NOP
    CALL FAR [BX]
    CALL FAR [BX]
    CALL FAR [BX]
    CALL FAR [BX]
    CALL FAR [BX]
    CALL FAR [BX]
    CALL FAR [BX]
    NOP
    NOP
    NOP
    NOP
    NOP
    MOV CX, 0x0012
    XOR DI, DI
    MOV SI, AX
    ADD SI, 0x0111
    REP MOVSW
    PUSH DS
    PUSH ES
    POP DS
    POP ES
    PUSH CS
    POP SS
    MOV CX, [0x0050]
    INC CX
    MOV AX, 0x1FFF
    MOV [0x0100], AX
    MOV BX, 0x0300
    MOV SP, [0x0200]
    NOP
    NOP
    ADD SP, 0x4000
    MOV [BX], SP
    MOV [BX+0x02], CS
    SUB [BX+0x02], CX
    ADD SP, 0x0500
    SHL CX, 1
    SHL CX, 1
    SHL CX, 1
    SHL CX, 1
    MOV [BX+0x20], CX
    MOV CX, 0x0009
    MOV SI, 0x0014
    MOV DI, [BX]
    SUB DI, [BX+0x20]
    NOP
    SUB DI, 0x0206
    MOV AX, 0xCCCC
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
    REP MOVSW
    SUB SP, DX
    SUB [BX], BP
    MOV DI, [BX]
    SUB DI, [BX+0x20]
    MOV CL, 0x09
    MOVSW
    XOR SI, SI
    DEC DI
    CALL FAR [BX]
    NOP
    CALL FAR [BX]
    CALL FAR [BX]
    CALL FAR [BX]
    CALL FAR [BX]
    CALL FAR [BX]
    CALL FAR [BX]
    CALL FAR [BX]
    NOP
    NOP
    NOP
    NOP
    NOP