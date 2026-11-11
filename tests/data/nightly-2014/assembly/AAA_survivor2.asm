    MOV CL, 0x0C
    PUSH SS
    POP ES
    MOV SI, AX
    ADD SI, WORD 0x0042
    REP MOVSW
    MOV CL, 0x30
    LOOP $
    PUSH AX
    PUSH CS
    POP ES
    MOV AX, 0x1FFF
    MOV DX, 0xCCCC
    INT 0x87
    POP AX
    MOV CX, 0x1003
    PUSH CX
    POP ES
    PUSH SS
    POP DS
    PUSH CS
    POP SS
    MOV AL, 0xA3
    MOV SP, AX
    ADD SP, 0x0060
    MOV BX, 0x00FC
    MOV [BX], AX
    MOV [BX+0x02], CX
    MOV DX, 0x3900
    MOV BP, 0x3100
    MOV AX, 0xF5AE
    XOR SI, SI
    MOV CX, 0x000A
    REP MOVSW
    SUB [BX], DX
    SUB SP, BP
    MOV DI, [BX]
    DEC DI
    MOV CL, 0x09
    MOVSW
    MOVSW
    XOR SI, SI
    DEC DI
    DEC DI
    CALL FAR [BX]
    NOP
    DB 0x67
    CALL FAR [BX]
    DB 0x67