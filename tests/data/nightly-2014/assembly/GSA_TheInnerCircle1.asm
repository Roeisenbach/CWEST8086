    JMP label3
    MOV SI, AX
    ADD SI, 0x00E6
    XOR DI, DI
    PUSH ES
    PUSH SS
    POP ES
    MOV CL, 0x0A
    REP MOVSW
    PUSH CS
    POP ES
    MOV AX, 0x1FFF
    MOV DI, 0xCCA3
    STOSW
    DEC DI
    PUSH DS
    PUSH SS
    POP DS
    XOR SI, SI
    MOV BX, 0x0069
    MOV WORD [BX], 0xCCA3
    MOV [BX+0x02], CS
    MOV WORD [BX+0x14], 0x2E00
    PUSH CS
    PUSH CS
    POP ES
    POP SS
    MOV SP, 0xCFA3
    MOV CL, 0x08
    MOV BP, CX
    MOV DX, 0x3100
    CALL FAR [BX]
    REP MOVSW
    SUB [BX], DX
    LES DI, [BX]
    MOV CL, 0x08
    MOVSW
    SUB SP, [BX+SI]
    DEC DI
    XOR SI, SI
    DEC AX
label1:
    JNZ label1+1
    POP DS
    CALL FAR [BX]
    NOP
    NOP
    MOV SI, AX
    ADD SI, 0x0092
    XOR DI, DI
    PUSH ES
    PUSH SS
    POP ES
    MOV CL, 0x0A
    REP MOVSW
    PUSH CS
    POP ES
    MOV AX, 0x1FFF
    MOV DI, 0xCCA3
    STOSW
    DEC DI
    PUSH DS
    PUSH SS
    POP DS
    XOR SI, SI
    MOV BX, 0x0069
    MOV WORD [BX], 0xCCA3
    MOV [BX+0x02], CS
    MOV WORD [BX+0x14], 0x2E00
    PUSH CS
    PUSH CS
    POP ES
    POP SS
    MOV SP, 0xCFA3
    MOV CL, 0x08
    MOV BP, CX
    MOV DX, 0x3100
    CALL FAR [BX]
    REP MOVSW
    SUB [BX], DX
    LES DI, [BX]
    MOV CL, 0x08
    MOVSW
    SUB SP, [BX+SI]
    DEC DI
    XOR SI, SI
    DEC AX
label2:
    JNZ label2+1
    POP DS
    CALL FAR [BX]
    NOP
    NOP
label3:
    MOV SI, AX
    ADD SI, 0x00E6
    XOR DI, DI
    PUSH ES
    PUSH SS
    POP ES
    MOV CL, 0x0A
    REP MOVSW
    PUSH CS
    POP ES
    MOV AX, 0x1FFF
    MOV DI, 0xCCA3
    STOSW
    DEC DI
    PUSH DS
    PUSH SS
    POP DS
    XOR SI, SI
    MOV BX, 0x0069
    MOV WORD [BX], 0xCCA3
    MOV [BX+0x02], CS
    MOV WORD [BX+0x14], 0x2E00
    PUSH CS
    PUSH CS
    POP ES
    POP SS
    MOV SP, 0xCFA3
    MOV CL, 0x08
    MOV BP, CX
    MOV DX, 0x3100
    CALL FAR [BX]
    REP MOVSW
    SUB [BX], DX
    LES DI, [BX]
    MOV CL, 0x08
    MOVSW
    SUB SP, [BX+SI]
    DEC DI
    XOR SI, SI
    DEC AX
label4:
    JNZ label4+1
    POP DS
    CALL FAR [BX]
    NOP
    NOP