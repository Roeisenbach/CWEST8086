    PUSH CS
    PUSH SS
    NOP
    POP ES
    POP SS
    MOV CL, 0x0B
    MOV SI, AX
    ADD SI, WORD 0x0058
    REP MOVSW
    XOR SI, SI
    SUB AH, 0x07
    MOV DI, 0x0040
    MOV BX, DI
    MOV CL, 0x09
    MOV AL, 0xA3
    STOSW
    MOV BP, AX
    MOV AX, 0x1006
    STOSW
    MOV SI, ES
    NOP
    MOV ES, AX
    MOV DI, BP
    MOV BP, 0x3200
    MOV SP, DI
    ADD SP, 0x061C
    MOV WORD [DI+0x58], 0x1FFF
    MOV WORD [DI+0x5C], 0x1FFF
    MOV WORD [DI+0x60], 0x1FFF
    MOV WORD [DI+0x64], 0x1FFF
    MOV WORD [DI+0x68], 0x1FFF
    NOP
    MOV DS, SI
    XOR SI, SI
    MOV DX, 0x0616
    INC DI
    CALL FAR [BX]
    REP MOVSW
    SUB [BX], BP
    MOV DI, [BX]
    MOVSW
    XOR SI, SI
    MOV SP, DI
    SUB DI, 0x0001
    ADD SP, DX
    MOV CL, 0x09
    CALL FAR [BX]
    CALL FAR [BX]