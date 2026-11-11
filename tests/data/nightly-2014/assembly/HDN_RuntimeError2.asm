    JMP label3
label1:
    JMP label2
    MOVSW
    MOVSW
    MOVSW
    SUB [BX], BP
    MOVSW
    MOVSW
    MOVSW
    SUB SP, BP
    MOVSW
    MOVSW
    MOVSW
    XOR SI, SI
    MOV DI, [BX]
    STOSW
    CALL FAR [BX]
label2:
    PUSH AX
    CLD
    MOV CX, 0x0014
    PUSH ES
    PUSH CS
    PUSH ES
    PUSH CS
    POP DS
    POP ES
    MOV DI, SI
    MOV SI, AX
    ADD SI, WORD 0x0004
    SHR CX, 1
    REP MOVSW
    POP ES
    POP DS
    SUB DI, 0x0014
    MOV SI, DI
    MOV DI, AX
    ADD DI, WORD 0x0056
    MOV BX, 0x0100
    MOV [BX], SI
    MOV [BX+0x02], CS
    PUSH CS
    POP SS
    MOV SP, AX
    MOVSW
    XOR CX, CX
    MOV BP, AX
    MOV AX, 0xA5A5
    MOV CL, AL
    JMP label3
    MOVSW
    MOVSW
label3:
    PUSH AX
    ADD AX, 0x00AE
    MOV BX, 0x2000
    MOV [BX], AX
    MOV [BX+0x02], CS
    MOV BX, 0x0A52
    MOV WORD [BX], 0x00B8
    MOV WORD [BX+0x02], 0x1E10
    MOV WORD [BX+0x04], 0xD88E
    MOV WORD [BX+0x06], 0xC88E
    SUB AX, 0x00AE
    MOV BX, 0x000A
    MOV WORD [BX], 0x26FF
    MOV WORD [BX+0x02], 0x2000
    MOV DI, AX
    MOV AX, 0xCCCC
    PUSH ES
    PUSH CS
    POP ES
    STD
    MOV CX, 0x0032
    REP STOSW
    STD
    MOV BX, 0xDB31
    MOV CX, 0xE6FF
    MOV AX, 0xCCCC
    MOV DX, 0x1FFF
    INT 0x87
    POP ES
    POP AX
    XOR SI, SI
    XOR DI, DI
    CLD
    JMP label1
    PUSH CS
    POP DS
    POP ES
    MOV CX, 0x0400
    XOR DI, DI
    CLD
    REP MOVSW
    STD
    MOV AX, SS
    CMP AX, 0x1000
    JZ label4
    PUSH SS
    PUSH DS
    POP ES
    POP DS
    MOV CX, 0x0800
    MOV DI, CX
    REP MOVSW
label4:
    INT3
    INT3
    INT3
    INT3
    INT3
    INT3
    INT3
    INT3