    JMP label1
    PUSH DS
    PUSH ES
    POP DS
    MOV CX, 0x0004
    LOOP $
    MOV BX, [0x00C8]
    ADD BX, 0x00D6
    POP DS
    MOV WORD [BX], 0x0097
    ADD BX, 0x005B
    MOV DX, BX
    NOT DX
    MOV BYTE [BX], 0xE9
    PUSH AX
    ADD AX, 0x014E
    ADD AX, DX
    MOV [BX+0x01], AX
    POP AX
    SUB BX, 0x001D
    PUSH BX
    NOT BX
    ADD BX, AX
    ADD BX, 0x013A
    ADD BX, 0x00BE
    MOV DX, BX
    POP BX
    MOV [BX-0x1D], DX
    MOV CX, 0x002C
    LOOP $
    MOV CX, 0x0004
    MOV SI, 0xC80F
    PUSH AX
    MOV DX, [SI]
    SUB DX, 0x12C3
    MOV AX, 0x1B3F
    MUL DX
    SUB AX, 0xFAEB
    MOV DI, AX
    ADD DI, 0x0047
    POP DX
    PUSH DX
    ADD DX, 0x0101
    ADD DX, 0x0036
    SUB DX, DI
    MOV [DI+0x01], DX
    MOV BYTE [DI], 0xE9
    SUB SI, 0x0200
    POP AX
    JMP label3
    MOV DX, 0xFF1F
    MOV AX, DX
    INT 0x87
    ADD DI, 0x0125
    MOV WORD [DI], 0x5153
    MOV WORD [DI+0x02], 0xD4FF
    JMP label2
    PUSH ES
    PUSH DS
    POP ES
    MOV DI, AX
    ADD DI, 0x0429
    INT 0x86
    SUB DI, 0x07F1
    INT 0x86
    MOV DX, 0xB953
    MOV AX, 0x51BB
    MOV BX, 0xAAAA
    INT 0x87
    SUB DI, 0x0100
    POP ES
    PUSH CS
    POP SS
    PUSH CS
    POP ES
    MOV CX, 0x5153
    MOV BX, 0xD4FF
    MOV SP, DI
    PUSH BX
    PUSH CX
    CALL SP
label1:
    PUSH DS
    PUSH ES
    POP DS
    MOV CX, 0x0004
    LOOP $
    MOV BX, [0x00C8]
    ADD BX, 0x00D6
    POP DS
    MOV WORD [BX], 0x0097
    ADD BX, 0x005B
    MOV DX, BX
    NOT DX
    MOV BYTE [BX], 0xE9
    PUSH AX
    ADD AX, 0x014E
    ADD AX, DX
    MOV [BX+0x01], AX
    POP AX
    SUB BX, 0x001D
    PUSH BX
    NOT BX
    ADD BX, AX
    ADD BX, 0x013A
    ADD BX, 0x00BE
    MOV DX, BX
    POP BX
    MOV [BX-0x1D], DX
    MOV CX, 0x002C
    LOOP $
    MOV CX, 0x0004
    MOV SI, 0xC80F
loop1:
    PUSH AX
    MOV DX, [SI]
    SUB DX, 0x12C3
    MOV AX, 0x1B3F
    MUL DX
    SUB AX, 0xFAEB
    MOV DI, AX
    ADD DI, 0x0047
    POP DX
    PUSH DX
    ADD DX, 0x0101
    ADD DX, 0x0036
    SUB DX, DI
    MOV [DI+0x01], DX
    MOV BYTE [DI], 0xE9
    SUB SI, 0x0200
    POP AX
    LOOP loop1
    JMP label3
    MOV DX, 0xFF1F
    MOV AX, DX
    INT 0x87
label2:
    ADD DI, 0x0125
    MOV WORD [DI], 0x5153
    MOV WORD [DI+0x02], 0xD4FF
    JMP label2
label3:
    PUSH ES
    PUSH DS
    POP ES
    MOV DI, AX
    ADD DI, 0x0429
    INT 0x86
    SUB DI, 0x07F1
    INT 0x86
    MOV DX, 0xB953
    MOV AX, 0x51BB
    MOV BX, 0xAAAA
    INT 0x87
    SUB DI, 0x0100
    POP ES
    PUSH CS
    POP SS
    PUSH CS
    POP ES
    MOV CX, 0x5153
    MOV BX, 0xD4FF
    MOV SP, DI
    PUSH BX
    PUSH CX
    CALL SP
    PUSH DS
    PUSH ES
    POP DS
    MOV CX, 0x0004
    LOOP $
    MOV BX, [0x00C8]
    ADD BX, 0x00D6
    POP DS
    MOV WORD [BX], 0x0097
    ADD BX, 0x005B
    MOV DX, BX
    NOT DX
    MOV BYTE [BX], 0xE9
    PUSH AX
    ADD AX, 0x014E
    ADD AX, DX
    MOV [BX+0x01], AX
    POP AX
    SUB BX, 0x001D
    PUSH BX
    NOT BX
    ADD BX, AX
    MOV CX, 0x002C
    LOOP $
    MOV CX, 0x0004
    MOV SI, 0xC80F
    PUSH AX
    MOV DX, [SI]
    SUB DX, 0x12C3
    MOV AX, 0x1B3F
    MUL DX
    SUB AX, 0xFAEB
    MOV DI, AX
    ADD DI, 0x0047
    POP DX
    PUSH DX
    ADD DX, 0x0101
    MOV BYTE [DI], 0xE9
    SUB SI, 0x0200
    POP AX
    INT 0x87
    ADD DI, 0x0125
    MOV WORD [DI], 0x5153
    MOV WORD [DI+0x02], 0xD4FF
    JMP label2
    PUSH ES
    PUSH DS
    POP ES
    MOV DI, AX
    ADD DI, 0x0429
    INT 0x86
    POP ES
    PUSH CS
    POP SS
    PUSH CS