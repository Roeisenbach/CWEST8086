    NOP
    MOV BX, AX
    MOV DX, [0x1243]
    PUSH WORD [0x1243]
    ADD BX, 0x0180
    PUSH DS
    POP ES
    NOP
    NOP
    NOP
    MOV DI, AX
    MOV DX, 0x26FF
    MOV AX, 0x1E06
    MOV [0x1E06], BX
    INT 0x86
    POP DX
    NOP
    NOP
    NOP
    NOP
    NOP
    MOV DI, AX
    MOV DX, 0x26FF
    MOV AX, 0x530E
    MOV [0x530E], BX
    INT 0x86
    JMP label1
    MOV SI, AX
loop1:
    ADD SI, 0x4F90
    CALL FAR [BX]
    INC BP
    MOV [0x1FFF], AL
    INC CX
    XCHG BX, AX
    LOOP loop1
    ADD BX, 0x8900
    MOVSW
    NOP
    XOR SI, AX
    PUSH ES
    ADD AL, [BX+SI]
    MOVSW
    SUB [BX+0x02], CX
    XOR DI, DI
    MOV DX, 0x0000
    INT3
    ADD BX, 0x3F8B
    PUSH ES
    POP DS
    MOV BYTE [BX+DI-0x3877], 0x4F
    DEC WORD [BX+DI+word 0x0007]
    ADD BH, AL
    INC DI
    POP ES
    MOV [0x8107], CX
    NOP
    DB 0xC6, 0xCD, 0x86 ;$2 MOV CH, 0x86
    MOV SI, AX
    PUSH CS
    POP SS
    REP MOVSW
    XOR SI, SI
    CALL FAR [BX]
label1:
    XCHG BP, BX
    XOR DI, DI
    MOV [0x7EA5], DI
    MOV [0x7DA5], DI
    MOV [0xCEA5], DI
    MOV [0xCDA5], DI
    MOV [0x3EA5], DI
    NOP
    NOP
    MOV [0x3DA5], DI
    MOV BYTE [DI], 0xCC
    MOV BYTE [0x06F1], 0xCC
    MOV BYTE [0xF631], 0xCC
    MOV BYTE [0x0789], 0xCC
    MOV BYTE [0x5141], 0xCC
    MOV BYTE [0x0006], 0xCC
    MOV BYTE [0xFE00], 0xCC
    NOP
    NOP
    MOV DI, 0x01FB
    MOV BX, 0xA026
    MOV CX, 0xC6A0
    MOV AX, 0x2A45
    MOV DX, 0x1FFF
    MOV [0xC6A0], BP
    INT 0x87
    XOR DI, DI
    PUSH SS
    POP ES
    PUSH CS
    POP SS
    MOV AX, BP
    MOV SI, AX
    SUB SI, 0x001B
    MOV CX, 0x000D
    NOP
    NOP
    REP MOVSW
    XOR DI, DI
    MOV BX, [0x7EA5]
    CMP BX, DI
    JNZ label2
    MOV BX, [0x7DA5]
    CMP BX, DI
    JNZ label2
    MOV BX, [0xCEA5]
    CMP BX, DI
    JNZ label2
    MOV BX, [0xCDA5]
    CMP BX, DI
    JNZ label2
    NOP
    NOP
    NOP
    MOV BX, [0x3EA5]
    CMP BX, DI
    JNZ label2
    CMP BX, [0x3DA5]
    CMP BX, DI
    JNZ label2
    JMP label3
label2:
    MOV [0x4F23], BP
    MOV WORD [BX+0x65], 0x4F23
    MOV WORD [BX+0x63], 0x26FF
label3:
    OR AH, 0x01
    ADD AH, 0x02
    NOP
    MOV AL, 0xA2
    MOV BX, 0x0331
    MOV CX, 0x1003
    MOV SP, 0xA242
    PUSH ES
    POP DS
    MOV [BX+0x02], CX
    MOV [BX], AX
    MOV ES, CX
    MOV CX, 0x0008
    PUSH CS
    POP SS
    MOV DI, [BX]
    DEC DI
    LEA SP, [BX+DI]
    XOR SI, SI
    MOV BP, 0x0002
    MOV DX, 0x2E00
    NOP
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
    ADC AX, 0x152E
    POP SS
    MOV DI, [0x0000]
    NOP
    NOP
    PUSH CS
    POP ES
    SUB DI, 0x0050
    INT 0x86
    MOV DI, [0xCCCC]
    SUB DI, 0x0050
    INT 0x86
    XOR DI, DI
    MOV DI, 0x01FB
    MOV AX, 0xB800
    MOV DX, 0x1FFF
    MOV BX, 0xCCCC
    MOV CX, 0xCCCC
    INT 0x87
    XOR DI, DI
    NOP
    NOP
    CALL prcd1
prcd1:
    POP AX
    DB 0x83, 0xE8, 0x4B ;$1 SUB AX, 0x004B
    MOV CX, 0x000C
    MOV SI, AX
    PUSH SS
    POP ES
    XOR DI, DI
    REP MOVSW
    ADD SI, 0x0002
    MOVSW
    SHR AH, 1
    MOV AL, 0xA2
    MOV BX, 0x0331
    MOV CX, 0x1003
    MOV SP, 0x0340
    PUSH ES
    POP DS
    NOP
    NOP
    MOV [BX], AX
    MOV [BX+0x02], CX
    MOV ES, CX
    PUSH CS
    POP SS
    MOV DI, [BX]
    DEC DI
    LEA SP, [BX+DI]
    MOV CX, 0x0008
    XOR SI, SI
    MOV BP, 0x0002
loop2:
    MOV DX, 0x1700
    MOVSW
    MOVSW
    SUB DI, BP
    CALL FAR [BX]
    LOOPZ 517
    INC CX
    XCHG BX, AX
    LOOP loop2+1
    ADC WORD [BX+SI-0x6CBF], 0xF2E2
    DB 0x81  ; ADD WORD [BX+SI], WORD 0x0000