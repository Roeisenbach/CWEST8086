    JMP label1
    INT3
    INT3
    INT3
    INT3
    CALL FAR [BX]
    CALL FAR [BX]
    CALL FAR [BX]
    CALL FAR [BX]
    CALL FAR [BX]
    CALL FAR [BX]
    CALL FAR [BX]
    ADD DI, 0x0004
    MOV SI, AX
    ADD SI, 0x00D6
    MOV CL, 0x13
    REP MOVSW
    PUSH ES
    POP DS
    PUSH AX
    MOV AX, 0xCCCC
    XOR DI, DI
    MOV DX, 0xCCCC
    PUSH CS
    INT 0x86
    POP ES
    INT 0x86
    POP AX
    MOV DI, AX
    ADD DI, AX
    AND DI, 0xFF00
    OR DI, 0x00A3
    MOV [0x0000], DI
    MOV [0x0002], CS
    MOV SI, 0x0027
    MOVSW
    PUSH CS
    POP SS
    MOV SP, DI
    ADD SP, 0x020E
    MOV SI, 0x0004
    MOV CL, 0x13
    DEC DI
    CALL FAR [BX]
    REP MOVSW
    MOV SI, 0x0027
    INC AX
    ADD DI, AX
    AND DI, 0xFF00
    OR DI, 0x00A3
    MOV [0x0000], DI
    MOVSW
    MOV SP, DI
    ADD SP, 0x020E
    MOV SI, 0x0004
    MOV CL, 0x13
    DEC DI
    CALL FAR [BX]
label1:
    ADD DI, 0x0004
    MOV SI, AX
    ADD SI, 0x00D6
    MOV CL, 0x13
    REP MOVSW
    PUSH ES
    PUSH CS
    POP ES
    PUSH AX
    MOV AX, 0xCCCC
    MOV DI, 0x0003
    MOV DX, 0xCCCC
    INT 0x86
    DB 0x9B, 0x9B ;$1 NRG
    INT 0x86
    MOV AX, 0x86CD
    MOV DX, 0x86CD
    MOV BL, 0xCC
    MOV CL, 0xCC
    INT 0x87
    XOR BL, BL
    POP AX
    POP DS
    MOV DI, AX
    ADD DI, AX
    AND DI, 0xFF00
    OR DI, 0x00A3
    MOV [0x0000], DI
    MOV [0x0002], CS
    MOV SI, 0x0027
    MOVSW
    PUSH CS
    POP SS
    MOV SP, DI
    ADD SP, 0x020E
    MOV SI, 0x0004
    MOV CL, 0x13
    DEC DI
    CALL FAR [BX]
    REP MOVSW
    MOV SI, 0x0027
    ADD AH, 0x45
    ADD DI, AX
    AND DI, 0xFF00
    OR DI, 0x00A3
    MOV [0x0000], DI
    MOVSW
    MOV SP, DI
    ADD SP, 0x020E
    MOV SI, 0x0004
    MOV CL, 0x13
    DEC DI
    CALL FAR [BX]
    ADD DI, 0x0004
    MOV SI, AX
    ADD SI, 0x00D6
    MOV CL, 0x13
    REP MOVSW
    PUSH ES
    POP DS
    MOV DI, AX
    ADD DI, AX
    AND DI, 0xFF00
    OR DI, 0x00A3
    MOV [0x0000], DI
    MOV [0x0002], CS
    PUSH CS
    POP ES
    MOV SI, 0x0027
    MOVSW
    PUSH CS
    POP SS
    MOV SP, DI
    ADD SP, 0x020E
    MOV SI, 0x0004
    MOV CL, 0x13
    DEC DI
    CALL FAR [BX]
    REP MOVSW
    MOV SI, 0x0027
    INC AX
    ADD DI, AX
    AND DI, 0xFF00
    OR DI, 0x00A3
    MOV [0x0000], DI
    MOVSW
    MOV SP, DI
    ADD SP, 0x020E
    MOV SI, 0x0004
    MOV CL, 0x13
    DEC DI
    INT3
    INT3
    INT3
    CALL FAR [BX]
    CALL FAR [BX]
    CALL FAR [BX]
    CALL FAR [BX]
    CALL FAR [BX]
    CALL FAR [BX]
    CALL FAR [BX]
    CALL FAR [BX]