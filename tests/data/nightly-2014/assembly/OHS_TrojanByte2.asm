    PUSH CS
    PUSH SS
    MOV WORD [0x1243], 0x2934
    POP ES
    MOV WORD [0x2612], 0xEF39
    POP SS
    MOV CL, 0x0B
    MOV SI, AX
    ADD SI, WORD 0x0077
    REP MOVSW
    XOR SI, SI
    SUB AH, 0x07
    MOV DI, 0x0040
    MOV BX, DI
    MOV CL, 0x09
    NOP
    MOV AL, 0xA3
    STOSW
    MOV BP, AX
    MOV AX, 0x1004
    STOSW
    MOV SI, ES
    MOV ES, AX
    MOV DI, BP
    MOV BP, 0x3200
    MOV SP, DI
    ADD SP, 0x0614
    MOV WORD [DI+0x38], 0x1FFF
    MOV WORD [DI+0x3C], 0x1FFF
    NRG
    NOP
    MOV WORD [DI+0x40], 0x1FFF
    MOV WORD [DI+0x44], 0x1FFF
    MOV WORD [DI+0x48], 0x1FFF
    MOV DS, SI
    XOR SI, SI
    MOV DX, 0x060E
    INC DI
    NRG
    NRG
    NRG
    NOP
    NOP
    NOP
    NOP
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