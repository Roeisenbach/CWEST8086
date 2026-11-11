    JMP label1
    PUSH CS
    POP ES
    MOV DI, AX
    ADD DI, 0x0200
    INT 0x86
    INT 0x86
    XOR DI, DI
    ADD AX, 0x0072
    MOV SI, AX
    PUSH SS
    POP ES
    REP MOVSW
    PUSH SS
    PUSH CS
    POP ES
    POP DS
    MOV DX, SS
    PUSH CS
    POP SS
    ADD AH, 0x08
    MOV [BX], AX
    MOV AH, 0x04
    PUSH SS
    POP ES
    REP MOVSW
    PUSH SS
    PUSH CS
    POP ES
    POP DS
    MOV DX, SS
    PUSH CS
    POP SS
    ADD AH, 0x08
    MOV [BX], AX
    MOV AH, 0x04
label1:
    PUSH CS
    POP ES
    MOV DI, AX
    ADD DI, 0x0200
    INT 0x86
    INT 0x86
    XOR DI, DI
    ADD AX, 0x0072
    MOV SI, AX
    PUSH SS
    POP ES
    MOV CL, 0x27
    REP MOVSW
    PUSH SS
    PUSH CS
    POP ES
    POP DS
    MOV DX, SS
    PUSH CS
    POP SS
    ADD AH, 0x08
    MOV AL, 0xA3
    MOV BX, 0x0123
    MOV [BX], AX
    MOV WORD [BX+0x02], 0x1000
    MOV AH, 0x04
    MOV SI, 0x000E
    JMP label2
    REP MOVSW
label2:
    LES DI, [BX]
    LEA SP, [DI+0x0200]
    MOV CL, 0x07
    MOVSW
    DEC DI
    CALL FAR [BX]
    CALL FAR [BX]
    REP MOVSW
    SUB DI, 0x080E
    MOV BP, DI
    MOVSW
    MOVSW
    MOV CX, 0x0013
    CALL BP
    MOVSW
    POP BP
    REP MOVSW
    LDS CX, [SI]
    XCHG SI, BP
    INC SI
    LEA DI, [SI+0x0100]
    REP CMPSW
    MOV CL, AH
    MOV SI, [DI]
    SHL SI, CL
    ADD SI, [DI-0x02]
    MOV [SI-0x02], AX
    MOV DS, DX
    MOV CL, 0x06
    SUB WORD [BX], 0x3200
    XOR SI, SI
    LES DI, [BX]
    MOVSW
    JMP WORD [BX]
    INT3
    INC WORD [BX+SI]
    ADD [BX+SI], DL
    PUSH CS
    POP ES
    MOV DI, AX
    ADD DI, 0x0200
    INT 0x86
    INT 0x86
    XOR DI, DI
    ADD AX, 0x0072
    MOV SI, AX
    PUSH SS
    POP ES
    REP MOVSW
    PUSH SS
    PUSH CS
    POP ES
    POP DS
    MOV DX, SS
    PUSH CS
    POP SS
    ADD AH, 0x08
    MOV [BX], AX
    MOV AH, 0x04
    PUSH SS
    POP ES
    REP MOVSW
    PUSH SS
    PUSH CS
    POP ES
    POP DS
    MOV DX, SS
    PUSH CS
    POP SS
    ADD AH, 0x08
    MOV [BX], AX
    MOV AH, 0x04