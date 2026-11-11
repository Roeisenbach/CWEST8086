    MOV BX, AX
    MOV SI, AX
    CMP SI, 0xE5AC
    JB label1
    SUB SI, 0x1056
    OR SI, 0x0003
    SUB SI, 0x0004
label1:
    OR SI, 0x0003
    MOV [BX+word 0x0051], SI
    MOV WORD [SI+0x0A50], 0x1FFF
    LEA BP, [SI+0x1002]
    LEA SI, [BX+word 0x0051]
    MOV CX, 0x0019
    REP MOVSW
    MOV AX, 0x10A5
    PUSH AX
    PUSH AX
    POP DS
    POP SS
    XCHG BP, SP
    PUSH ES
    PUSH DS
    POP ES
    POP DS
    MOV SI, 0x000A
    XOR BX, BX
    MOV DI, [BX]
    INC DI
    MOV CX, 0x0012
    MOV AX, 0xEC29
    MOV DX, 0x1000
    MOV BP, 0x0000
    CALL FAR [BX]
    MOV BX, 0xA5AA
    ADC [BX+SI-0x5A70], DL
    ADC BH, CH
    MOV SI, 0xA5F3
    MOV DL, [BX+0x01]
    ADD DI, DX
    CMP DI, 0xE5AC
    JB label2
    AND DI, 0x00FF
label2:
    OR DI, 0x0003
    SUB DI, BP
    MOV [BX], DI
    MOVSW
    DEC DI
    MOV CX, 0x0012
    MOV SI, 0x000A
    LEA SP, [DI+0x1001]
    CALL FAR [BX]
    CALL FAR [BX]