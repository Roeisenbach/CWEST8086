    MOV SI, AX
    MOV BX, AX
    ADD AX, 0x0097
    MOV [BX], AX
    PUSH CS
    POP ES
    MOV AX, 0xD4FF
    MOV DX, 0xCC0E
    MOV CX, BX
    MOV BX, 0x26FF
    INT 0x87
    MOV [SI+0x0098], SI
    MOV BP, SI
    ADD BP, 0x006F
    MOV BX, 0xE33F
    MOV CX, 0x0098
    REP MOVSW
    DB 0x9B, 0x9B ;$1 NRG
    XCHG AX, [0xB80F]
    XCHG SI, [0xBA0F]
    XCHG DI, [0xB60F]
    XCHG DX, [0xBC0F]
    PUSH AX
    PUSH SI
    PUSH DI
    PUSH DX
    MOV CL, 0x04
loop1:
    XOR SI, SI
    POP AX
    SUB AX, 0xBEC3
    MUL BX
    MOV DI, 0xFAEB
    XOR DI, AX
    ADD DI, 0x004C
    MOV BYTE [DI], 0xB9
    MOV [DI+0x01], BP
    MOV WORD [DI+0x03], 0xE1FF
    LOOP loop1
    PUSH CS
    POP SS
    MOV CX, 0x5153
    MOV BX, 0xD4FF
    MOV SP, BP
    SUB SP, 0x006F
    PUSH BX
    PUSH CX
    CALL SP
    PUSH CS
    POP SS
    PUSH CS
    POP ES
    MOV SP, AX
    MOV AX, 0x891F
    MOV DX, 0xD1DD
    MOV CX, 0xCCCC
    MOV BX, 0xCCCC
    INT 0x87
    MOV DX, 0x5350
    MOV CX, 0x5251
    MOV BX, 0xFF90
    MOV AX, 0x90D4
    PUSH AX
    PUSH BX
    PUSH CX
    PUSH DX
    NOP
    CALL SP
    NOP
    MOV BP, 0x0000
    PUSH AX
    ADD BP, 0x006F
    MOV BX, 0xE33F
    MOV CX, 0x0097
    REP MOVSW
    DB 0x9B, 0x9B ;$1 NRG
    XCHG DX, [0xBC0F]
    XCHG DI, [0xB60F]
    XCHG SI, [0xBA0F]
    XCHG AX, [0xB80F]
    PUSH DX
    PUSH DI
    PUSH SI
    PUSH AX
    MOV CL, 0x04
loop2:
    XOR SI, SI
    POP AX
    SUB AX, 0xBEC3
    MUL BX
    MOV DI, 0xFAEB
    XOR DI, AX
    ADD DI, 0x004C
    MOV BYTE [DI], 0xB9
    MOV [DI+0x01], BP
    MOV WORD [DI+0x03], 0xE1FF
    LOOP loop2
    PUSH CS
    POP SS
    PUSH CS
    POP ES
    MOV AX, 0xE8E0
    MOV DX, 0xCCCC
    MOV CX, 0xCCCC
    MOV BX, 0xCCCC
    INT 0x87
    MOV CX, 0x5153
    MOV BX, 0xD4FF
    POP AX
    MOV SP, AX
    PUSH BX
    PUSH CX
    CALL SP