    JMP label2
    JMP label1
    JMP label4
    JMP label5
label1:
    MOV AX, 0xABE9
    MOV DX, 0x5000
    PUSH CS
    POP ES
    INT 0x87
    MOV DI, 0x0000
    INT 0x86
    PUSH CS
    POP SS
    MOV CX, 0x5253
    MOV DX, 0x5551
    MOV BX, 0xD4FF
    MOV BP, 0xF401
    MOV SP, AX
    ADD SP, 0x002F
    MOV SI, 0x0132
    ADD SP, SI
    PUSH BX
    PUSH DX
    PUSH CX
    PUSH BP
    CALL SP
label2:
    STOSW
    MOV CX, 0x00A4
    LOOP $
    MOV CX, AX
    ADD CX, 0x0004
    MOV BX, 0xE33F
    XCHG AX, [0xB80F]
    XCHG BP, [0xB60F]
    ADD AX, 0x413D
    MUL BX
    MOV BX, AX
    XOR BX, 0xFAEB
    ADD BX, 0x004C
    MOV BYTE [BX], 0xBE
    MOV [BX+0x01], CX
    MOV WORD [BX+0x03], 0xE6FF
    MOV AX, BP
    ADD CX, 0x0003
    ADD AX, 0x413D
    MOV BX, 0xE33F
    MUL BX
    MOV BX, AX
    XOR BX, 0xFAEB
    ADD BX, 0x004C
    MOV BYTE [BX], 0xBE
    MOV [BX+0x01], CX
    MOV WORD [BX+0x03], 0xE6FF
    PUSH CS
    POP ES
    PUSH CS
    POP SS
    MOV SI, AX
    ADD SI, 0x00A1
    MOV DI, SI
    MOV SP, AX
    SUB SP, 0x0153
    MOV AX, 0xABE9
    MOV DX, 0x5000
    INT 0x87
    MOV WORD [DI+0x0100], 0xEBFE
label3:
    PUSH SP
    SUB SP, 0x0121
    CMP WORD [DI+0x0100], 0xEBFE
    JZ label3
    MOV CX, 0x000F
    REP MOVSW
    MOV SI, DI
    ADD SI, 0xFFE1
    JMP SI
label4:
    MOV AX, 0xABE9
    MOV DX, 0x5000
    PUSH CS
    POP ES
    INT 0x87
    MOV DI, 0x0000
    INT 0x86
    PUSH CS
    POP SS
    MOV CX, 0x5253
    MOV DX, 0x5551
    MOV BX, 0xD4FF
    MOV BP, 0xF401
    MOV SP, AX
    ADD SP, 0x010E
    MOV SI, 0x0153
    JMP label6
label5:
    MOV AX, 0xABE9
    MOV DX, 0x5000
    PUSH CS
    POP ES
    INT 0x87
    MOV DI, 0x0000
    INT 0x86
    PUSH CS
    POP SS
    MOV CX, 0x5253
    MOV DX, 0x5551
    MOV BX, 0xD4FF
    MOV BP, 0xF401
    MOV SP, AX
    ADD SP, 0x010E
    MOV SI, 0x0157
label6:
    ADD SP, SI
    PUSH BX
    PUSH DX
    PUSH CX
    PUSH BP
    CALL SP
    INT3
    INT3
    INT3
    INT3
    ADD [BX+SI], AL
    ADD [BX+SI], AL