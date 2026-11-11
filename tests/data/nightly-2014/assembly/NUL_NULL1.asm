    PUSH CS
    POP ES
    MOV DI, AX
    ADD DI, 0x1000
    INT 0x86
    MOV DI, 0xE430
    INT 0x86
    MOV DI, 0x0000
    MOV SI, AX
    ADD SI, WORD 0x002F
    MOV CL, 0x12
    REP MOVSW
    MOV DX, 0x0019
    PUSH CS
    POP SS
    MOV SP, AX
    PUSH ES
    POP DS
    PUSH CS
    POP ES
    MOV BX, DI
    MOV [BX], AX
    ADD WORD [BX], WORD 0x0041
    MOV [BX+0x02], CS
    MUL DX
    ADD AL, 0x69
    ADD DL, 0x69
    MOV DI, AX
    XOR SI, SI
    MOV CL, 0x10
loop1:
    CALL FAR [BX]
    LOOP loop1
    MOV CL, 0x12
    REP MOVSW
    MOV [BX], AX
    ADD WORD [BX], 0x0012
    JMP AX
    NOP