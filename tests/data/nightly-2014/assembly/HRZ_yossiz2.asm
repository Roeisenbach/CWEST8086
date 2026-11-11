    JMP label1
    ADD DI, 0x0094
    MOV SI, AX
    ADD SI, WORD 0x0057
    MOV CL, 0x4D
    REP MOVSW
    PUSH ES
    PUSH CS
    POP ES
    PUSH AX
    MOV AX, 0xCCCC
    XOR DI, DI
    MOV DX, 0xCCCC
    INT 0x86
    DB 0x9B, 0x9B ;$1 NRG
    INT 0x86
    MOV AX, 0x86CD
    MOV DX, 0x86CD
    MOV BL, 0xCC
    MOV CL, 0xCC
    INT 0x87
    POP AX
    POP DS
    MOV DI, AX
    ADD DI, AX
    AND DI, 0xFF00
    OR DI, 0x00A3
    MOV BL, 0x90
    MOV [BX], DI
    MOV [BX+0x02], CS
    MOV SI, 0x012B
    MOVSW
    PUSH CS
    POP SS
    MOV SP, DI
    ADD SP, 0x020E
    LEA SI, [BX+0x04]
    MOV CL, 0x4D
    DEC DI
    CALL FAR [BX]
    REP MOVSW
    MOV SI, 0x012B
    ADD AX, DI
    XOR DI, AX
    AND DI, 0xFF00
    OR DI, 0x00A3
    MOV [BX], DI
    MOVSW
    MOV SP, DI
    ADD SP, 0x020E
    LEA SI, [BX+0x04]
    MOV CL, 0x4D
    DEC DI
    CALL FAR [BX]
label1:
    ADD DI, 0x0094
    MOV SI, AX
    ADD SI, WORD 0x0057
    MOV CL, 0x4D
    REP MOVSW
    PUSH ES
    PUSH CS
    POP ES
    PUSH AX
    MOV AX, 0xCCCC
    XOR DI, DI
    MOV DX, 0xCCCC
    INT 0x86
    DB 0x9B, 0x9B ;$1 NRG
    INT 0x86
    MOV AX, 0x86CD
    MOV DX, 0x86CD
    MOV BL, 0xCC
    MOV CL, 0xCC
    INT 0x87
    POP AX
    POP DS
    MOV DI, AX
    ADD DI, AX
    AND DI, 0xFF00
    OR DI, 0x00A3
    MOV BL, 0x90
    MOV [BX], DI
    MOV [BX+0x02], CS
    MOV SI, 0x012B
    MOVSW
    PUSH CS
    POP SS
    MOV SP, DI
    ADD SP, 0x020E
    LEA SI, [BX+0x04]
    MOV CL, 0x4D
    DEC DI
    CALL FAR [BX]
    REP MOVSW
    MOV SI, 0x012B
    ADD AX, DI
    XOR DI, AX
    AND DI, 0xFF00
    OR DI, 0x00A3
    MOV [BX], DI
    MOVSW
    MOV SP, DI
    ADD SP, 0x020E
    LEA SI, [BX+0x04]
    MOV CL, 0x4D
    DEC DI
    CALL FAR [BX]
    ADD DI, 0x0094
    MOV SI, AX
    ADD SI, WORD 0x0057
    MOV CL, 0x4D
    REP MOVSW
    PUSH ES
    PUSH CS
    POP ES
    PUSH AX
    MOV AX, 0xCCCC
    XOR DI, DI
    MOV DX, 0xCCCC
    INT 0x86
    DB 0x9B, 0x9B ;$1 NRG
    INT 0x86
    MOV AX, 0x86CD
    MOV DX, 0x86CD
    MOV BL, 0xCC
    MOV CL, 0xCC
    INT 0x87
    POP AX
    POP DS
    MOV DI, AX
    ADD DI, AX
    AND DI, 0xFF00
    OR DI, 0x00A3
    MOV BL, 0x90
    MOV [BX], DI
    MOV [BX+0x02], CS
    MOV SI, 0x012B
    MOVSW
    PUSH CS
    POP SS
    MOV SP, DI
    ADD SP, 0x020E
    LEA SI, [BX+0x04]
    MOV CL, 0x4D
    DEC DI
    CALL FAR [BX]
    REP MOVSW
    MOV SI, 0x012B
    ADD AX, DI
    XOR DI, AX
    AND DI, 0xFF00
    OR DI, 0x00A3
    MOV [BX], DI
    MOVSW
    MOV SP, DI
    ADD SP, 0x020E
    LEA SI, [BX+0x04]
    MOV CL, 0x4D
    DEC DI
    CALL FAR [BX]