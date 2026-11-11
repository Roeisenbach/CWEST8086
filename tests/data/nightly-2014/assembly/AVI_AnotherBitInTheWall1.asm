    JMP label1+1
    JMP label1+1
    MOV CL, 0x0E
    ADD SI, AX
    ADD SI, 0x00A3
    REP MOVSW
    PUSH CS
    PUSH ES
    POP DS
    POP ES
    PUSH CS
    POP SS
    MOV DI, 0x0988
    MOV BP, 0x1FFF
    MOV DX, 0xC429
    MOV SI, 0x0200
    LODSW
    ADD AH, 0x9E
    MOV BL, 0x66
    NOP
    XOR SI, SI
    MOV CL, 0x0B
    SUB DI, 0xFFE8
    MOV [BX], DI
    MOV [BX+0x01], AH
    MOV AX, 0x0126
    MOV WORD [BX+0x02], 0x1009
    MOV SP, [BX]
    MOV DI, [BX]
    ADD DI, 0x0091
    INC SI
    MOVSW
    REP MOVSW
    MOV CL, 0x0B
    SUB WORD [BX], 0x0500
    NOP
    DEC SP
    SUB DI, 0x051A
    LEA SP, [DI+0x04]
    INC DI
    PUSH BP
    PUSH DX
    ADD SP, [SI]
    XOR SI, SI
    JMP FAR [BX]
    XCHG SP, AX
label1:
    MOV SP, 0x0EB1
    ADD SI, AX
    ADD SI, 0x00A3
    REP MOVSW
    PUSH CS
    PUSH ES
    POP DS
    POP ES
    PUSH CS
    POP SS
    MOV DI, 0x0988
    NOP
    MOV BP, 0x1FFF
    MOV DX, 0xC429
    MOV SI, 0x0200
    LODSW
    ADD AH, 0x7E
    MOV BL, 0x66
    XOR SI, SI
    MOV CL, 0x0B
    SUB DI, 0xFFE8
    MOV [BX], DI
    MOV [BX+0x01], AH
    MOV AX, 0x0126
    MOV WORD [BX+0x02], 0x100D
    MOV SP, [BX]
    DEC SP
    NOP
    MOV DI, [BX]
    ADD DI, 0x00D2
    INC SI
    MOVSW
    REP MOVSW
    MOV CL, 0x0B
    SUB WORD [BX], 0x0500
    SUB DI, 0x051A
    LEA SP, [DI+0x04]
    INC DI
    PUSH BP
    PUSH DX
    ADD SP, [SI]
    XOR SI, SI
    JMP FAR [BX]
    XCHG SP, AX
    MOV SP, 0x0EB1
    ADD SI, AX
    NOP
    ADD SI, 0x00A3
    REP MOVSW
    PUSH CS
    PUSH ES
    POP DS
    POP ES
    PUSH CS
    POP SS
    MOV DI, 0x0988
    MOV BP, 0x1FFF
    MOV DX, 0xC429
    MOV SI, 0x0200
    ADD AH, 0x68
    MOV BL, 0x66
    XOR SI, SI
    MOV CL, 0x0B
    SUB DI, 0xFFE8
    MOV [BX], DI
    NOP
    LODSW
    MOV [BX+0x01], AH
    MOV AX, 0x0126
    MOV WORD [BX+0x02], 0x1009
    MOV SP, [BX]
    DEC SP
    MOV DI, [BX]
    ADD DI, 0x0092
    INC SI
    MOVSW
    REP MOVSW
    MOV CL, 0x0B
    SUB WORD [BX], 0x0500
    SUB DI, 0x051A
    LEA SP, [DI+0x04]
    NOP
    INC DI
loop1:
    PUSH BP
    PUSH DX
    ADD SP, [SI]
    XOR SI, SI
    JMP FAR [BX]
    XCHG SP, AX
    MOV SP, 0x9341
    LOOP loop1
    JMP label1+1