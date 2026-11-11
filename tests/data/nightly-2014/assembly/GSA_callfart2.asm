    JMP label1
    INC CX
    XCHG BX, AX
    LOOP -8
    ADD BX, 0xBFFF
    ADD [BP+SI+0x6F00], BH
    JBE -111
    RET
    MOV DI, [BX]
    PUSH ES
    POP DS
    MOV [BX], AX
    ADD [BX+SI], AL
    POP ES
    MOV [0x8107], CX
    DB 0xC6, 0xCD, 0x86 ;$2 MOV CH, 0x86
    MOV SI, AX
    PUSH CS
    POP SS
    DB 0xF3, 0x90
    MOVSW
    XOR SI, SI
    CALL FAR [BX]
label1:
    MOV BX, AX
    MOV [0x1243], DX
    MOV [0x1243], DX
    MOV [0x2162], DX
    MOV [0x2162], DX
    MOV DX, 0x064E
    MOV AX, [0x3612]
    LEA SI, [BX+0x00EC]
    PUSH DS
    ADD BX, 0x0155
    NOP
    POP ES
    TEST AL, 0x80
    JZ label2
    XOR AL, 0x80
label2:
    XLATB
    SHL AL, 1
    XCHG AH, AL
    TEST AL, 0x80
    JZ label3
    XOR AL, 0x80
label3:
    XLATB
    SHL AL, 1
    ADD DI, 0x0100
    ADD DI, AX
    MOV [0x064E], SI
    MOV AX, 0xA4FF
    INT 0x86
    INT 0x86
    NOP
    MOV BX, 0x7026
    MOV DI, 0x01F5
    MOV DX, 0xC681
    MOV AX, 0xC689
    MOV [0xC6FF], SI
    INT 0x87
    XOR DI, DI
    PUSH SS
    POP ES
    MOV [0xC6FF], AX
    PUSH CS
    MOV AX, SI
    SUB SI, 0x001C
    POP SS
    MOV CX, 0x000D
    REP MOVSW
    NOP
    NOP
    AND AH, 0xFE
    ADD AH, 0x04
    MOV AL, 0xA2
    MOV BX, 0x0331
    MOV CX, 0x1003
    MOV SP, 0x6690
    PUSH ES
    POP DS
    MOV [BX], AX
    MOV [BX+0x02], CX
    MOV ES, CX
    MOV DI, [BX]
    DEC DI
    PUSH CS
    POP SS
    LEA SP, [BX+DI]
    MOV CX, 0x0008
    XOR SI, SI
    NOP
    NOP
    MOV BP, 0x0002
    MOV DX, 0x2E00
    MOVSW
    MOVSW
    SUB DI, BP
    CALL FAR [BX]
    ES:
    CALL FAR [BX]
    DB 0x68
    MOVSW
    REP MOVSW
    MOV CL, 0x08
    SUB DI, [SI]
    LEA SP, [BX+DI]
    XOR SI, SI
    SUB [BX], DX
    MOVSW
    MOVSW
    SUB DI, BP
    CALL FAR [BX]
    ADC AX, 0x902E
    ADC AX, 0x0E17
    POP ES
    MOV DI, 0xA500
    INT 0x86
    MOV DI, [0x1243]
    SUB DI, 0x0050
    INT 0x86
    XOR DI, DI
    MOV DI, 0x01F5
    MOV DX, 0x1FFF
    MOV AX, 0x4F90
    MOV BX, 0xCCCC
    MOV CX, 0xCCCC
    INT 0x87
    NOP
    NOP
    XOR DI, DI
    CALL prcd1
prcd1:
    POP AX
    DB 0x83, 0xE8, 0x46 ;$1 SUB AX, 0x0046
    MOV CX, 0x000C
    MOV SI, AX
    PUSH SS
    POP ES
    XOR DI, DI
    REP MOVSW
    ADD SI, 0x0003
    MOVSW
    SHR AH, 1
    MOV AL, 0xA2
    MOV BX, 0x0331
    MOV CX, 0x1003
    PUSH ES
    POP DS
    MOV [BX], AX
    NOP
    MOV [BX+0x02], CX
    MOV ES, CX
label4:
    PUSH CS
    POP SS
    MOV DI, [BX]
    DEC DI
    LEA SP, [BX+DI]
    MOV CX, 0x0008
    XOR SI, SI
    MOV BP, 0x0002
    MOV DX, 0x1700
    MOVSW
    MOVSW
    SUB DI, BP
    CALL FAR [BX]
    ADD [BX+SI], BP
    XOR BX, [BP+SI+0x7D]
    ADD AL, 0x2F
    SS:
    POP CX
    NOP
    OR DX, [BP+SI]
    XOR AX, 0x675C
    PUSH CS
    ADC [BX+SI], DI
    INC BX
    DB 0x6A
    OR AX, 0x3F14
    INC SI
    DB 0x69
    JO label6+1
    AND AL, [DI+0x6C]
    JA label7+1
    AND [BX+SI+0x53], CX
    JP label8
    AND AL, 0x4F
    PUSH SI
    JNS label5
label5:
    SUB SI, [BP+SI]
    PUSH BP
    JL label6+1
    CS:
    NOP
    POP AX
    DB 0x63
    OR CH, [DI]
label6:
    XOR AL, 0x5F
    DB 0x66
    OR [BX+SI], DX
    CMP AX, [BP+SI+0x65]
label7:
    OR AL, 0x17
    DS:
    INC CX
    DB 0x68
label8:
    JNB label10+2
    CMP AX, 0x6F44
    JBE label11+1
    AND [BP+DI+0x52], CL
    JNZ label12+2
    DAA
    DEC SI
    PUSH CX
    JS label9
    SUB CL, [DI+0x54]
label9:
    JG label4
    SUB [BX+SI], SI
    POP BX
    DB 0x62
label10:
    ADD AX, 0x372C
    POP SI
    DB 0x61
label11:
    OR [BP+DI], DL
    CMP BL, [DI+0x64]
    DB 0x0F
    PUSH SS
label12:
    CMP [BX+SI+0x6B], AX
    JB label15+1
    CMP AL, 0x47
    DB 0x6E
    JNO label16+3
    AND CX, [BP+SI+0x6D]
    JZ label17
    ES:
    DEC CX
    PUSH AX
    JNP label13+2
label13:
    AND AX, 0x904C
label14:
    JLE label14+1
label15:
    MOV DI, 0xBA00
    ADD [BX+0x76], CH
label16:
    ADD BX, 0x3F8B
    PUSH ES
    POP DS
    MOV [BX], AX
    ADD [BX+SI], AL
    POP ES
    MOV [0x8107], CX
label17:
    DB 0xC6, 0xCD, 0x86 ;$2 MOV CH, 0x86
    MOV SI, AX
    PUSH CS
    POP SS
    REP MOVSW
    NOP