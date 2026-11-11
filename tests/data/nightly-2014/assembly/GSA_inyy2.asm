    MOV SI, AX
    ADD SI, 0x0027
    MOV CX, 0x0027
    MOV AX, CX
    MOV BX, 0x0028
    MOV DX, 0x0040
    REP MOVSW
    PUSH ES
    POP DS
    INC AX
    MOV AX, 0xA71C
    MOV BX, DI
    STOSW
    MOV [DI], CS
    MOV SS, [DI]
    XOR AX, AX
    LES DI, [BX]
    MOV AX, 0x1FFF
    XOR SI, SI
    MOV CL, 0x22
    MOV SP, [BX]
    STOSW
    DEC DI
    CALL FAR [BX]
    REP MOVSW
    SUB WORD [BX], 0x0019
    MOV DI, [BX]
    SUB SP, 0x0600
    MOV DI, [BX]
    NOT AX
    MOV CL, 0x17
    XOR SI, SI
    STOSW
    DEC DI
    CALL FAR [BX]