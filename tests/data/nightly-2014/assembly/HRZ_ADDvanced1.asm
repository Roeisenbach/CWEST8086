    JMP label1
    CALL FAR [BX]
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
    JMP label2
    MOV [BX], AX
    MOV [BX+0x02], CS
    MOV [BX], AX
    MOV [BX+0x02], CS
    REP MOVSW
    MOVSW
    MOVSW
    CALL FAR [BX]
    NOP
    DEC DI
    REP MOVSW
    MOVSW
    DEC DI
    CALL FAR [BX]
label1:
    PUSH CS
    MOV BP, 0x0FFE
    MOV CS, BP
label2:
    MOV SI, AX
    ADD SI, 0x00C3
    MOV CX, 0x000D
    MOV DI, 0x0100
    REP MOVSW
    PUSH ES
    POP DS
    MOV BX, 0x0202
    MOV [BX], AX
    MOV AX, [BX-0x02]
    PUSH AX
    NOP
    MOV ES, BP
    LEA DI, [SI+0x082A]
    MOV AX, 0xCCCC
    MOV DX, AX
    INT 0x86
    ADD DI, 0x003C
    INT 0x86
    POP AX
    MOV AL, 0xAF
    ADD AH, 0x73
    POP SS
    MOV SP, AX
    SUB SP, 0x00E8
    SUB AX, 0x1E08
    MOV BP, SS
    MOV ES, BP
    NOP
    MOV BX, 0x0130
    MOV [BX], AX
    MOV [BX+0x02], CS
    MOV AX, 0x1FFF
    MOV BX, 0xCCCC
    MOV CX, BX
    XOR DI, DI
    INT 0x87
    MOV BP, CS
    MOV ES, BP
    MOV BX, 0x0130
    MOV DX, 0x0019
    MOV BP, 0x21FE
    MOV DI, [BX]
    MOV SI, 0x0116
    NOP
    MOVSW
    MOVSW
    MOV SI, 0x0100
    MOV CX, 0x000A
    MOV AX, 0x90A5
    DEC DI
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
    NOP
    REP MOVSW
    SUB WORD [BX], 0x0700
    MOV CX, 0x000A
    MOV DI, [BX]
    MOVSW
    MOVSW
    DEC DI
    MOV SI, 0x0100
    LEA SP, [BP+DI+0x1B]
    CALL FAR [BX]
    SUB SP, DX
    CALL FAR [BX]
    CALL FAR [BX]
    SUB SP, DX
    INT3
    INT3
    CALL FAR [BX]
    SUB SP, DX
    CALL FAR [BX]
    NOP
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
    MOVSW
    MOVSW
    DEC DI
    CALL FAR [BX]
    PUSH AX
    POP SI