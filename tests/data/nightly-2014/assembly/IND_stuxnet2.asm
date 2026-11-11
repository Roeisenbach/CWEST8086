    PUSH ES
    MOV AL, 0xA2
    ADD AH, 0x01
    POP DS
    PUSH CS
    POP ES
    PUSH CS
    POP SS
    MOV BX, 0x0230
    MOV [BX], AX
    MOV WORD [BX+0x02], 0x1000
    XOR SI, SI
    MOV DI, AX
    MOV SP, DI
    ADD SP, 0x0300
    MOV CX, 0x0008
    MOV AX, 0x1FFF
    STOSW
    NOP
    DEC DI
    MOV DX, 0x3100
    MOV BP, 0x3400
    JMP WORD [BX]
    MOVSW
    REP MOVSW
    SUB SP, DX
    SUB [BX], BP
    MOV DI, [BX]
    MOV CL, 0x07
    XOR SI, SI
    STOSW
    DEC DI
    JMP WORD [BX]