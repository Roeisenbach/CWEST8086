    MOV SI, AX
    ADD SI, 0x0021
    MOV CL, 0x06
    REP MOVSW
    PUSH ES
    POP DS
    MOV BX, DI
    STOSW
    MOV [DI], CS
    MOV SS, [DI]
    MOV AX, 0x1FFF
    LES DI, [BX]
    XOR SI, SI
    MOV CL, 0x05
    MOV SP, [BX]
    STOSW
    DEC DI
    CALL FAR [BX]
    REP MOVSW
    MOV DI, [BX]
    MOV CL, 0x06
    XOR SI, SI
    STOSW
    DEC DI
    CALL FAR [BX]