    NOP
    JMP label1
    ADD AX, 0x012B
    INC AX
    STOSW
    PUSH ES
    PUSH DS
    POP ES
    MOV SI, AX
    MOV BX, AX
    POP ES
    XOR DI, DI
    MOV AX, 0x29A5
    MOV DX, 0xABEC
    INT 0x86
    MOV CL, 0x0A
    REP MOVSW
    MOV DI, BX
    PUSH CS
    POP SS
    PUSH DS
    PUSH ES
    POP DS
    NOP
    POP ES
    MOV SP, BX
    MOV AX, 0xA552
    MOV BP, 0x36F0
    MOV SI, 0x0000
    MOV CX, 0x0772
    MOVSW
    MOVSW
    MOVSW
    MOVSW
    MOVSW
    MOVSW
    MOVSW
    MOVSW
    SUB DI, CX
    MOV SP, DI
    MOVSW
    JMP SP
    MOVSW
    MOVSW
    MOVSW
    XOR SI, SI
    MOVSW
    NOP
    NOP
    ADD AX, 0x0034
    STOSW
    PUSH ES
    PUSH DS
    POP ES
    MOV SI, AX
    MOV BX, AX
    POP ES
    XOR DI, DI
    MOV AX, 0x29A5
    MOV DX, 0xABEC
    INT 0x86
    MOV CL, 0x0A
    REP MOVSW
    MOV DI, BX
    PUSH CS
    POP SS
    PUSH DS
    PUSH ES
    POP DS
    POP ES
    MOV SP, BX
    NOP
    NOP
    MOV AX, 0xA552
    MOV BP, 0x3671
    MOV SI, 0x0000
    MOV CX, 0x0772
    MOVSW
    MOVSW
    MOVSW
    MOVSW
    CALL -23018
    MOVSW
    MOVSW
    SUB DI, CX
    MOV SP, DI
    MOVSW
    JMP SP
    MOVSW
    MOVSW
    MOVSW
    ADD AX, 0x0034
    STOSW
    PUSH ES
    PUSH DS
    NOP
    POP ES
    MOV SI, AX
    MOV BX, AX
    POP ES
    XOR DI, DI
    MOV AX, 0x29A5
    MOV DX, 0xABEC
    INT 0x86
    MOV CL, 0x0A
    REP MOVSW
    MOV DI, BX
    PUSH CS
    POP SS
    PUSH DS
    PUSH ES
    POP DS
    POP ES
    MOV SP, BX
    MOV AX, 0xA552
    MOV BP, 0x36F0
    NOP
    NOP
    MOV SI, 0x0000
    MOV CX, 0x0772
    MOVSW
    MOVSW
    MOVSW
    MOVSW
    MOVSW
    MOVSW
    MOVSW
    SUB DI, CX
    MOV SP, DI
    MOVSW
    JMP SP
    MOVSW
    MOVSW
    MOVSW
    XOR SI, SI
    MOVSW
label1:
    ADD AX, 0x013D
    MOV DI, 0x03FE
    STOSW
    MOV DX, [0x1243]
    NOP
    XCHG SI, AX
    MOV CX, 0x10E1
    DIV CX
    MOV DI, AX
    ADD DI, WORD 0x0021
    MOV AX, 0xCCCC
    MOV CX, 0x000A
    PUSH ES
    PUSH CS
    POP ES
    REP STOSW
    POP ES
    XOR DI, DI
    MOV AX, 0x29A5
    MOV DX, 0xABEC
    INT 0x86
    MOV CL, 0x0A
    NOP
    NOP
    REP MOVSW
    SUB SI, WORD 0x0014
    MOV DI, 0x0142
    STOSW
    PUSH DS
    PUSH ES
    POP DS
    POP ES
    MOV DI, 0xFFFF
    MOV AX, 0x18FF
    MOV DX, 0xCCCC
    MOV BX, DX
    MOV CX, DX
    STD
    INT 0x87
    CLD
    PUSH CS
    POP SS
    MOV DI, SI
    MOV SP, DI
    NOP
    MOV AX, 0xA552
    MOV BP, 0x36F0
    XOR SI, SI
    MOV CX, 0x0772
    MOVSW
    MOVSW
    MOVSW
    MOVSW
    MOVSW
    MOVSW
    MOVSW
    MOVSW
    SUB DI, CX
    MOV SP, DI
    MOVSW
    JMP SP
    MOVSW
    MOVSW
    MOVSW
    XOR SI, SI
    MOVSW
    ADD AX, 0x0034
    STOSW
    PUSH ES
    NOP
    PUSH DS
    POP ES
    MOV SI, AX
    MOV BX, AX
    POP ES
    XOR DI, DI
    MOV AX, 0x29A5
    MOV DX, 0xABEC
    INT 0x86
    MOV CL, 0x0A
    REP MOVSW
    MOV DI, BX
    PUSH CS
    POP SS
    PUSH DS
    PUSH ES
    POP DS
    POP ES
    MOV SP, BX
    MOV AX, 0xA552
    MOV BP, 0x36F0
    NOP
    MOV SI, 0x0100
    ADD AX, 0x3F8B
    LEA SP, [BP+DI]
    MOVSW
    MOVSW
    MOVSB
    XOR SI, SI
    MOV CL, 0x09
    CALL FAR [BX]
    SUB SP, AX
    CALL FAR [BX]
    MOVSW
    INT3
    INT3
    INT3
    INT3
    SUB DI, CX
    MOV SP, DI
    MOVSW
    JMP SP
    MOVSW
    MOVSW
    MOVSW
    XOR [BX+SI-0x5A0A], DX
    NOP
    NOP
    NOP
    ADD AX, 0x0122
    STOSW
    PUSH ES
    PUSH DS
    POP ES
    MOV SI, AX
    MOV BX, AX
    POP ES
    XOR DI, DI
    MOV AX, 0x29A5
    MOV DX, 0xABEC
    INT 0x86
    MOV CL, 0x0A
    REP MOVSW
    MOV DI, 0x0142
    STOSW
    MOV DI, BX
    NOP
    PUSH CS
    POP SS
    PUSH DS
    PUSH ES
    POP DS
    POP ES
    MOV SP, BX
    MOV AX, 0xA552
    MOV BP, 0x36F0
    NOP
    MOV SI, 0x0000
    MOV CX, 0x0772
    MOVSW
    MOVSW
    MOVSW
    MOVSW
    MOVSW
    MOVSW
    MOVSW
    MOVSW
    SUB DI, CX
    MOV SP, DI
    ESC 0xDA
    AND DX, 0x9010
    MOVSW
    MOVSW
    XOR SI, SI
    MOV AX, 0x29A5