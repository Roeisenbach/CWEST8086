    JMP label1
    CALL FAR [BX]
    SUB SP, DX
    INT3
    INT3
    CALL FAR [BX]
    SUB SP, DX
    CALL FAR [BX]
label1:
    PUSH CS
    MOV BP, 0x1002
    MOV CS, BP
    SUB SP, DX
    INT3
    INT3
    CALL FAR [BX]
    SUB SP, DX
    MOV [BX], AX
    MOV [BX+0x02], CS
    MOV [BX], AX
    MOV [BX+0x02], CS
    NOP
    REP MOVSW
    MOVSW
    MOVSW
    DEC DI
    CALL FAR [BX]
    REP MOVSW
    MOVSW
    MOVSW
    CALL FAR [BX]
    MOV SI, AX
    NOP
    ADD SI, 0x00A5
    MOV CX, 0x000D
    REP MOVSW
    PUSH ES
    POP DS
    MOV BX, 0x0200
    MOV [BX], AX
    MOV ES, BP
    PUSH AX
    MOV AX, 0xCCCC
    NOP
    LEA DI, [SI+0x080F]
    MOV DX, AX
    INT 0x86
    ADD DI, 0x003C
    INT 0x86
    POP AX
    MOV AL, 0xAF
    ADD AH, 0x10
    POP SS
    MOV SP, AX
    SUB SP, 0x00FF
    SUB AX, 0x1E08
    MOV BX, 0x0030
    MOV [BX], AX
    MOV [BX+0x02], CS
    NOP
    NOP
    MOV BP, SS
    MOV ES, BP
    MOV AX, 0x1FFF
    MOV BX, 0xCCCC
    MOV CX, BX
    XOR DI, DI
    INT 0x87
    MOV BP, CS
    MOV ES, BP
    MOV BX, 0x0030
    MOV DX, 0x0019
    MOV BP, 0x21FE
    MOV DI, [BX]
    MOV SI, 0x0016
    MOVSW
    MOVSW
    XOR SI, SI
    NOP
    MOV CX, 0x000A
    MOV AX, 0x90A5
    DEC DI
    CALL FAR [BX]
    REP MOVSW
    SUB WORD [BX], 0x0700
    MOV CX, 0x000A
    MOV DI, [BX]
    MOVSW
    MOVSW
    DEC DI
    XOR SI, SI
    LEA SP, [BP+DI+0x04]
    CALL FAR [BX]
    NOP
    SUB SP, DX
    CALL FAR [BX]
    CALL FAR [BX]
    NOP
    NOP
    SUB SP, DX
    INT3
    INT3
    CALL FAR [BX]
    SUB SP, DX
    CALL FAR [BX]
    SUB SP, DX
    INT3
    INT3
    CALL FAR [BX]
    SUB SP, DX
    MOV [BX], AX
    MOV [BX+0x02], CS
    MOV [BX], AX
    MOV [BX+0x02], CS
    REP MOVSW
    MOVSW
    MOVSW
    DEC DI
    CALL FAR [BX]
    REP MOVSW
    NOP
    NOP
    MOVSW
    MOVSW
    DEC DI
    CALL FAR [BX]
    PUSH AX
    POP SI