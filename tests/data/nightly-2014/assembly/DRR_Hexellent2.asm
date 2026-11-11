    PUSH ES
    XCHG DI, AX
    POP DS
    LODSW
    PUSH CS
    LEA BX, [DI+0x1C]
    POP DS
    MOV [DI], BX
    XCHG DI, AX
    MOV [DI+0x0F], AX
    MOV WORD [DI+0x0D], 0x26FF
    PUSH SS
    NOP
    POP ES
    PUSH AX
    NOP
    JMP label1
    NOP
    PUSH AX
    MOV AX, 0xFFDD
    CALL prcd1
prcd1:
    POP SI
    ADD AX, SI
label1:
    JMP label2+1
label2:
    ADC BX, BP
    DB 0x03, 0xDE ;$1 ADD BX, SI
    JMP label4
    JMP label5
label3:
    MOV SI, 0x2000
    PUSH SI
    SUB SP, 0x0002
    RETF
label4:
    JMP label3
label5:
    XOR DI, DI
    MOV CL, 0x16
    MOV SI, 0x00F4
    ADD SI, AX
    REP MOVSW
    CMP BX, 0x11BF
    JNZ label6
    JMP label11
label6:
    POP DX
    CMP AX, DX
    PUSH DX
    JNZ label7
    MOV SI, 0xC80F
    JMP label8
label7:
    MOV SI, 0xC40F
    NOP
label8:
    MOV BP, AX
    MOV BX, BP
    SUB AX, 0xFEE0
    MOV CL, 0x0D
    MOV [BX], AX
    LOOP $
    ADD CL, 0x15
    XCHG AX, [SI]
    MOV SI, BX
    MOV BX, 0xDE3F
loop1:
    SUB AX, 0x4743
    MUL BX
    LOOP loop1
    XCHG BX, AX
    SUB BX, 0xFAEB
    MOV DX, 0x26FF
    MOV [BX+0x4E], SI
    XCHG DX, [BX+0x4C]
    POP DX
    CMP AX, DX
    PUSH DX
    JNZ label9
    MOV SI, 0xB80F
    MOV DI, 0xB60F
    JMP label10
label9:
    MOV SI, 0xBC0F
    MOV DI, 0xBA0F
    NOP
label10:
    MOV BP, AX
    MOV BX, BP
    SUB AX, 0xFEE0
    MOV CL, 0x0C
    MOV [BX], AX
    LOOP $
    ADD CL, 0x02
    XCHG SI, [SI]
    XCHG DI, [DI]
    XCHG BP, SI
loop2:
    XOR AX, AX
    ADD AX, 0xE33F
    MUL BP
    XCHG BX, AX
    SUB BX, 0xDAFD
    XOR BX, 0xFAEB
    MOV DX, 0x26FF
    MOV [BX+0x4E], SI
    XCHG DX, [BX+0x4C]
    XCHG DI, BP
    LOOP loop2
    XOR DX, DX
label11:
    PUSH SS
    PUSH DS
    POP ES
    POP DS
    POP AX
    PUSH ES
    POP SS
    MOV AL, 0xE0
    MOV SP, 0x03CA
    DEC AH
    ADD SP, AX
    MOV BX, 0x0078
    MOV [BX+0x46], ES
    MOV DI, AX
    MOV [BX+0x44], AX
    MOV AX, 0x0005
    JMP label12
    INT3
    REP MOVSW
    SUB SP, 0x003D
    SUB WORD [BX+0x44], 0x0400
    SUB DI, 0x042C
label12:
    XOR SI, SI
    MOV CL, 0x15
    DEC DX
    JZ label13
    MOV BP, [BX+0x44]
    DEC BP
    MOV WORD [BP+0x00], 0x2BA5
    MOV WORD [BP+0x02], 0xFFE0
    MOV WORD [BP+0x04], 0x445F
    CALL FAR [BX+0x44]
label13:
    DB 0xC9
    PUSH AX
    XOR DX, DX
    ADD DX, 0x0319
    CALL prcd2
    INT3
prcd2:
    POP AX
    ADD AX, 0xFED6
    PUSH SS
    POP ES
    PUSH CS
    POP DS
    JMP label5