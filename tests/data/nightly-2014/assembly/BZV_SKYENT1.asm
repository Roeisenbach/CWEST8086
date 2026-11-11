    JMP label2
    INT3
    INT3
    INT3
    INT3
    PUSH SS
    POP ES
    MOV SI, AX
    ADD SI, WORD 0x003F
    MOV CL, 0x0C
    REP MOVSW
    PUSH SS
    POP DS
    PUSH CS
    POP SS
    PUSH CS
    POP ES
    MOV BX, 0x0330
    SUB AX, 0x1000
    MOV AL, 0x71
    MOV DI, AX
    ADD DI, 0x0014
    NOP
    MOV [BX], DI
    ADD WORD [BX], 0x001D
    MOV WORD [BX+0x02], 0x0FFD
    MOV BP, 0x0002
    MOV DX, 0x5200
    MOV AX, BX
    MOV SI, 0x0015
    JMP label3
    CALL FAR [BX]
    MOVSB
    MOVSW
    REP MOVSW
    MOV CL, 0x07
    ADD DI, [SI]
    LEA SP, [BX+DI]
    XOR SI, SI
    NOP
    ADD [BX+SI], DX
    MOVSW
    DEC DI
    CALL FAR [BX]
    IN AX, DX
    PUSH CX
    INT3
    INT3
    INT3
    INT3
    PUSH SS
    POP ES
    MOV SI, AX
    ADD SI, 0x0091
    MOV CL, 0x0C
    REP MOVSW
    PUSH SS
    POP DS
    PUSH CS
    POP SS
    PUSH CS
    POP ES
    MOV BX, 0x0330
    SUB AX, 0x1000
    MOV AL, 0x71
    NOP
    MOV DI, AX
    ADD DI, 0x0014
    MOV [BX], DI
    ADD WORD [BX], 0x001D
    MOV WORD [BX+0x02], 0x0FFD
    MOV BP, 0x0002
    MOV DX, 0x5200
    MOV AX, BX
    MOV SI, 0x0015
    JMP label1
    CALL FAR [BX]
    MOVSB
    MOVSW
    REP MOVSW
label1:
    MOV CL, 0x07
    ADD DI, [SI]
    NOP
    LEA SP, [BX+DI]
    XOR SI, SI
    ADD [BX+SI], DX
    MOVSW
    DEC DI
    CALL FAR [BX]
    IN AX, DX
    PUSH CX
    NOP
    INT3
    INT3
    INT3
    INT3
label2:
    PUSH SS
    POP ES
    MOV SI, AX
    ADD SI, 0x00EA
    MOV CL, 0x0B
    REP MOVSW
    PUSH SS
    POP DS
    PUSH CS
    POP SS
    PUSH CS
    POP ES
    MOV BX, 0x0330
    NOP
    SUB AX, 0x1000
    MOV AL, 0x71
    MOV DI, AX
    ADD DI, 0x0014
    MOV [BX], DI
    ADD WORD [BX], 0x001D
    MOV WORD [BX+0x02], 0x0FFD
    MOV BP, 0x0002
    MOV DX, 0x5200
    MOV AX, BX
    MOV SI, 0x0014
    JMP label3
    NOP
    NOP
    NOP
    NOP
    NOP
    NOP
    CALL FAR [BX]
    MOVSB
    MOVSW
    REP MOVSW
label3:
    MOV CL, 0x07
    ADD DI, [SI]
    LEA SP, [BX+DI]
    XOR SI, SI
    ADD [BX+SI], DX
    MOVSW
    DEC DI
    CALL FAR [BX]
    IN AX, DX
    PUSH CX
    INT3
    INT3
    INT3
    INT3
    PUSH SS
    POP ES
    MOV SI, AX
    ADD SI, 0x013F
    MOV CL, 0x0B
    NOP
    NOP
    NOP
    REP MOVSW
    PUSH SS
    POP DS
    PUSH CS
    POP SS
    PUSH CS
    POP ES
    MOV BX, 0x0330
    SUB AX, 0x1000
    MOV AL, 0x71
    MOV DI, AX
    ADD DI, 0x0014
    MOV [BX], DI
    ADD WORD [BX], 0x001D
    MOV WORD [BX+0x02], 0x0FFD
    MOV BP, 0x0002
    MOV DX, 0x5200
    NOP
    NOP
    MOV AX, BX
    MOV SI, 0x0014
    JMP label4
    CALL FAR [BX]
    MOVSB
    MOVSW
    REP MOVSW
label4:
    MOV CL, 0x07
    ADD DI, [SI]
    LEA SP, [BX+DI]
    XOR SI, SI
    ADD [BX+SI], DX
    MOVSW
    DEC DI
    CALL FAR [BX]
    IN AX, DX
    PUSH CX
    INT3
    INT3
    INT3
    INT3
    PUSH SS
    POP ES
    MOV SI, AX
    NOP
    NOP
    ADD SI, 0x0193
    MOV CL, 0x0B
    REP MOVSW
    PUSH SS
    POP DS
    PUSH CS
    POP SS
    PUSH CS
    POP ES
    MOV BX, 0x0330
    SUB AX, 0x1000
    MOV AL, 0x71
    MOV DI, AX
    ADD DI, 0x0014
    MOV [BX], DI
    ADD WORD [BX], 0x001D
    MOV WORD [BX+0x02], 0x0FFD
    NOP
    NOP
    MOV BP, 0x0002
    MOV DX, 0x5200
    MOV AX, BX
    MOV SI, 0x0014
    JMP label5
    CALL FAR [BX]
    MOVSB
    MOVSW
    REP MOVSW
label5:
    MOV CL, 0x07
    ADD DI, [SI]
    LEA SP, [BX+DI]
    XOR SI, SI
    ADD [BX+SI], DX
    MOVSW
    DEC DI
    CALL FAR [BX]
    IN AX, DX
    PUSH CX