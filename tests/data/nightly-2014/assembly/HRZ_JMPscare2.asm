    NOP
    JMP label1
    INT3
    INT3
    SUB SP, BP
    CALL FAR [BX]
    INT3
    INT3
    ADD AX, 0x0067
    ADD DI, 0x0001
    STOSW
    XOR DI, DI
    MOV AL, 0xB9
    STOSB
    ADD DI, 0x0002
    MOV AX, 0xE1FF
    STOSW
    POP DI
    ADD DI, 0x020B
    PUSH ES
    PUSH CS
    POP ES
    MOV BX, 0xE33F
    MOV CL, 0x94
    REP MOVSW
    NRG
    XCHG AX, [0xB60F]
    XCHG SI, [0xB80F]
    XCHG BP, [0xBA0F]
    XCHG DX, [0xBC0F]
    POP DS
    PUSH AX
    PUSH SI
    PUSH BP
    PUSH DX
    MOV CL, 0x04
loop1:
    XOR SI, SI
    POP AX
    SUB AX, 0xBEC3
    MUL BX
    MOV DI, 0xFAEB
    XOR DI, AX
    ADD DI, 0x004E
    MOVSW
    MOVSW
    MOVSW
    LOOP loop1
    XOR DX, DX
    MOV DI, 0xBE11
    JMP WORD [0x0001]
    PUSH AX
    XOR DI, DI
    POP AX
    PUSH CS
    POP DS
    XOR DH, DH
    MOV AL, 0xA1
    PUSH SS
    POP ES
    XOR DI, DI
    REP MOVSW
    PUSH ES
    PUSH CS
    POP ES
    POP DS
    PUSH CS
    POP SS
    MOV SP, AX
    MOV [BX], AX
    MOV [BX+0x02], CS
    SUB SP, BP
    ADD SP, 0x0002
    XOR SI, SI
    REP MOVSW
    ADD [BX], AX
    MOV DI, [BX]
    MOVSW
    MOVSW
    DEC DI
    ADD SP, [SI]
    DEC DX
    XOR SI, SI
    CALL FAR [BX]
    SUB SP, BP
    CALL FAR [BX]
label1:
    PUSH AX
    ADD AX, 0x0107
    ADD DI, 0x0001
    STOSW
    XOR DI, DI
    MOV AL, 0xB9
    STOSB
    ADD DI, 0x0002
    MOV AX, 0xE1FF
    STOSW
    POP DI
    ADD DI, 0x020B
    PUSH ES
    PUSH CS
    POP ES
    MOV BX, 0xE33F
    MOV CL, 0x94
    REP MOVSW
    NRG
    XCHG DX, [0xBC0F]
    XCHG BP, [0xBA0F]
    XCHG SI, [0xB80F]
    XCHG AX, [0xB60F]
    MOV DI, [0x11BF]
    SUB DI, 0x0100
    INT 0x86
    INT 0x86
    POP DS
    PUSH AX
    PUSH SI
    PUSH BP
    PUSH DX
    MOV CL, 0x04
loop2:
    XOR SI, SI
    POP AX
    SUB AX, 0xBEC3
    MUL BX
    MOV DI, 0xFAEB
    XOR DI, AX
    ADD DI, 0x004E
    MOVSW
    MOVSW
    MOVSW
    LOOP loop2
    XOR BX, BX
    MOV DI, 0xBF11
    JMP WORD [0x0001]
    MOV AX, DI
    SHL AX, 1
    SHL AX, 1
    SHL AX, 1
    SHL AX, 1
    PUSH AX
    XOR DI, DI
    MOV AX, 0xCCCC
    MOV DX, 0x01CC
    MOV CH, 0xCC
    INT 0x87
    POP AX
    XOR CX, CX
    PUSH CS
    POP DS
    MOV DX, BX
    XOR DH, DH
    CALL prcd1
prcd1:
    POP SI
    ADD AX, SI
    ADD SI, 0x003A
    MOV AL, 0xA1
    PUSH SS
    POP ES
    XOR DI, DI
    MOV CL, 0x0D
    REP MOVSW
    PUSH ES
    PUSH CS
    POP ES
    POP DS
    PUSH CS
    POP SS
    MOV SP, AX
    MOV BX, 0x0050
    MOV [BX], AX
    MOV AX, 0x0FFE
    MOV [BX+0x02], AX
    MOV ES, AX
    ADD SP, 0xFFE0
    MOV AX, 0xA100
    MOV BP, 0x001E
    SUB SP, BP
    ADD SP, 0x0002
    XOR SI, SI
    ADD SI, 0x0014
    JMP label2
    REP MOVSW
label2:
    MOV CL, 0x09
    ADD [BX], AX
    MOV DI, [BX]
    MOVSW
    MOVSW
    DEC DI
    ADD SP, [SI]
    DEC DX
    JZ label3
    XOR SI, SI
    CALL FAR [BX]
    SUB SP, BP
    CALL FAR [BX]
    XOR AL, 0xC2
    ADD AX, 0x01CD
    ADD DI, 0x0001
    STOSW
    XOR DI, DI
    MOV AL, 0xB9
    STOSB
    ADD DI, 0x0002
    MOV AX, 0xE1FF
    STOSW
    POP DI
label3:
    ADD DI, 0x020B
    PUSH ES
    PUSH CS
    POP ES
    MOV BX, 0xE33F
    MOV CL, 0x94
    REP MOVSW
    NRG
    XCHG AX, [0xB60F]
    XCHG SI, [0xB80F]
    XCHG BP, [0xBA0F]
    XCHG DX, [0xBC0F]
    POP DS
    PUSH AX
    PUSH SI
    PUSH BP
    PUSH DX
    MOV CL, 0x04
loop3:
    XOR SI, SI
    POP AX
    SUB AX, 0xBEC3
    MUL BX
    MOV DI, 0xFAEB
    XOR DI, AX
    ADD DI, 0x004E
    MOVSW
    MOVSW
    MOVSW
    LOOP loop3
    XOR DX, DX
    MOV DI, 0xBF11
    JMP WORD [0x0001]
    PUSH AX
    XOR DI, DI
    POP AX
    PUSH CS
    POP DS
    XOR DH, DH
    MOV AL, 0xA1
    PUSH SS
    POP ES
    XOR DI, DI
    REP MOVSW
    PUSH ES
    PUSH CS
    POP ES
    POP DS
    PUSH CS
    POP SS
    MOV SP, AX
    MOV [BX], AX
    MOV [BX+0x02], CS
    SUB SP, BP
    ADD SP, 0x0002
    XOR SI, SI
    REP MOVSW
    DB 0x01  ; ADD [BX+SI], AX