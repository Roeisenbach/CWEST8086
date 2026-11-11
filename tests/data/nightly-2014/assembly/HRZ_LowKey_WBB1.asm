    ADD AX, 0x004F
    MOV SI, AX
    MOV [0x8A43], SI
    DB 0x83, 0xC0, 0x1C ;$1 ADD AX, 0x001C
    STOSW
    PUSH ES
    MOV AX, 0xF9EB
    MOV DX, 0xCCCC
    PUSH CS
    POP ES
    STD
    INT 0x87
    POP ES
    CLD
    MOV BX, 0x003E
    MOV DI, 0x3424
    PUSH CS
    PUSH ES
    PUSH CS
    NOP
    NOP
    NOP
    NOP
    NOP
    NOP
    NOP
    NOP
    NOP
    NOP
    NOP
    NOP
    POP ES
    POP DS
    POP SS
    MOV DX, 0xFFF0
    XOR SI, SI
    MOV SP, [BX]
    ADD SP, 0xF186
    MOV CX, 0x0008
    MOV BP, 0x1700
    MOV AX, 0x0815
    CALL FAR [BX+SI]
    NOP
    NOP
    NOP
    NOP
    NOP
    NOP
    XOR SI, SI
    MOV WORD [0x769D], 0x1CFF
    PUSH SS
    POP DS
    PUSH CS
    POP SS
    NOP
    NOP
    MOV SP, 0x2521
    MOV WORD [SI], 0x1DFD
    MOV WORD [SI+0x02], 0x158A
    CALL FAR [SI]
    MOV SI, AX
    PUSH ES
    MOV AX, 0xF9EB
    MOV DX, 0xCCCC
    PUSH CS
    NOP
    POP ES
    MOV CX, 0x8A43
    MOV BX, 0x26FF
    STD
    INT 0x87
    POP ES
    CLD
    MOV DI, 0x3421
    MOV DX, 0x33A1
    PUSH ES
    POP DS
    MOV BX, 0x003E
    NOP
    MOV [BX], DX
    MOV WORD [BX+0x02], 0x1008
    ADD DX, 0x4000
    ADD BL, 0x04
    NOP
    MOV [BX], DX
    MOV WORD [BX+0x02], 0x1008
    PUSH CS
    POP DS
    PUSH ES
    PUSH DS
    POP ES
    ADD SI, 0x0083
    MOVSW
    MOVSW
    ADD DI, 0x3FFC
    LEA AX, [DI+0x03]
    MOVSW
    MOVSW
    XOR DI, DI
    POP ES
    MOV CX, 0x000B
    REP MOVSW
    MOV DI, AX
    NOP
    NOP
    JMP label1
    NOP
label1:
    PUSH CS
    PUSH ES
    PUSH CS
    POP ES
    POP DS
    POP SS
    MOV DX, 0xFFF0
    XOR SI, SI
    MOV SP, [BX]
    ADD SP, 0xF186
    MOV CX, 0x0008
    MOV BP, 0x1600
    MOV AX, 0x0715
    CALL FAR [BX+SI]
    NOP
    NOP
    NOP
    NOP
    NOP
    NOP
    NOP
    NOP
    NOP
    NOP
    NOP
    SUB SP, DX
    CALL FAR [BX+SI]
    SUB SP, DX
    CALL FAR [BX+SI]
    REP MOVSW
    SUB [BX+0x01], AH
    SUB DI, AX
    MOV CL, 0x08
    MOVSW
    MOVSW
    DEC DI
    XOR SI, SI
    SUB SP, BP
    CALL FAR [BX+SI]
    SUB SP, DX
    CALL FAR [BX+SI]