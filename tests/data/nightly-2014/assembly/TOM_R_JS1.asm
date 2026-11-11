    NOP
    JMP label1
    PUSH AX
    PUSH ES
    PUSH CS
    POP ES
    MOV DI, AX
    STD
    MOV DX, 0xEE81
    MOV CX, 0x27FF
    MOV AX, 0x5ECC
    INT 0x87
    POP ES
    POP AX
    CLD
    PUSH ES
    MOV CX, 0x0009
    MOV SI, AX
    ADD SI, 0x00D8
    XOR DI, DI
    REP MOVSW
    STOSW
    INT3
    STD
    NOP
    NOP
    NOP
    MOV WORD [0x2FA1], 0xCCCC
    MOV AL, 0xA2
    ADD AH, 0x01
    POP DS
    PUSH CS
    POP ES
    PUSH CS
    POP SS
    MOV BX, 0x0200
    MOV [BX], AX
    MOV WORD [BX+0x02], 0x1000
    XOR SI, SI
    MOV DI, AX
    MOV SP, DI
    ADD SP, 0x0300
    MOV CX, 0x0008
    INT3
    NOP
    NOP
    NOP
    MOV AX, 0x1FFF
    STOSW
    DEC DI
    MOV DX, 0x3100
    MOV BP, 0x3400
    JMP WORD [BX]
    MOVSW
    REP MOVSW
    SUB SP, DX
    SUB [BX], BP
    MOV DI, [BX]
    MOV CL, 0x07
    XOR SI, SI
    STOSW
    DEC DI
    JMP WORD [BX]
label1:
    PUSH AX
    PUSH ES
    PUSH CS
    POP ES
    MOV DI, AX
    STD
    MOV DX, 0xEE81
    NOP
    NOP
    NOP
    MOV CX, 0x27FF
    MOV AX, 0x5ECC
    INT 0x87
    POP ES
    POP AX
    CLD
    PUSH ES
    MOV CX, 0x0009
    MOV SI, AX
    ADD SI, 0x00D8
    XOR DI, DI
    REP MOVSW
    STOSW
    MOV WORD [0x2FA1], 0xCCCC
    MOV AL, 0xA2
    ADD AH, 0x01
    POP DS
    PUSH CS
    POP ES
    NOP
    NOP
    NOP
    PUSH CS
    POP SS
    MOV BX, 0x0200
    MOV [BX], AX
    MOV WORD [BX+0x02], 0x1000
    XOR SI, SI
    MOV DI, AX
    MOV SP, DI
    ADD SP, 0x0300
    MOV CX, 0x0008
    MOV AX, 0x1FFF
    STOSW
    DEC DI
    MOV DX, 0x3100
    MOV BP, 0x3400
    JMP WORD [BX]
    INT3
    ESC 0xDE
    NOP
    NOP
    NOP
    MOVSW
    REP MOVSW
    SUB SP, DX
    SUB [BX], BP
    MOV DI, [BX]
    MOV CL, 0x07
    XOR SI, SI
    STOSW
    DEC DI
    JMP WORD [BX]
    PUSH AX
    PUSH ES
    PUSH CS
    POP ES
    MOV DI, AX
    STD
    MOV DX, 0xEE81
    MOV CX, 0x27FF
    MOV AX, 0x5ECC
    INT 0x87
    POP ES
    POP AX
    CLD
    PUSH ES
    NOP
    NOP
    NOP
    NOP
    MOV CX, 0x0009
    MOV SI, AX
    ADD SI, 0x00D8
    XOR DI, DI
    REP MOVSW
    STOSW
    MOV WORD [0x2FA1], 0xCCCC
    MOV AL, 0xA2
    ADD AH, 0x01
    POP DS
    PUSH CS
    POP ES
    PUSH CS
    POP SS
    MOV BX, 0x0200
    MOV [BX], AX
    MOV WORD [BX+0x02], 0x1000
    NOP
    NOP
    NOP
    XOR SI, SI
    MOV DI, AX
    MOV SP, DI
    ADD SP, 0x0300
    MOV CX, 0x0008
    MOV AX, 0x1FFF
    STOSW
    DEC DI
    MOV DX, 0x3100
    MOV BP, 0x3400
    JMP WORD [BX]
    MOVSW
    REP MOVSW
    SUB SP, DX
    SUB [BX], BP
    MOV DI, [BX]
    MOV CL, 0x07
    XOR SI, SI
    STOSW
    NOP
    NOP
    NOP
    DEC DI
    JMP WORD [BX]