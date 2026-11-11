    NOP
    PUSH AX
    PUSH ES
    PUSH DS
    POP ES
    MOV DI, 0x0100
    MOV DX, 0xCCCC
    MOV AX, 0xCCCC
    INT 0x86
    STD
    ADD DI, 0x00FF
    INT 0x86
    CLD
    POP ES
    POP AX
    XOR DI, DI
    MOV DX, AX
    NOP
    NOP
    NOP
    NOP
    NOP
    PUSH BX
    PUSH ES
    POP DS
    NOP
    MOV BX, [0x0200]
    MOV AX, BX
    SUB AX, 0x076C
    MOV BX, ES
    MOV DS, BX
    MOV BX, 0x1000
    MOV DS, BX
    POP BX
    PUSH CS
    PUSH SS
    POP ES
    POP SS
    MOV SI, DX
    ADD SI, 0x0097
    MOV CL, 0x0F
    REP MOVSW
    SUB AH, 0x07
    NOP
    NOP
    ADD DI, 0x0040
    MOV BX, DI
    MOV AL, 0xA1
    STOSW
    MOV BP, AX
    MOV AX, 0x1002
    STOSW
    MOV SI, ES
    MOV ES, AX
    MOV DI, BP
    MOV BP, 0x1E00
    MOV SP, DI
    ADD SP, 0x071A
    MOV CL, 0x0C
    MOV WORD [DI+0x18], 0x1FFF
    NOP
    NOP
    MOV WORD [DI+0x1C], 0x1FFF
    MOV WORD [DI+0x22], 0x1FFF
    MOV WORD [DI+0x26], 0x1FFF
    MOV WORD [DI+0x2A], 0x1FFF
    MOV DS, SI
    MOV DX, 0x070B
    XOR SI, SI
    INC DI
    ADD DI, 0x0002
    ADD WORD [BX], 0x0002
    CALL FAR [BX]
    REP MOVSW
    NOP
    NOP
    SUB [BX], BP
    MOV DI, [BX]
    MOVSW
    MOVSW
    XOR SI, SI
    MOV SP, DI
    ADD SP, 0x0001
    SUB DI, 0x0003
    ADD SP, DX
    MOV CL, 0x0C
    CALL FAR [BX]
    CALL FAR [BX]
    CALL FAR [BX]