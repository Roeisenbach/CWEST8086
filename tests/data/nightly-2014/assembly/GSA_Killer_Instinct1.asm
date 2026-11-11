    MOV SI, AX
    NOP
    NOP
    NOP
    XCHG CX, AX
    MOV DI, 0x20A0
    PUSH ES
    PUSH DS
    POP ES
    INT 0x86
    INT 0x86
    POP ES
    XCHG CX, AX
    ADD AX, 0x0097
    MOV DI, AX
    MOV AX, 0xCCCC
    PUSH ES
    PUSH DS
    POP ES
    MOV DX, 0xCCCC
    MOV CX, 0x000B
label1:
    STOSW
    DEC CX
    JCXZ loop1
    JMP label1
loop1:
    STOSW
    STOSW
    STOSW
    STOSW
    POP ES
    XOR DI, DI
    PUSH ES
    PUSH SS
    POP ES
    MOV DX, SP
    ADD SI, WORD 0x0076
    MOV CL, 0x0D
    MOV CX, 0x0011
    REP MOVSW
    MOV SP, DX
    POP ES
    PUSH DS
    PUSH ES
    POP DS
    MOV DX, [0x0000]
    ADD DX, 0x0500
    MOV DL, 0xA3
    POP DS
    PUSH DS
    PUSH SS
    POP DS
    MOV BX, 0x0040
    MOV [BX+0x02], CS
    MOV [BX], DX
    POP DS
    MOV AX, 0x1FFF
    PUSH SS
    PUSH DS
    POP ES
    MOV DI, DX
    STOSW
    POP DS
    PUSH CS
    POP SS
    ADD DX, 0x0300
    MOV SP, DX
    DEC DI
    XOR SI, SI
    MOV CL, 0x11
    CALL FAR [BX]
    REP MOVSW
    SUB SP, 0x2E00
    MOV CL, 0x11
    MOV AX, 0x1FFF
    SUB DI, 0x3125
    MOV DX, DI
    MOV [BX], DI
    STOSW
    XOR SI, SI
    DEC DI
    MOV DI, DI
    MOV DI, DI
    MOV DI, DI
    MOV DI, DI
    JMP DX