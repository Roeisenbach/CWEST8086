    PUSH AX
    MOV AX, 0xCCCC
    JMP label1
    INT3
    INT3
    INT3
    MOV CX, 0x000C
    MOV SI, AX
    ADD SI, WORD 0x002A
    REP MOVSW
    PUSH DS
    PUSH ES
    POP DS
    POP ES
    MOV BX, 0x0020
    MOV AL, 0xA3
    MOV [BX], AX
    MOV [BX+0x02], CS
    MOV CX, 0x000A
    MOV SI, 0x0002
    PUSH CS
    POP SS
    MOV SP, AX
    ADD SP, 0x0A00
    REP MOVSW
    SUB WORD [BX], 0x1000
    MOV DI, [BX]
    SUB SP, 0x0600
    MOVSW
    DEC DI
    XOR SI, SI
    MOV CX, 0x000A
    CALL FAR [BX]
    INT3
    CALL FAR [BX]
    INT3
    INT3
    INT3
    MOV CX, 0x000C
    MOV SI, AX
    ADD SI, WORD 0x002A
    REP MOVSW
    PUSH DS
    PUSH ES
    POP DS
    POP ES
    MOV BX, 0x0020
    MOV AL, 0xA3
    MOV [BX], AX
    MOV [BX+0x02], CS
    MOV CX, 0x000A
    MOV SI, 0x0002
    PUSH CS
    POP SS
    MOV SP, AX
    ADD SP, 0x0A00
    REP MOVSW
    SUB WORD [BX], 0x1000
    MOV DI, [BX]
    SUB SP, 0x0600
    MOVSW
    DEC DI
    XOR SI, SI
    MOV CX, 0x000A
    CALL FAR [BX]
    INT3
    CALL FAR [BX]
    INT3
    INT3
    INT3
label1:
    NOP
    POP AX
    MOV CX, 0x000C
    MOV SI, AX
    ADD SI, 0x00B7
    REP MOVSW
    PUSH DS
    PUSH ES
    POP DS
    POP ES
    MOV BX, 0x0020
    MOV AL, 0xA3
    MOV [BX], AX
    MOV [BX+0x02], CS
    MOV CX, 0x000A
    MOV SI, 0x0002
    PUSH CS
    POP SS
    MOV SP, AX
    ADD SP, 0x0A00
    REP MOVSW
    SUB WORD [BX], 0x1000
    MOV DI, [BX]
    SUB SP, 0x0600
    MOVSW
    DEC DI
    XOR SI, SI
    MOV CX, 0x000A
    CALL FAR [BX]
    INT3
    CALL FAR [BX]
    INT3
    INT3
    INT3
    MOV CX, 0x000C
    MOV SI, AX
    ADD SI, WORD 0x002A
    REP MOVSW
    PUSH DS
    PUSH ES
    POP DS
    POP ES
    MOV BX, 0x0020
    MOV AL, 0xA3
    MOV [BX], AX
    MOV [BX+0x02], CS
    MOV CX, 0x000A
    MOV SI, 0x0002
    PUSH CS
    POP SS
    MOV SP, AX
    ADD SP, 0x0A00
    REP MOVSW
    SUB WORD [BX], 0x1000
    MOV DI, [BX]
    SUB SP, 0x0600
    MOVSW
    DEC DI
    XOR SI, SI
    MOV CX, 0x000A
    CALL FAR [BX]
    INT3
    CALL FAR [BX]
    INT3
    INT3
    INT3
    MOV CX, 0x000C
    MOV SI, AX
    ADD SI, WORD 0x002A
    REP MOVSW
    PUSH DS
    PUSH ES
    POP DS
    POP ES
    MOV BX, 0x0020
    MOV AL, 0xA3
    MOV [BX], AX
    MOV [BX+0x02], CS
    MOV CX, 0x000A
    MOV SI, 0x0002
    PUSH CS
    POP SS
    MOV SP, AX
    ADD SP, 0x0A00
    REP MOVSW
    SUB WORD [BX], 0x1000
    MOV DI, [BX]
    SUB SP, 0x0600
    MOVSW
    DEC DI
    XOR SI, SI
    MOV CX, 0x000A
    CALL FAR [BX]
    INT3
    CALL FAR [BX]