    NOP
    PUSH AX
    PUSH ES
    PUSH DS
    POP ES
    MOV DI, 0xFAFE
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
    PUSH BX
    MOV BX, ES
    MOV DS, BX
    MOV BX, AX
    NOP
    NOP
    NOP
    NOP
    MOV [0x0200], BX
    MOV BX, 0x1000
    MOV DS, BX
    POP BX
    PUSH CS
    PUSH SS
    POP ES
    POP SS
    MOV SI, AX
    ADD SI, 0x008D
    MOV CL, 0x0F
    REP MOVSW
    SUB AH, 0x07
    ADD DI, 0x0040
    MOV BX, DI
    MOV AL, 0xA1
    STOSW
    MOV BP, AX
    NOP
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
    MOV WORD [DI+0x1C], 0x1FFF
    MOV WORD [DI+0x22], 0x1FFF
    NOP
    NOP
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
    SUB [BX], BP
    MOV DI, [BX]
    MOVSW
    MOVSW
    XOR SI, SI
    NOP
    NOP
    MOV SP, DI
    ADD SP, 0x0001
    SUB DI, 0x0003
    ADD SP, DX
    MOV CL, 0x0C
    CALL FAR [BX]
    CALL FAR [BX]
    CALL FAR [BX]