    JMP label3
    INT3
    INT3
    INT3
    STOSW
    MOV SI, AX
    ADD SI, WORD 0x0044
    MOV CX, 0x000E
    REP MOVSW
    PUSH DS
    PUSH ES
    POP DS
    POP ES
    PUSH AX
    MOV AX, 0xCCCC
    MOV DX, 0xCCCC
    INT 0x87
    POP AX
    MOV DI, AX
    ADD DI, 0x0A94
    MOV BX, 0x0020
    MOV [BX], AX
    MOV WORD [BX+0x02], 0x10A5
    PUSH CS
    POP SS
    MOV SP, AX
    ADD SP, 0x0A52
    MOV SI, 0x0004
    MOV CX, 0x000C
    REP MOVSW
    MOV AX, 0xF596
    MOV DX, 0x10A5
    MOV ES, DX
    MOV CS, DX
    REP MOVSW
label1:
    ADD WORD [BX], 0x1900
    ADD SP, 0x1B00
    CMP [BX], AX
    JA label1
    MOV DI, [BX]
    MOVSW
    DEC DI
    MOV SI, 0x0002
    MOV CX, 0x000C
    CALL FAR [BX]
    CALL FAR [BX]
    INT3
    INT3
    INT3
    STOSW
    MOV SI, AX
    ADD SI, WORD 0x0044
    MOV CX, 0x000E
    REP MOVSW
    PUSH DS
    PUSH ES
    POP DS
    POP ES
    PUSH AX
    MOV AX, 0xCCCC
    MOV DX, 0xCCCC
    INT 0x87
    POP AX
    MOV DI, AX
    ADD DI, 0x0A94
    MOV BX, 0x0020
    MOV [BX], AX
    MOV WORD [BX+0x02], 0x10A5
    PUSH CS
    POP SS
    MOV SP, AX
    ADD SP, 0x0A52
    MOV SI, 0x0004
    MOV CX, 0x000C
    REP MOVSW
    MOV AX, 0xF596
    MOV DX, 0x10A5
    MOV ES, DX
    MOV CS, DX
    REP MOVSW
label2:
    ADD WORD [BX], 0x1900
    ADD SP, 0x1B00
    CMP [BX], AX
    JA label2
    MOV DI, [BX]
    MOVSW
    DEC DI
    MOV SI, 0x0002
    MOV CX, 0x000C
    CALL FAR [BX]
    CALL FAR [BX]
    INT3
    INT3
    INT3
label3:
    STOSW
    MOV SI, AX
    ADD SI, 0x0110
    MOV CX, 0x000E
    REP MOVSW
    PUSH DS
    PUSH ES
    POP DS
    POP ES
    PUSH AX
    MOV AX, 0xCCCC
    MOV DX, 0xCCCC
    INT 0x87
    POP AX
    MOV DI, AX
    ADD DI, 0x0B60
    MOV BX, 0x0020
    MOV [BX], AX
    MOV WORD [BX+0x02], 0x10A5
    PUSH CS
    POP SS
    MOV SP, AX
    ADD SP, 0x0A52
    MOV SI, 0x0004
    MOV CX, 0x000C
    REP MOVSW
    MOV AX, 0xF596
    MOV DX, 0x10A5
    MOV ES, DX
    MOV CS, DX
    REP MOVSW
label4:
    ADD WORD [BX], 0x1900
    ADD SP, 0x1B00
    CMP [BX], AX
    JA label4
    MOV DI, [BX]
    MOVSW
    DEC DI
    MOV SI, 0x0002
    MOV CX, 0x000C
    CALL FAR [BX]
    CALL FAR [BX]
    INT3
    INT3
    INT3
    STOSW
    MOV SI, AX
    ADD SI, WORD 0x0044
    MOV CX, 0x000E
    REP MOVSW
    PUSH DS
    PUSH ES
    POP DS
    POP ES
    PUSH AX
    MOV AX, 0xCCCC
    MOV DX, 0xCCCC
    INT 0x87
    POP AX
    MOV DI, AX
    ADD DI, 0x0A94
    MOV BX, 0x0020
    MOV [BX], AX
    MOV WORD [BX+0x02], 0x10A5
    PUSH CS
    POP SS
    MOV SP, AX
    ADD SP, 0x0A52
    MOV SI, 0x0004
    MOV CX, 0x000C
    REP MOVSW
    MOV AX, 0xF596
    MOV DX, 0x10A5
    MOV ES, DX
    MOV CS, DX
    REP MOVSW
label5:
    ADD WORD [BX], 0x1900
    ADD SP, 0x1B00
    CMP [BX], AX
    JA label5
    MOV DI, [BX]
    MOVSW
    DEC DI
    MOV SI, 0x0002
    MOV CX, 0x000C
    CALL FAR [BX]
    CALL FAR [BX]
    INT3
    INT3
    INT3
    STOSW
    MOV SI, AX
    ADD SI, WORD 0x0044
    MOV CX, 0x000E
    REP MOVSW
    PUSH DS
    PUSH ES
    POP DS
    POP ES
    PUSH AX
    MOV AX, 0xCCCC
    MOV DX, 0xCCCC
    INT 0x87
    POP AX
    MOV DI, AX
    ADD DI, 0x0A94
    MOV BX, 0x0020
    MOV [BX], AX
    MOV WORD [BX+0x02], 0x10A5
    PUSH CS
    POP SS
    MOV SP, AX
    ADD SP, 0x0A52
    MOV SI, 0x0004
    MOV CX, 0x000C
    REP MOVSW
    MOV AX, 0xF596
    MOV DX, 0x10A5
    MOV ES, DX
    MOV CS, DX
    REP MOVSW
label6:
    ADD WORD [BX], 0x1900
    ADD SP, 0x1B00
    CMP [BX], AX
    JA label6
    MOV DI, [BX]
    MOVSW
    DEC DI
    MOV SI, 0x0002
    MOV CX, 0x000C
    CALL FAR [BX]
    CALL FAR [BX]
    INT3
    INT3
    INT3