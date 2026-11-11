    JMP label1+1
    JMP label1+1
    JMP label1+1
    MOV DI, 0x0200
    STOSW
    XOR [BP+DI+0x02EB], AL
    ADD SI, AX
    MOV CL, 0x0A
    ADD SI, 0x009E
    MOV DI, 0x0040
    REP MOVSW
    MOV BP, 0x3098
    MOV DX, 0x3400
    MOV BL, 0x70
    MOV SI, 0x0040
    NOP
    SUB AH, 0x34
    PUSH CS
    PUSH ES
    POP DS
    POP ES
    MOV [BX+0x01], AH
    MOV CL, 0x09
    MOV WORD [BX+0x02], 0x1003
    SUB AH, 0x34
    PUSH CS
    POP SS
    MOV BYTE [BX], 0xA3
    MOV DI, [BX]
    ADD DI, 0x0033
    MOV AX, 0x1FFF
    MOV SP, [BX]
    REP MOVSW
    NOP
    NOP
    SUB DI, 0x3413
    MOV SI, [SI]
    SUB [BX], DX
    MOV CL, 0x08
    SUB SP, BP
    STOSW
    CALL FAR [BX]
    INC AX
label1:
    ADD [BX+0x0200], BH
    STOSW
    ADD SI, AX
    MOV CL, 0x0A
    ADD SI, 0x009E
    MOV DI, 0x0040
    REP MOVSW
    MOV BP, 0x3098
    PUSH CS
    NOP
    MOV DX, 0x3400
    MOV BL, 0x70
    MOV SI, 0x0040
    ADD AH, 0x50
    PUSH ES
    POP DS
    POP ES
    MOV [BX+0x01], AH
    MOV CX, 0x0009
    MOV WORD [BX+0x02], 0x1003
    PUSH CS
    POP SS
    MOV BYTE [BX], 0xA3
    MOV DI, [BX]
    ADD DI, 0x0031
    MOV AX, 0x1FFF
    NOP
    MOV SP, [BX]
    REP MOVSW
    SUB DI, 0x3413
    MOV SI, [SI]
    SUB [BX], DX
    MOV CL, 0x08
    SUB SP, BP
    STOSW
    DEC DI
    CALL FAR [BX]
    INC AX
    ADD [BX+0x0200], BH
    STOSW
    ADD SI, AX
    MOV CL, 0x0A
    ADD SI, 0x009E
    MOV DI, 0x0040
    NOP
    NOP
    REP MOVSW
    MOV BP, 0x3098
    MOV DX, 0x3400
    MOV BL, 0x70
    MOV SI, 0x0040
    SUB AH, 0x34
    PUSH CS
    PUSH ES
    POP DS
    POP ES
    MOV [BX+0x01], AH
    MOV CL, 0x09
    MOV WORD [BX+0x02], 0x1003
    SUB AH, 0x00
    PUSH CS
    POP SS
    MOV BYTE [BX], 0xA3
    NOP
    MOV DI, [BX]
    ADD DI, 0x0031
    MOV AX, 0x1FFF
    MOV SP, [BX]
    REP MOVSW
    SUB DI, 0x3413
    MOV SI, [SI]
    SUB [BX], DX
loop1:
    MOV CL, 0x08
    SUB SP, BP
    STOSW
    DEC DI
    CALL FAR [BX]
    INC AX
    ADD [BX+DI-0x6D], AL
    LOOP loop1
    JMP label1+1