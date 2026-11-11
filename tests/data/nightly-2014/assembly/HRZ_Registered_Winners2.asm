    MOV SI, AX
    PUSH CS
    POP ES
    MOV AX, 0xF9EB
    MOV DX, 0xCCCC
    MOV CX, 0x9769
    MOV BX, 0x26FF
    STD
    INT 0x87
    CLD
    MOV AX, SI
    MOV AL, AH
    XOR AH, AH
    MOV CH, 0x34
    DIV CH
    MUL CH
    MOV AH, AL
    MOV AL, 0xA2
    NOP
    NOP
    NOP
    MOV BX, 0x0200
    MOV CX, 0x0009
    ADD SI, WORD 0x0062
    PUSH SS
    POP ES
    REP MOVSW
    PUSH SS
    POP DS
    PUSH CS
    POP SS
    MOV [BX], AX
    MOV WORD [BX+0x02], 0x0FFC
    XOR SI, SI
    MOV DI, AX
    MOV AX, 0x0FFC
    MOV ES, AX
    MOV SP, DI
    NOP
    NOP
    NOP
    MOV CX, 0x0009
    ADD SP, 0x0300
    MOV DX, 0x3000
    MOV BP, 0x3400
    MOV AX, 0x1FFF
    STOSW
    DEC DI
    CALL FAR [BX]
    MOVSW
    REP MOVSW
    SUB SP, DX
    SUB [BX], BP
    MOV DI, [BX]
    MOV CL, 0x09
    XOR SI, SI
    STOSW
    DEC DI
    CALL FAR [BX]