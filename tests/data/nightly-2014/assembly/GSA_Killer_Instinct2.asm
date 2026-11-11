    JMP label1
    INC DI
    STOSW
    MOV CL, 0x11
    CALL FAR [BX]
label1:
    MOV SI, AX
    XOR AX, AX
    XOR DX, DX
    MOV DI, 0x20A0
    PUSH ES
    PUSH DS
    POP ES
    INT 0x86
    INT 0x86
    POP ES
    MOV [SI+0x00D4], SS
    XOR DI, DI
    MOV AX, SI
    ADD AX, 0x00A9
    SUB AH, 0x01
    PUSH DS
    PUSH ES
    POP DS
    MOV [0x0000], AX
    POP DS
    PUSH ES
    PUSH CS
    POP ES
    MOV CX, 0x0004
    MOV AX, 0xCCCC
label2:
    STOSW
    DEC CX
    JCXZ loop1
    JMP label2
loop1:
    STOSW
    STOSW
    STOSW
    MOV WORD [0xDFE7], 0x00B8
    MOV [0xDFE8], SI
    ADD WORD [0xDFE8], 0x00D6
    MOV WORD [0xDFEA], 0xE0FF
    STOSW
    STOSW
    STOSW
    STOSW
    STOSW
    STOSW
    STOSW
    STOSW
    STOSW
    STOSW
    STOSW
    STOSW
    STOSW
    STOSW
    STOSW
    STOSW
    STOSW
    STOSW
    STOSW
    STOSW
    STOSW
    STOSW
    MOV AX, 0xCCCC
    MOV DX, 0xDC29
    INT 0x87
    XOR DI, DI
    POP ES
    PUSH ES
    PUSH SS
    POP ES
    MOV DX, SP
    ADD SI, 0x00B3
    MOV CL, 0x0D
    MOV CX, 0x0011
    REP MOVSW
    MOV SP, DX
    POP ES
    MOV DX, AX
    MOV DL, 0xA3
    MOV AX, 0x1FFF
    PUSH SS
    PUSH ES
    PUSH DS
    POP ES
    MOV DI, DX
    STOSW
    POP ES
    POP DS
    MOV BX, 0x0050
    MOV [BX+0x02], CS
    MOV [BX], DX
    PUSH CS
    POP ES
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
    ADD [BX+SI], AL
    MOV SI, AX
    DB 0x83, 0xC0, 0x64 ;$1 ADD AX, 0x0064
    PUSH SS
    POP BX
    XCHG BX, AX
    SUB AX, [SI-0x02]
    MOV CX, 0x000A
    MUL CX
    MOV BX, AX
    PUSH ES
    PUSH DS
    POP ES
    PUSH AX
    MOV DI, AX
    MOV AX, 0xCCCC
    MOV DX, AX
    INT 0x86
    INT 0x86
    XOR DI, DI
    POP AX
    POP ES
    PUSH ES
    PUSH SS
    POP ES
    MOV DX, SP
    ADD SI, 0x0066
    MOV CL, 0x0D
    MOV CX, 0x0011
    REP MOVSW
    MOV SP, DX
    POP ES
    SUB BH, 0x36
    MOV BL, 0xA3
    MOV AX, 0x1FFF
    PUSH SS
    PUSH ES
    PUSH DS
    POP ES
    MOV DI, BX
    STOSW
    POP ES
    POP DS
    MOV DX, BX
    MOV BX, 0x0050
    MOV [BX+0x02], CS
    MOV [BX], DX
    PUSH CS
    POP ES
    PUSH CS
    POP SS
    ADD DX, 0x0300
    MOV SP, DX
    DEC DI
    XOR SI, SI
    MOV CL, 0x11
    MOV AX, 0x0240
    CALL FAR [BX]
    REP MOVSW
    SUB SP, 0x2E00
    MOV CX, AX
    MOV AX, 0x1FFF
    SUB DI, 0x3125
    MOV DX, DI
    MOV [BX], DI
    STOSW
label3:
    MOV AX, CX
    XOR SI, SI
    DEC AX
    JZ label3+1
    MOV CX, 0x0011
    DEC DI
    JMP DX
    INC DI
    STOSW
    MOV CL, 0x11
    CALL FAR [BX]