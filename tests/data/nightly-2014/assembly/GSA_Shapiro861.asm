    JMP label1
    REP MOVSW
    PUSH DS
    PUSH ES
    REP MOVSW
    PUSH DS
    PUSH ES
    SUB SP, BP
    CALL FAR [BX]
    INT3
    INT3
    PUSH SS
    POP ES
    MOV CL, 0x12
    MOV SI, AX
    ADD SI, 0x00DE
    REP MOVSW
    PUSH DS
    PUSH ES
    POP DS
    POP ES
    MOV BX, 0x0040
    MOV AL, 0xA0
    MOV [BX], AX
    MOV WORD [BX+0x02], 0x0FFA
    MOV CL, 0x0E
    PUSH CS
    POP SS
    MOV AL, 0x40
    MOV SP, AX
    MOV SI, 0x001F
    MOV BP, 0x01FC
    JMP label2
    MOVSW
    REP MOVSW
    ADD WORD [BX], 0x0300
    ADD WORD [BX+0x02], 0x0001
    LES DI, [BX]
    ADD SP, 0x0114
    MOVSW
    MOVSW
    DEC DI
    DEC DI
    DEC DI
    XOR SI, SI
    MOV CL, 0x0E
    DEC DX
    DB 0x0F
    TEST [DI+0x1800], BX
    INT3
    SUB SP, BP
    CALL FAR [BX+SI]
    PUSH SS
    POP ES
    MOV CL, 0x12
    MOV SI, AX
    ADD SI, 0x00DE
    REP MOVSW
    PUSH DS
    PUSH ES
    POP DS
    POP ES
    MOV BX, 0x0040
    MOV AL, 0xA0
    MOV [BX], AX
    MOV WORD [BX+0x02], 0x0FFA
    MOV CL, 0x0E
    PUSH CS
    POP SS
    MOV AL, 0x40
    MOV SP, AX
    MOV SI, 0x001F
    MOV BP, 0x01FC
    JMP label2
    MOVSW
    REP MOVSW
    ADD WORD [BX], 0x0300
    ADD WORD [BX+0x02], 0x0001
    LES DI, [BX]
    ADD SP, 0x0114
    MOVSW
    MOVSW
    DEC DI
    DEC DI
    DEC DI
    XOR SI, SI
    MOV CL, 0x0E
    DEC DX
    JNZ label3+1
    SBB AH, CL
    SUB SP, BP
    CALL FAR [BX+SI]
label1:
    PUSH SS
    POP ES
    MOV CL, 0x12
    MOV SI, AX
    ADD SI, 0x00DE
    REP MOVSW
    PUSH DS
    PUSH ES
    POP DS
    POP ES
    MOV BX, 0x0040
    MOV AL, 0xA0
    MOV [BX], AX
    MOV WORD [BX+0x02], 0x0FFA
    MOV CL, 0x0E
    PUSH CS
    POP SS
    MOV AL, 0x40
    MOV SP, AX
    MOV SI, 0x001F
    MOV BP, 0x01FC
    JMP label2
    MOVSW
    REP MOVSW
label2:
    ADD WORD [BX], 0x0300
    ADD WORD [BX+0x02], 0x0001
    LES DI, [BX]
    ADD SP, 0x0114
    MOVSW
    MOVSW
    DEC DI
    DEC DI
    DEC DI
    XOR SI, SI
    MOV CL, 0x0E
    DEC DX
label3:
    JNZ label3+1
    SBB AH, CL
    SUB SP, BP
    CALL FAR [BX+SI]
    PUSH SS
    POP ES
    MOV CL, 0x12
    MOV SI, AX
    ADD SI, 0x00DE
    REP MOVSW
    PUSH DS
    PUSH ES
    POP DS
    POP ES
    MOV BX, 0x0040
    MOV AL, 0xA0
    MOV [BX], AX
    MOV WORD [BX+0x02], 0x0FFA
    MOV CL, 0x0E
    PUSH CS
    POP SS
    MOV AL, 0x40
    MOV SP, AX
    MOV SI, 0x001F
    MOV BP, 0x01FC
    JMP label2
    MOVSW
    REP MOVSW
    ADD WORD [BX], 0x0300
    ADD WORD [BX+0x02], 0x0001
    LES DI, [BX]
    ADD SP, 0x0114
    MOVSW
    MOVSW
    DEC DI
    DEC DI
    DEC DI
    XOR SI, SI
    MOV CL, 0x0E
    DEC DX
    JNZ label3+1
    SBB AH, CL
    SUB SP, BP
    CALL FAR [BX+SI]
    PUSH SS
    POP ES
    MOV CL, 0x12
    MOV SI, AX
    ADD SI, 0x00DE
    REP MOVSW
    PUSH DS
    PUSH ES
    POP DS
    POP ES
    MOV BX, 0x0040
    MOV AL, 0xA0
    MOV [BX], AX
    MOV WORD [BX+0x02], 0x0FFA
    MOV CL, 0x0E
    PUSH CS
    POP SS
    MOV AL, 0x40
    MOV SP, AX
    MOV SI, 0x001F
    MOV BP, 0x01FC
    JMP label2
    MOVSW
    REP MOVSW
    ADD WORD [BX], 0x0300
    ADD WORD [BX+0x02], 0x0001
    LES DI, [BX]
    ADD SP, 0x0114
    MOVSW
    MOVSW
    DEC DI
    DEC DI
    DEC DI
    XOR SI, SI
    MOV CL, 0x0E
    DEC DX
    DB 0x0F
    TEST [BP-0x01], BX
    SBB AH, CL
    SUB SP, BP
    CALL FAR [BX+SI]