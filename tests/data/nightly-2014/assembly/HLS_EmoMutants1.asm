    JMP label1
    MOV SI, AX
    ADD SI, 0x0143
    PUSH ES
    POP DS
    MOV AX, [BX]
    PUSH CS
    POP DS
    PUSH CS
    POP SS
    MOV DI, 0x0114
    MOV CL, 0x10
    REP MOVSW
    MOV CX, 0x1994
    MOV BP, 0x05DC
    PUSH CS
    PUSH ES
    POP DS
    POP ES
    ADD AX, 0x018E
    NOP
    MOV BX, 0x0190
    MOV [BX+0x02], CS
    MOV [BX], AX
    MOV SP, AX
    MOV DI, AX
    MOV AX, 0x1FFF
    ADD SP, 0x0640
    STOSW
    SUB DI, WORD 0x0002
    MOV SI, 0x0114
    MOV DX, SI
    CALL FAR [BX]
    MOVSW
    MOVSW
    MOVSW
    MOVSW
    MOVSW
    MOVSW
    NOP
    MOVSW
    MOVSW
    MOVSW
    MOVSW
    MOVSW
    MOVSW
    MOVSW
    MOVSW
    MOVSW
    PUSH AX
    MOV [BX], SP
    SUB SP, CX
    CALL FAR [BX]
    INC SI
    PUSH ES
    POP DS
    MOV AX, [BX]
    PUSH CS
    MOV DI, 0x0196
    MOV CL, 0x10
    REP MOVSW
    MOV CX, 0x1994
    MOV BP, 0x05DC
    PUSH CS
    PUSH ES
    NOP
    POP DS
    POP ES
    ADD AX, 0x018E
    MOV BX, 0x0190
    MOV [BX+0x02], CS
    MOV [BX], AX
    MOV AX, 0x1FFF
    ADD SP, 0x0640
    STOSW
    SUB DI, WORD 0x0002
    MOV SI, 0x0114
    MOV DX, SI
    CALL FAR [BX]
    MOVSW
    MOVSW
    MOVSW
    MOVSW
    MOVSW
    NOP
    MOVSW
    MOVSW
    MOVSW
    MOVSW
    MOVSW
    MOVSW
    MOVSW
    MOVSW
    MOVSW
    MOVSW
    MOV SP, [BX]
    MOV SI, DX
    SUB SP, BP
    MOV DI, SP
    PUSH AX
    MOV [BX], SP
    SUB SP, CX
    CALL FAR [BX]
label1:
    ADD AX, 0x0134
    PUSH AX
    MOV DX, [0x6769]
    MOV SI, [0x6769]
    NOP
    PUSH ES
    MOV DI, [0x6969]
    PUSH CS
    POP ES
    MOV AX, 0xCCCC
    CMP DI, AX
    JZ label2
    MOV CL, 0x08
    REP STOSW
label2:
    CMP DX, AX
    JZ label3
    MOV CL, 0x08
    MOV DI, DX
    REP STOSW
label3:
    CMP SI, AX
    JZ label4
    MOV CL, 0x08
    MOV DI, SI
    NOP
    NOP
    REP STOSW
label4:
    POP DS
    MOV SI, 0x03FE
    LODSW
    POP SI
    PUSH DS
    POP ES
    MOV BX, CS
    MOV DS, BX
    MOV SS, BX
    MOV DI, 0x0114
    MOV CL, 0x10
    REP MOVSW
    MOV CX, 0x1994
    MOV BP, 0x05DC
    PUSH ES
    POP DS
    MOV ES, BX
    MOV BX, 0x29A5
    NOP
    NOP
label5:
    CMP BX, [0x0142]
    JNZ label5
    ADD AX, 0x018E
    MOV BX, 0x0190
    MOV [BX+0x02], CS
    MOV [BX], AX
    MOV SP, AX
    MOV DI, AX
    MOV AX, 0x1FFF
    ADD SP, 0x0640
    STOSW
    SUB DI, WORD 0x0002
    MOV SI, 0x0114
    NOP
    NOP
    MOV DX, SI
    CALL FAR [BX]
    MOVSW
    MOVSW
    MOVSW
    MOVSW
    MOVSW
    MOVSW
    MOVSW
    MOVSW
    MOVSW
    MOVSW
    MOVSW
    MOVSW
    MOVSW
    MOVSW
    MOVSW
    MOV SP, [BX]
    MOV SI, DX
    SUB SP, BP
    MOV DI, SP
    PUSH AX
    MOV [BX], SP
    SUB SP, CX
    CALL FAR [BX]
    MOV SI, AX
    NOP
    NOP
    ADD SI, 0x0143
    PUSH ES
    POP DS
    MOV AX, [BX]
    PUSH CS
    POP DS
    PUSH CS
    POP SS
    MOV DI, 0x0114
    MOV CL, 0x10
    REP MOVSW
    MOV CX, 0x1994
    MOV BP, 0x05DC
    PUSH CS
    PUSH ES
    POP DS
    POP ES
    ADD AX, 0x018E
    MOV BX, 0x0190
    JMP label6
    NOP
label6:
    MOV [BX+0x02], CS
    MOV [BX], AX
    MOV SP, AX
    MOV DI, AX
    MOV AX, 0x1FFF
    ADD SP, WORD 0x0064
    STOSW
    SUB DI, WORD 0x0002
    MOV SI, 0x0114
    MOV DX, SI
    CALL FAR [BX]
    MOVSW
    MOVSW
    MOVSW
    MOVSW
    MOVSW
    MOVSW
    MOVSW
    MOVSW
    MOVSW
    NOP
    MOVSW
    MOVSW
    MOVSW
    MOVSW
    MOVSW
    MOVSW
    MOV SP, [BX]
    MOV SI, DX
    SUB SP, BP
    MOV DI, SP
    PUSH AX
    MOV [BX], SP
    SUB SP, CX
    CALL FAR [BX]
    MOV SI, AX
    ADD SI, WORD 0x0014
    PUSH ES
    POP DS
    MOV AX, [BX]
    PUSH CS
    POP DS
    PUSH CS
    POP SS
    JMP label7
    NOP
label7:
    MOV DI, 0xEF83
    ADD BH, [BP+0x0114]
    MOV DX, SI
    CALL FAR [BX]
    MOVSW
    PUSH SI
    CALL FAR [BX+SI]
    SUB [BX], DX
    SUB SP, BP
    MOV DI, [BX]
    DEC DI
    MOVSB
    MOVSW
    MOVSW
    SUB DI, WORD 0x0003
    XOR SI, SI
    MOV CL, 0x0A
    CALL FAR [BX+SI]
    INT3
    INT3
    NOP
    LES SP, [BX]
    SUB SP, CX
    INT3
    CALL FAR [BX]