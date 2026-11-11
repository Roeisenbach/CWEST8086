    XCHG BX, AX
    PUSH CS
    PUSH ES
    POP DS
    LODSW
    POP DS
    MOV DI, AX
    PUSH ES
    PUSH DS
    POP ES
    MOV WORD [DI+0x00A9], 0x071E
    MOV [DI+0x00AD], BX
    MOV WORD [DI+0x00AB], 0x26FF
    ADD BX, WORD 0x003C
    MOV [BX-0x3C], BX
    MOV [BX+0x01], BX
    POP ES
    PUSH SS
    PUSH CS
    POP ES
    MOV DX, 0xCCCC
    MOV CX, 0xCCCC
    XCHG CX, AX
    MOV DI, 0x0700
    INT 0x86
    INT 0x86
    XCHG CX, AX
    POP ES
    JMP label3
    MOV BP, 0xCCCC
    SUB BP, WORD 0x003C
    MOV AX, BP
    XOR DI, DI
    MOV BX, 0x0400
    PUSH CS
    PUSH SS
    PUSH SS
    PUSH CS
    POP ES
    MOV DX, 0xCCCC
    MOV CX, 0xCCCC
    XCHG CX, AX
    MOV DI, 0x0700
    INT 0x86
    INT 0x86
    XCHG CX, AX
    POP ES
    MOV SI, AX
    ADD SI, 0x00CA
    PUSH SS
    POP ES
    XOR DI, DI
    ADD AX, 0x00C5
    MOV CX, 0x0014
    REP MOVSW
    POP DS
    MOV DX, 0x1FFF
    MOV CX, 0x10A5
    MOV [BX+0x02], CX
    MOV [BX], AX
    SUB WORD [BX], 0x0A50
    MOV DI, [BX]
    ADD DI, 0x0A50
    PUSH CS
    POP ES
    POP SS
    MOV SP, AX
    ADD SP, 0x02FE
    MOV BP, SP
    MOV [BP-0x02FE], DX
    MOV CX, 0x10A5
    MOV BP, AX
    CMP AX, 0xF43A
    JB label1
    MOV WORD [BP-0x0A5C], 0xC98E
    SUB BP, 0x0A5C
    JMP BP
label1:
    ADD BP, 0x0A44
    MOV WORD [BP+0x04], 0xADE9
    MOV BYTE [BP+0x06], 0xF5
    XOR SI, SI
    MOV CS, CX
    XOR CX, CX
    MOV AX, 0xEC83
    MOV BP, 0x10CC
    SUB SP, 0x0010
    CALL FAR [BX]
    MOVSW
    MOVSW
    MOVSW
    MOV CL, 0x10
    MOVSW
    REP MOVSW
    CMP SP, 0x1460
    JA label2
    SUB SP, 0x11B0
label2:
    SUB SP, 0x09FE
    PUSH DX
    MOV DI, SP
    SUB SP, 0x0A50
    MOV [BX], SP
    ADD SP, 0x144E
    XOR SI, SI
    CALL FAR [BX]
label3:
    XCHG BX, AX
    SUB AX, 0x003C
    XOR DI, DI
    MOV BX, 0x0400
    PUSH CS
    PUSH SS
    MOV SI, AX
    ADD SI, 0x010F
    PUSH SS
    POP ES
    ADD AX, 0x0185
    MOV CX, 0x0014
    REP MOVSW
    POP DS
    JMP label5
    MOVSW
    MOVSW
    MOVSW
    MOV CL, 0x10
    MOVSW
    REP MOVSW
    CMP SP, 0x1460
    JA label4
    SUB SP, 0x11B0
label4:
    SUB SP, 0x09FE
    PUSH DX
    MOV DI, SP
    SUB SP, 0x0A50
    MOV [BX], SP
    ADD SP, 0x144E
    XOR SI, SI
    CALL FAR [BX]
label5:
    MOV DX, 0x1FFF
    MOV CX, 0x10A5
    MOV [BX+0x02], CX
    MOV [BX], AX
    SUB WORD [BX], 0xAA50
    MOV DI, [BX]
    ADD DI, 0x0A50
    PUSH CS
    POP ES
    POP SS
    MOV SP, AX
    ADD SP, 0x62FE
    MOV BP, SP
    MOV WORD [BP-0x02FE], 0x1FFF
    MOV CX, 0x10A5
    MOV BP, AX
    CMP AX, 0xF43A
    JB label6
    MOV WORD [BP-0x0A56], 0xC98E
    SUB BP, 0x0A56
    JMP BP
label6:
    ADD BP, 0x0A4A
    MOV WORD [BP+0x02], 0xADE9
    MOV BYTE [BP+0x04], 0xF5
    MOV CS, CX
    XOR SI, SI
    XOR CX, CX
    CALL FAR [BX]