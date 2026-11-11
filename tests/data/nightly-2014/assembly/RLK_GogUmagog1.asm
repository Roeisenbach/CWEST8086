    JMP label5
    MOV BX, AX
    MOV SI, AX
    CMP SI, 0xE5AC
    JB label1
    SUB SI, 0x1056
    OR SI, 0x0003
label1:
    MOV [BX+word 0x0069], SI
    MOV WORD [SI+0x0A50], 0x1FFF
    MOV BL, AH
    MOV BH, 0x00
    MOV DI, AX
    ADD DI, WORD 0x0069
    XCHG SI, DI
    XCHG DI, BX
    CMP DI, 0x0030
    JA label2
    ADD DI, 0x0030
label2:
    MOVSW
    MOVSW
    SUB DI, 0x0004
    MOV BP, [SI-0x04]
    ADD BP, 0x1002
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
    PUSH DI
    MOV DI, [DI]
    INC DI
    POP BX
    MOV CX, 0x0024
    SHR CX, 1
    JNB label3
    INC CX
label3:
    MOV AX, 0xEC29
    MOV DX, 0x1350
    MOV BP, 0x0000
    CALL FAR [BX]
    MOV BX, 0xA5AA
    ADC [BX+SI-0x5A70], DL
    ADC BH, CH
    MOV SI, 0xA5F3
    MOV DL, [BX+0x01]
    ADD DI, DX
    CMP DI, 0xE5AC
    JB label4
    AND DI, 0x00FF
label4:
    OR DI, 0x0003
    SUB DI, BP
    MOV [BX], DI
    STOSW
    DEC DI
    MOV CX, 0x0013
    MOV SI, 0x000A
    LEA SP, [DI+0x1001]
    CALL FAR [BX]
label5:
    MOV BX, AX
    MOV SI, AX
    CMP SI, 0xE5AC
    JB label6
    SUB SI, 0x1056
    OR SI, 0x0003
label6:
    MOV [BX+0x0101], SI
    MOV WORD [SI+0x0A50], 0x1FFF
    MOV BL, AH
    MOV BH, 0x00
    MOV DI, AX
    ADD DI, 0x0101
    XCHG SI, DI
    XCHG DI, BX
    CMP DI, 0x0030
    JA label7
    ADD DI, 0x0030
label7:
    MOVSW
    MOVSW
    SUB DI, 0x0004
    MOV BP, [SI-0x04]
    ADD BP, 0x1002
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
    PUSH DI
    MOV DI, [DI]
    INC DI
    POP BX
    MOV CX, 0x0024
    SHR CX, 1
    JNB label8
    INC CX
label8:
    MOV AX, 0xEC29
    MOV DX, 0x1350
    MOV BP, 0x0000
    CALL FAR [BX]
    MOV BX, 0xA5AA
    ADC [BX+SI-0x5A70], DL
    ADC BH, CH
    MOV SI, 0xA5F3
    MOV DL, [BX+0x01]
    ADD DI, DX
    CMP DI, 0xE5AC
    JB label9
    AND DI, 0x00FF
label9:
    OR DI, 0x0003
    SUB DI, BP
    MOV [BX], DI
    STOSW
    DEC DI
    MOV CX, 0x0013
    MOV SI, 0x000A
    LEA SP, [DI+0x1001]
    CALL FAR [BX]
    MOV BX, AX
    MOV SI, AX
    CMP SI, 0xE5AC
    JB label10
    SUB SI, 0x1056
    OR SI, 0x0003
label10:
    MOV [BX+word 0x0069], SI
    MOV WORD [SI+0x0A50], 0x1FFF
    MOV BL, AH
    MOV BH, 0x00
    MOV DI, AX
    ADD DI, WORD 0x0069
    XCHG SI, DI
    XCHG DI, BX
    CMP DI, 0x0030
    JA label11
    ADD DI, 0x0030
label11:
    MOVSW
    MOVSW
    SUB DI, 0x0004
    MOV BP, [SI-0x04]
    ADD BP, 0x1002
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
    PUSH DI
    MOV DI, [DI]
    INC DI
    POP BX
    MOV CX, 0x0024
    SHR CX, 1
    JNB label12
    INC CX
label12:
    MOV AX, 0xEC29
    MOV DX, 0x1350
    MOV BP, 0x0000
    CALL FAR [BX]
    MOV BX, 0xA5AA
    ADC [BX+SI-0x5A70], DL
    ADC BH, CH
    MOV SI, 0xA5F3
    MOV DL, [BX+0x01]
    ADD DI, DX
    CMP DI, 0xE5AC
    JB label13
    AND DI, 0x00FF
label13:
    OR DI, 0x0003
    SUB DI, BP
    MOV [BX], DI
    STOSW
    DEC DI
    MOV CX, 0x0013
    MOV SI, 0x000A
    LEA SP, [DI+0x1001]
    CALL FAR [BX]