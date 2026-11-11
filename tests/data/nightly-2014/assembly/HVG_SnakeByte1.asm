    PUSH CS
    PUSH CS
    POP ES
    POP SS
    MOV SP, AX
    MOV DI, AX
    ADD DI, 0x00F3
    PUSH AX
    MOV CX, 0x009F
    MOV BX, 0x11BF
    INT 0x86
    INT 0x86
    REP STOSW
    MOV AX, [0xBA0F]
    MOV CX, 0x0035
loop1:
    SUB AX, 0x4743
    PUSH BX
    PUSH AX
    DIV BX
    MOV AX, DX
    MOV DX, 0x0257
    MUL DX
    DIV BX
    SUB BX, DX
    MOV DX, BX
    POP AX
    POP BX
    DIV BX
    LOOP loop1
    XOR AX, 0xFAEB
    POP BX
    ADD BX, 0x0091
    MOV [BX-0x0091], BX
    MOV [BX+0x03], BX
    MOV [BX+0x54], BX
    SUB BX, 0x0091
    XCHG BX, AX
    ADD BX, 0x004E
    MOV WORD [BX], 0x26FF
    MOV [BX+0x02], AX
    MOV BX, AX
    ADD BX, WORD 0x0079
    ADD AX, 0x0067
    MOV [BX], AX
    MOV SI, AX
    CMP WORD [BX+0x04F3], 0xCCCC
    JNZ label1
    CMP WORD [BX-0x04F3], 0xCCCC
    JNZ label1
    CALL FAR [BX]
    NOP
    NOP
    ADD [BX+SI], DL
label1:
    ADD BX, 0x0543
    MOV DI, BX
    MOV CX, 0x0016
    REP MOVSW
    MOV SI, BX
    ADD BX, 0x0012
    MOV [BX], SI
    CALL FAR [BX]
    PUSH CS
    POP ES
    MOV DI, 0x0000
    MOV AX, [0xB60F]
    PUSH DI
    MOV DX, 0x0000
    MOV CX, 0x0035
loop2:
    SUB AX, 0x4743
    PUSH BX
    PUSH AX
    DIV BX
    MOV AX, DX
    MOV DX, 0x0257
    MUL DX
    DIV BX
    SUB BX, DX
    MOV DX, BX
    POP AX
    POP BX
    DIV BX
    LOOP loop2
    XOR AX, 0xFAEB
    POP DI
    MOV BX, DI
    ADD BX, 0x0051
    MOV [BX-0x00E2], BX
    XCHG BX, AX
    SUB AX, 0x00E2
    ADD BX, 0x004E
    MOV WORD [BX], 0x26FF
    ADD BX, 0x0002
    MOV [BX], AX
    ADD DI, 0x0062
    MOV AX, 0xCCCC
    MOV CX, 0xFFFF
    REP STOSW
    PUSH CS
    POP ES
    MOV DI, 0x0000
    ADD DI, 0x1000
    MOV AX, 0xCCCC
    MOV CX, 0xFFFF
    REP STOSW