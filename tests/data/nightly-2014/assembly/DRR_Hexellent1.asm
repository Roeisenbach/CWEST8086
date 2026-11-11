    ADD AX, 0x00B1
    JMP label6
    INT3
    INT3
    INT3
    STOSW
    PUSH AX
label1:
    XOR DI, DI
    PUSH SS
    POP ES
    MOV SI, AX
    ADD SI, 0x007A
    MOV CL, 0x0E
    REP MOVSW
    PUSH CS
    POP ES
    MOV BX, 0x26FF
    MOV CX, AX
    MOV SI, AX
    ADD AX, 0x0097
    MOV [SI], AX
    MOV AX, 0x432D
    MOV DX, 0xF747
    INT 0x87
    XCHG DX, BP
    XOR CX, CX
    POP DI
    CMP SI, DI
    JNZ label3
    MOV CL, 0x80
    LOOP $
    MOV SI, 0xDE3F
    XCHG AX, [0xBA0F]
    XCHG BP, [0xBC0F]
label2:
    MOV CL, 0x35
loop1:
    ADD AX, 0xB8BD
    MUL SI
    LOOP loop1
    XCHG BX, AX
    XOR BX, 0xFAEB
    MOV WORD [BX+0x4C], 0x26FF
    MOV [BX+0x4E], DI
    CMP AX, 0x26FF
    XCHG BP, AX
    JZ label2
    XOR DX, DX
label3:
    MOV AX, DI
    PUSH CS
    PUSH SS
    POP DS
    POP SS
    DEC AH
    MOV AL, 0xA3
    MOV BX, 0x0100
    MOV [BX], AX
    MOV [BX+0x02], CS
    MOV SP, AX
    ADD SP, 0x03EC
    XCHG DI, AX
    INC DI
    MOV AX, 0x0114
    JMP label4
    REP MOVSW
    SUB SP, AX
    SUB WORD [BX], 0x0500
    SUB DI, 0x051C
label4:
    XOR SI, SI
    MOV CL, 0x0D
    DEC DX
    JZ label5
    MOV BP, [BX]
    MOV WORD [BP+0x00], 0x1FFF
    CALL FAR [BX]
label5:
    INT3
    MOV BP, 0x02C3
    PUSH AX
    CALL prcd1
prcd1:
    POP AX
    SUB AX, 0x009E
    JMP label1
    INT3
    INT3
    INT3
label6:
    STOSW
    PUSH AX
label7:
    XOR DI, DI
    PUSH SS
    POP ES
    MOV SI, AX
    ADD SI, 0x007A
    MOV CL, 0x0E
    REP MOVSW
    PUSH CS
    POP ES
    MOV BX, 0x26FF
    MOV CX, AX
    MOV SI, AX
    ADD AX, 0x0097
    MOV [SI], AX
    MOV AX, 0x432D
    MOV DX, 0xF747
    INT 0x87
    XCHG DX, BP
    XOR CX, CX
    POP DI
    CMP SI, DI
    JNZ label9
    MOV CL, 0x80
    LOOP $
    MOV SI, 0xDE3F
    XCHG AX, [0xBA0F]
    XCHG BP, [0xBC0F]
label8:
    MOV CL, 0x35
loop2:
    ADD AX, 0xB8BD
    MUL SI
    LOOP loop2
    XCHG BX, AX
    XOR BX, 0xFAEB
    MOV WORD [BX+0x4C], 0x26FF
    MOV [BX+0x4E], DI
    CMP AX, 0x26FF
    XCHG BP, AX
    JZ label8
    XOR DX, DX
label9:
    MOV AX, DI
    PUSH CS
    PUSH SS
    POP DS
    POP SS
    DEC AH
    MOV AL, 0xA3
    MOV BX, 0x0100
    MOV [BX], AX
    MOV [BX+0x02], CS
    MOV SP, AX
    ADD SP, 0x03EC
    XCHG DI, AX
    INC DI
    MOV AX, 0x0114
    JMP label10
    REP MOVSW
    SUB SP, AX
    SUB WORD [BX], 0x0500
    SUB DI, 0x051C
label10:
    XOR SI, SI
    MOV CL, 0x0D
    DEC DX
    JZ label11
    MOV BP, [BX]
    MOV WORD [BP+0x00], 0x1FFF
    CALL FAR [BX]
label11:
    INT3
    MOV BP, 0x02C3
    PUSH AX
    CALL prcd2
prcd2:
    POP AX
    SUB AX, 0x009E
    JMP label7
    INT3
    INT3
    INT3
    STOSW
    PUSH AX
label12:
    XOR DI, DI
    PUSH SS
    POP ES
    MOV SI, AX
    ADD SI, 0x007A
    MOV CL, 0x0E
    REP MOVSW
    PUSH CS
    POP ES
    MOV BX, 0x26FF
    MOV CX, AX
    MOV SI, AX
    ADD AX, 0x0097
    MOV [SI], AX
    MOV AX, 0x432D
    MOV DX, 0xF747
    INT 0x87
    XCHG DX, BP
    XOR CX, CX
    POP DI
    CMP SI, DI
    JNZ label14
    MOV CL, 0x80
    LOOP $
    MOV SI, 0xDE3F
    XCHG AX, [0xBA0F]
    XCHG BP, [0xBC0F]
label13:
    MOV CL, 0x35
loop3:
    ADD AX, 0xB8BD
    MUL SI
    LOOP loop3
    XCHG BX, AX
    XOR BX, 0xFAEB
    MOV WORD [BX+0x4C], 0x26FF
    MOV [BX+0x4E], DI
    CMP AX, 0x26FF
    XCHG BP, AX
    JZ label13
    XOR DX, DX
label14:
    MOV AX, DI
    PUSH CS
    PUSH SS
    POP DS
    POP SS
    DEC AH
    MOV AL, 0xA3
    MOV BX, 0x0100
    MOV [BX], AX
    MOV [BX+0x02], CS
    MOV SP, AX
    ADD SP, 0x03EC
    XCHG DI, AX
    INC DI
    MOV AX, 0x0114
    JMP label15
    REP MOVSW
    SUB SP, AX
    SUB WORD [BX], 0x0500
    SUB DI, 0x051C
label15:
    XOR SI, SI
    MOV CL, 0x0D
    DEC DX
    JZ label16
    MOV BP, [BX]
    MOV WORD [BP+0x00], 0x1FFF
    CALL FAR [BX]
label16:
    INT3
    MOV BP, 0x02C3
    PUSH AX
    CALL prcd3
prcd3:
    POP AX
    SUB AX, 0x009E
    JMP label12
    INT3
    INT3