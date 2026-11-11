    JMP label4
    INT3
    NRG
    SUB AX, 0x5FE3
    PUSH AX
    MOV CX, 0x0024
    MOV [BX+0x00A0], DI
    MOV CX, 0x0019
    PUSH SS
    MOVSW
    JMP AX
    MOVSW
    MOVSW
    JMP AX
    MOVSW
    MOVSW
    JMP AX
    MOVSW
    MOVSW
    MOVSW
    MOV CX, 0x0015
    REP MOVSW
    MOV [DI], AX
label1:
    ADD [BP+0x703D], DL
    OR DH, [BP+SI-0x08]
    MOV DI, [BP+SI]
    CMP DI, [BP+0x00]
    JZ label1+1
    MOV DI, AX
    MOVSW
    DEC WORD [BX+DI-0x5A39]
    JMP AX
    DB 0xC7, 0xA5, 0xFF, 0xE0, 0xE8, 0x00 ;$4 MOV WORD [DI-0x1F01], 0x00E8
    ADD [BX+SI], AL
    REP MOVSW
    PUSH DS
    MOV WORD [SI], 0x00ED
    MOV WORD [SI+0x02], 0x1000
    LOOPNZ -3
    MOVSW
    MOV CX, 0x0017
    MOVSW
    REP MOVSW
    SUB DI, 0x0080
    MOV BX, DI
label2:
    ADD SP, [BX]
    POP DI
    POP BP
    CMP BP, 0x10A5
    JNB label3
    SHL BP, CL
    MOV [BP+DI-0x02], SI
label3:
    MOV BP, [BX]
    CMP [BX+SI], BP
    JZ label2
label4:
    PUSH CS
    PUSH ES
    MOV DX, AX
    POP DS
    LODSW
    MOV BX, AX
    PUSH CS
    POP DS
    SUB AX, 0x5FE3
    PUSH AX
    PUSH DS
    POP ES
    MOV WORD [BX+0x00DD], 0x15E9
    MOV BYTE [BX+0x00DF], 0x01
    LEA DI, [BX+0x01F5]
    MOV CX, 0x0024
    MOV SI, DX
    MOV [SI+0x01C2], DX
    ADD SI, 0x01B6
label5:
    REP MOVSW
    MOV AX, 0xB60F
    XCHG AX, [SI-0x29]
    XCHG AX, [SI-0x1D]
    MOV AX, 0xBA0F
    XCHG AX, [SI-0x21]
    XCHG AX, [SI-0x25]
    MOV WORD [SI-0x02], 0xFEF1
    SUB SI, 0x0004
    MOV BYTE [SI], 0x90
    MOV SI, DX
    MOV DI, 0x00A0
    MOV CX, 0x0019
    PUSH SS
    ADD SI, 0x0114
    POP ES
    PUSH SS
    REP MOVSW
    PUSH DS
    POP ES
    PUSH DX
    MOV CX, 0x0008
    MOV BX, 0x0000
loop1:
    MOV WORD [BX], 0x1FFF
    ADD BX, 0x2000
    LOOP loop1
    MOV CX, 0x0020
    JMP label10
    POP AX
    MOV SI, [0xC781]
    MOV WORD [SI-0x38], 0xFFFF
    MOV SI, [0xC781]
    MOV WORD [SI-0x38], 0xFFFF
    MOV SI, [0x0011]
    POP DS
    MOV SI, 0x00A0
    POP AX
    MOV DI, AX
    MOV DX, 0x0013
    POP SS
    MOVSW
    JMP AX
    MOVSW
    MOVSW
    MOVSW
    MOV CX, 0x0015
    REP MOVSW
    MOV BP, AX
    MOV SP, AX
label6:
    ADD AX, 0x9600
    CMP AX, 0x0A70
    JB label6
    MOV SI, 0x00A0
    SUB BP, 0x003C
    MOV [BP+SI], SI
    MOV [BP+0x00], SI
label7:
    SUB SP, DX
    PUSH CX
    SUB SP, DX
    PUSH CX
    MOV DI, [BP+SI]
    CMP DI, [BP+0x00]
    JZ label7
    MOV DI, AX
    MOVSW
    JMP AX
    CALL prcd1
prcd1:
    MOV BX, AX
    POP AX
    PUSH SS
    SUB AX, 0x0148
    POP ES
    MOV CX, 0x001B
    MOV SI, AX
    MOV DI, 0x00ED
    ADD SI, 0x0180
    REP MOVSW
    PUSH DS
    MOV SI, DI
    PUSH ES
    POP DS
    POP ES
    MOV WORD [SI], 0x00ED
    PUSH CS
    POP SS
    MOV DX, DS
    MOV WORD [SI+0x02], 0x1000
    SUB AX, 0x7343
    MOV SI, 0x00ED
    MOV SP, AX
    ADD AX, BX
    MOV DI, AX
    MOVSW
    JMP AX
    MOVSW
    MOVSW
    MOV CX, 0x0017
    MOVSW
    REP MOVSW
    SUB DI, 0x0080
    MOV BX, DI
    MOV CX, 0x0004
    LDS SI, [SI]
    SUB AX, 0x7343
    MOV [BX+SI], SI
    MOV [BX], SI
label8:
    ADD SP, [BX]
    POP DI
    POP BP
    CMP BP, 0x10A5
    JNB label9
    SHL BP, CL
    MOV [BP+DI-0x02], SI
label9:
    MOV BP, [BX]
    CMP [BX+SI], BP
    JZ label8
    MOV DI, AX
    MOV DS, DX
    MOVSW
    JMP AX
    PUSH DS
    PUSH CS
    POP DS
    MOV CX, 0x0084
label10:
    LOOP $
    MOV DI, 0xE33F
    MOV DX, 0xCCCC
    ADD DX, 0x0145
    MOV [0x4305], DX
    MOV CX, 0x0004
    NRG
    XCHG BP, [0xBC0F]
    XCHG BX, [0xBA0F]
    XCHG AX, [0xB80F]
    XCHG SI, [0xB60F]
    PUSH BP
    PUSH BX
    PUSH AX
    PUSH SI
loop2:
    POP AX
    MUL DI
    MOV BX, 0x2503
    ADD BX, AX
    XOR BX, 0xFAEB
    MOV WORD [BX+0x47], 0x26FF
    LOOP loop2
    POP DS
    JMP label5