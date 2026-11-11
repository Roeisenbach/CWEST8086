    MOV SI, AX
    ADD AX, 0x0086
    MOV [0x9769], AX
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
    NOP
    MOV AH, AL
    ADD AH, 0x18
    MOV AL, 0xA2
    MOV CX, 0x0009
    ADD SI, WORD 0x0075
    PUSH SS
    POP ES
    REP MOVSW
    PUSH SS
    POP DS
    PUSH CS
    POP SS
    MOV BX, 0x0200
    MOV [BX], AX
    MOV WORD [BX+0x02], 0x0FFC
    XOR SI, SI
    MOV DI, AX
    NOP
    NOP
    NOP
    MOV AX, 0x0FFC
    MOV ES, AX
    MOV SP, DI
    ADD SP, 0x0300
    MOV CX, 0x0009
    MOV AX, 0x1FFF
    STOSW
    DEC DI
    MOV DX, 0x3000
    MOV BP, 0x3400
    CALL FAR [BX]
    NOP
    NOP
    NOP
    NOP
    NOP
    NOP
    NOP
    NOP
    NOP
    NOP
    NOP
    NOP
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
    CALL prcd1
prcd1:
    POP AX
    MOV SI, AX
    MOV AL, AH
    XOR AH, AH
    MOV CH, 0x34
    DIV CH
    MUL CH
    MOV AH, AL
    ADD AH, 0x28
    NOP
    MOV AL, 0xA2
    SUB SI, 0x0014
    XOR DI, DI
    PUSH SS
    POP ES
    MOV CX, 0x0009
    REP MOVSW
    PUSH SS
    POP DS
    PUSH CS
    POP SS
    MOV BX, 0x0200
    MOV [BX], AX
    MOV WORD [BX+0x02], 0x0FFC
    XOR SI, SI
    MOV DI, AX
    MOV AX, 0x0FFC
    MOV ES, AX
    NOP
    NOP
    MOV SP, DI
    ADD SP, 0x0300
    MOV CX, 0x0009
    MOV AX, 0x1FFF
    STOSW
    DEC DI
    MOV DX, 0x3000
    MOV BP, 0x3400
    CALL FAR [BX]