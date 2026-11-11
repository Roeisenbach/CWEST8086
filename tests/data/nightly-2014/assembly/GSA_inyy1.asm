    JMP label1
    MOV BX, AX
    SHR DL, 1
    SUB DL, 0x03
    MOV BP, DX
    DEC DL
    SUB BP, 0x0022
    MOV [BX+0x008A], DL
    MOV [BX+0x53], DL
    INC DL
    INC DL
    MOV [BX+0x56], DL
    DEC DL
    DEC DL
    INC CX
    SHR CX, 1
    MOV SI, AX
    ADD SI, 0x006F
    REP MOVSW
    PUSH ES
    PUSH DS
    POP ES
    POP DS
    PUSH CS
    POP SS
    MOV BX, 0x0064
    SUB BP, 0x0100
    MOV AX, BP
    MOV CX, 0x0622
    ADD AX, CX
    DB 0x83, 0xE8, 0x04 ;$1 SUB AX, 0x0004
    MOV [BX+0x04], CS
    MOV [BX+0x02], AX
    MOV [BX+0x08], BP
    MOV [BX+0x08], CS
    MOV DI, BP
    INC DI
    MOV SI, 0x000C
    MOV DX, 0x0600
    MOV SP, BP
    ADD SP, 0x0624
    MOV AX, 0x0004
    MOV BYTE [BP+0x00], 0xA5
    JMP BP
    MOVSW
    MOVSW
    MOVSW
    MOVSW
    SUB DI, CX
    MOVSW
    CALL FAR [BX+0x02]
    NOP
    MOVSW
    MOVSW
    ADD SP, AX
    SUB [BX+0x02], DX
    MOVSW
    MOVSW
    MOVSW
    SUB [BX+0x08], DX
    MOVSW
    MOVSW
    MOV SI, 0x0000
    MOVSW
label1:
    ADD AX, 0x0086
    MOV DX, AX
    MOV BX, AX
    SHR DL, 1
    SUB DL, 0x03
    MOV BP, DX
    DEC DL
    SUB BP, WORD 0x0022
    MOV [BX+0x008A], DL
    MOV [BX+0x53], DL
    INC DL
    INC DL
    MOV [BX+0x56], DL
    DEC DL
    DEC DL
    MOV CX, 0x0022
    INC CX
    SHR CX, 1
    MOV SI, AX
    ADD SI, WORD 0x006F
    REP MOVSW
    PUSH ES
    PUSH DS
    POP ES
    POP DS
    PUSH CS
    POP SS
    MOV BX, 0x0064
    SUB BP, 0x0100
    MOV AX, BP
    MOV CX, 0x0622
    ADD AX, CX
    DB 0x83, 0xE8, 0x04 ;$1 SUB AX, 0x0004
    MOV [BX+0x04], CS
    MOV [BX+0x02], AX
    MOV [BX+0x08], BP
    MOV [BX+0x08], CS
    MOV DI, BP
    INC DI
    MOV SI, 0x000C
    MOV DX, 0x0600
    MOV SP, BP
    ADD SP, 0x0624
    MOV AX, 0x0004
    MOV BYTE [BP+0x00], 0xA5
    JMP BP
    MOVSW
    MOVSW
    MOVSW
    MOVSW
    SUB DI, CX
    MOVSW
    CALL FAR [BX+0x02]
    NOP
    MOVSW
    MOVSW
    ADD SP, AX
    MOVSW
    MOVSW
    MOVSW
    MOVSW
    SUB [BX+0x02], DX
    MOVSW
    MOVSW
    MOVSW
    SUB [BX+0x08], DX
    MOVSW
    MOVSW
    MOV SI, 0x0000
    MOVSW
    MOV BX, AX
    SHR DL, 1
    SUB DL, 0x03
    MOV BP, DX
    DEC DL
    SUB BP, 0x0022
    MOV [BX+0x008A], DL
    MOV [BX+0x53], DL
    INC DL
    INC DL
    MOV [BX+0x56], DL
    DEC DL
    DEC DL
    INC CX
    SHR CX, 1
    MOV SI, AX
    ADD SI, 0x006F
    REP MOVSW
    PUSH ES
    PUSH DS
    POP ES
    POP DS
    PUSH CS
    POP SS
    MOV BX, 0x0064
    SUB BP, 0x0100
    MOV AX, BP
    MOV CX, 0x0622
    ADD AX, CX
    DB 0x83, 0xE8, 0x04 ;$1 SUB AX, 0x0004
    MOV [BX+0x04], CS
    MOV [BX+0x02], AX
    MOV [BX+0x08], BP
    MOV [BX+0x08], CS
    MOV DI, BP
    INC DI
    MOV SI, 0x000C
    MOV DX, 0x0600
    MOV SP, BP
    ADD SP, 0x0624
    MOV AX, 0x0004
    MOV BYTE [BP+0x00], 0xA5
    JMP BP
    SUB DI, CX
    MOVSW
    INT 0x87
    CALL FAR [BX+0x02]