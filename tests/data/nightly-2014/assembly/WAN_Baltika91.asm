    JMP label1
    ADD SI, 0x0167
    MOV CL, 0x09
    REP MOVSW
    MOV BX, 0x0040
    MOV BP, 0x6200
    MOV DX, 0x5F00
    PUSH ES
    POP DS
    ADD AX, 0x01D9
    MOV [BX], AX
    MOV WORD [BX+0x02], 0x0FF6
    CALL FAR [BX]
    PUSH CS
    POP ES
    POP CX
    POP SS
    NOP
    NOP
    MOV AL, 0xA3
    MOV [BX], AX
    MOV SP, AX
    ADD SP, 0x0300
    MOV DI, AX
    MOV CX, 0x0007
    MOV SI, 0x0010
    MOVSW
    DEC DI
    XOR SI, SI
    JMP WORD [BX]
    REP MOVSW
    SUB [BX], BP
    SUB SP, DX
    MOV DI, [BX]
    MOVSW
    DEC DI
    XOR SI, SI
    MOV CL, 0x07
    JMP WORD [BX]
    NOP
label1:
    MOV SI, AX
    LEA BP, [SI+0x008B]
    MOV DX, [0x1243]
    MOV AX, 0x0000
    PUSH ES
    PUSH CS
    POP ES
    MOV BX, 0x10E1
    DIV BX
    MOV DI, AX
    MOV [0x0000], BP
    MOV AX, 0x24FF
    MOV DX, 0x24FF
    INT 0x86
    MOV DI, 0x0000
    NOP
    POP ES
    LEA AX, [SI+0x00DB]
    STOSW
    XOR DI, DI
    SUB AX, 0x00DB
    MOV WORD [0x0000], 0xCCCC
    JMP label3
    MOV AX, [SI]
    MOV SI, AX
    MOV BP, 0x3100
    MOV DX, 0x2E00
    CMP CX, 0x0000
    JNZ label2
    ADD SI, 0x00DC
    MOV CL, 0x09
    XOR DI, DI
    PUSH SS
    NOP
    POP ES
    REP MOVSW
    MOV BX, 0x0050
    PUSH ES
    POP DS
    DB 0x83, 0xC0, 0x5E ;$1 ADD AX, 0x005E
    MOV [BX], AX
    MOV WORD [BX+0x02], 0x1005
    MOV AX, 0x0000
    ADD AH, 0x06
    CALL FAR [BX]
label2:
    ADD SI, 0x00DC
    MOV CL, 0x09
    REP MOVSW
    MOV BX, 0x0040
    PUSH ES
    NOP
    POP DS
    DB 0x83, 0xC0, 0x7E ;$1 ADD AX, 0x007E
    MOV [BX], AX
    MOV WORD [BX+0x02], 0x1003
    ADD AH, 0x06
    CALL FAR [BX]
    MOV BX, [BP+0x00]
    MOV [BX-0x24], SI
    MOV AX, BX
    MOV BX, 0x0050
    MOV BP, 0x6200
    MOV DX, 0x5F00
    PUSH SS
    POP DS
    ADD AX, 0x00FE
    NOP
    MOV [BX], AX
    MOV WORD [BX+0x02], 0x0FF6
    MOV AX, SI
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
    NOP
    NOP
    NOP
    NOP
label3:
    ADD SI, 0x0167
    MOV CL, 0x09
    REP MOVSW
    MOV BX, 0x0040
    MOV BP, 0x6200
    MOV DX, 0x5F00
    PUSH ES
    POP DS
    ADD AX, 0x01D9
    MOV [BX], AX
    MOV WORD [BX+0x02], 0x0FF6
    CALL FAR [BX]
    PUSH CS
    POP ES
    POP CX
    POP SS
    MOV AL, 0xA3
    NOP
    MOV [BX], AX
    MOV SP, AX
    ADD SP, 0x0300
    MOV DI, AX
    MOV CX, 0x0007
    MOV SI, 0x0010
    MOVSW
    DEC DI
    XOR SI, SI
    JMP WORD [BX]
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
    SUB [BX], BP
    SUB SP, DX
    MOV DI, [BX]
    MOVSW
    DEC DI
    XOR SI, SI
    MOV CL, 0x07
    JMP WORD [BX]
    CALL FAR [BX]
    ADD SI, 0x0167
    MOV CL, 0x09
    REP MOVSW
    MOV BX, 0x0040
    MOV BP, 0x6200
    MOV DX, 0x5F00
    PUSH ES
    POP DS
    NOP
    NOP
    ADD AX, 0x01D9
    MOV [BX], AX
    MOV WORD [BX+0x02], 0x0FF6
    CALL FAR [BX]
    PUSH CS
    POP ES
    POP CX
    POP SS
    MOV AL, 0xA3
    MOV [BX], AX
    MOV SP, AX
    ADD SP, 0x0300
    MOV DI, AX
    MOV CX, 0x0007
    MOV SI, 0x0010
    MOVSW
    DEC DI
    XOR SI, SI
    NOP
    JMP WORD [BX]
    REP MOVSW
    SUB [BX], BP
    SUB SP, DX
    MOV DI, [BX]
    MOVSW
    DEC DI
    XOR SI, SI
    MOV CL, 0x07
    JMP WORD [BX]