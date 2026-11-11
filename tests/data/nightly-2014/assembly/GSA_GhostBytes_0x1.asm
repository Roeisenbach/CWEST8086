    JMP label1
    PUSH AX
    XOR AX, AX
    XOR DX, DX
    PUSH CS
    MOV DI, 0x2102
    INT 0x86
    MOV AX, 0xA5A5
    MOV DX, 0xA5A5
    MOV BX, [BP+0x00]
    MOV CX, [BP+0x00]
    INT 0x87
    MOV DX, 0x1200
    MOV BP, 0x1400
    MOV CX, 0x000A
    NOP
    NOP
    MOV SI, AX
    ADD SI, WORD 0x0059
    XOR DI, DI
    REP MOVSW
    PUSH DS
    PUSH SS
    POP DS
    POP SS
    PUSH CS
    POP ES
    MOV AL, 0xA9
    MOV SP, AX
    ADD SP, 0x0200
    MOV BX, 0x0200
    MOV [BX], AX
    MOV [BX+0x02], CS
    MOV AX, 0x1FFF
    MOV DI, [BX]
    NOP
    NOP
    STOSW
    DEC DI
    MOV AX, 0xA5F3
    MOV CL, 0x0A
    XOR SI, SI
    CALL FAR [BX]
    ADD [BX], DX
    ADD SP, BP
    MOV CL, 0x0A
    MOV DI, [BX]
    MOV AX, 0x1FFF
    STOSW
    MOV AX, 0xA5F3
    XOR SI, SI
    DEC DI
    CALL FAR [BX]
    PUSH AX
    XOR AX, AX
    XOR DX, DX
    PUSH CS
    POP ES
    NOP
    MOV DI, 0x2102
    INT 0x86
    MOV AX, 0xA5A5
    MOV DX, 0xA5A5
    MOV BX, [BP+0x00]
    MOV CX, [BP+0x00]
    INT 0x87
    MOV DX, 0x1200
    MOV BP, 0x1400
    MOV CX, 0x000A
    PUSH SS
    POP AX
    MOV SI, AX
    ADD SI, WORD 0x0059
    XOR DI, DI
    NOP
    REP MOVSW
    PUSH DS
    PUSH SS
    POP DS
    POP SS
    MOV AL, 0xA9
    MOV SP, AX
    ADD SP, 0x0200
    MOV BX, 0x0200
    MOV [BX], AX
    MOV [BX+0x02], CS
    MOV DI, [BX]
    MOV AX, 0x1FFF
    STOSW
    DEC DI
    MOV AX, 0xA5F3
    MOV CL, 0x0A
    XOR SI, SI
    CALL FAR [BX]
    NOP
    ADD [BX], DX
    ADD SP, BP
    MOV CL, 0x0A
    MOV DI, [BX]
    MOV AX, 0x1FFF
    STOSW
    MOV AX, 0xA5F3
    XOR SI, SI
    DEC DI
    CALL FAR [BX]
label1:
    PUSH AX
    XOR AX, AX
    XOR DX, DX
    PUSH CS
    POP ES
    MOV DI, 0x2102
    INT 0x86
    MOV AX, 0xB8FF
    MOV DX, 0x1FBA
    NOP
    MOV BX, [BP+0x00]
    MOV CX, [BP+0x00]
    INT 0x87
    INT 0x87
    MOV DX, 0x1200
    MOV BP, 0x1400
    MOV CX, 0x000A
    POP AX
    MOV DI, AX
    ADD DI, 0x0222
    INT 0x86
    PUSH SS
    POP ES
    MOV SI, AX
    ADD SI, WORD 0x0059
    XOR DI, DI
    NOP
    REP MOVSW
    PUSH DS
    PUSH SS
    POP DS
    POP SS
    PUSH CS
    POP ES
    MOV AL, 0xA9
    MOV SP, AX
    ADD SP, 0x0200
    MOV BX, 0x0200
    MOV [BX], AX
    MOV [BX+0x02], CS
    MOV DI, [BX]
    MOV AX, 0x1FFF
    STOSW
    DEC DI
    MOV AX, 0xA5F3
    MOV CL, 0x0A
    XOR SI, SI
    NOP
    CALL FAR [BX]
    ADD [BX], DX
    ADD SP, BP
    MOV CL, 0x0A
    MOV DI, [BX]
    MOV AX, 0x1FFF
    STOSW
    MOV AX, 0xA5F3
    XOR SI, SI
    CALL FAR [BX]
    JMP 548
    PUSH AX
    XOR AX, AX
    XOR DX, DX
    PUSH CS
    MOV DI, 0x2102
    INT 0x86
    MOV AX, 0xA5A5
    NOP
    MOV DX, 0xA5A5
    MOV BX, [BP+0x00]
    MOV CX, [BP+0x00]
    INT 0x87
    MOV DX, 0x1200
    MOV BP, 0x1400
    MOV CX, 0x000A
    PUSH SS
    POP ES
    POP AX
    MOV SI, AX
    ADD SI, WORD 0x0059
    XOR DI, DI
    REP MOVSW
    PUSH DS
    PUSH SS
    POP DS
    POP SS
    PUSH CS
    NOP
    MOV AL, 0xA9
    MOV SP, AX
    ADD SP, 0x0200
    MOV BX, 0x0200
    MOV [BX], AX
    MOV [BX+0x02], CS
    MOV DI, [BX]
    MOV AX, 0x1FFF
    STOSW
    DEC DI
    MOV AX, 0xA5F3
    MOV CL, 0x0A
    XOR SI, SI
    CALL FAR [BX]
    ADD [BX], DX
    ADD SP, BP
    MOV CL, 0x0A
    NOP
    MOV DI, [BX]
    MOV AX, 0x1FFF
    STOSW
    MOV AX, 0xA5F3
    XOR SI, SI
    CALL FAR [BX]
    PUSH AX
    XOR AX, AX
    XOR DX, DX
    PUSH CS
    MOV DI, 0x2102
    INT 0x86
    MOV AX, 0xA5A5
    MOV DX, 0xA5A5
    MOV BX, [BP+0x00]
    MOV CX, [BP+0x00]
    INT 0x87
    NOP
    MOV DX, 0x1200
    MOV BP, 0x1400
    MOV CX, 0x000A
    PUSH SS
    POP AX
    MOV SI, AX
    ADD SI, WORD 0x0059
    XOR DI, DI
    REP MOVSW
    PUSH DS
    POP DS
    POP SS
    PUSH CS
    MOV AL, 0xA9
    MOV SP, AX
    ADD SP, 0x0200
    MOV BX, 0x0200
    MOV [BX], AX
    NOP
    MOV [BX+0x02], CS
    MOV DI, [BX]