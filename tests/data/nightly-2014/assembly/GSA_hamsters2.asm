    JMP label1
    PUSH SS
    POP ES
    XOR DI, DI
    MOV AX, 0xA5A5
    INT 0x87
    PUSH SS
    POP DS
    MOV [BX], AX
    MOV WORD [BX+0x02], 0x1004
    PUSH CS
    POP DS
    CALL FAR [BX+SI]
    REP MOVSW
    SUB [BX], AX
    MOV DI, [BX]
    SUB SP, DX
    CALL FAR [BX+SI]
    CALL FAR [BX+SI]
    CALL FAR [BX+SI]
    CALL FAR [BX+SI]
    PUSH SS
    POP DS
    PUSH CS
    POP SS
    PUSH SS
    POP DS
    LES DI, [BX]
    PUSH CS
    POP DS
    MOVSW
    DEC DI
    XOR SI, SI
    PUSH SS
    POP ES
    XOR DI, DI
    MOV AX, 0xA5A5
    INT 0x87
    PUSH SS
    POP DS
    MOV [BX], AX
    MOV WORD [BX+0x02], 0x1004
    PUSH CS
    POP DS
    CALL FAR [BX+SI]
    REP MOVSW
    SUB [BX], AX
    MOV DI, [BX]
    CALL FAR [BX+SI]
    CALL FAR [BX+SI]
    CALL FAR [BX+SI]
    CALL FAR [BX+SI]
    PUSH SS
    POP DS
    PUSH CS
    POP SS
    PUSH SS
    POP DS
    LES DI, [BX]
    PUSH CS
    POP DS
    MOVSW
    DEC DI
    XOR SI, SI
    MOV BP, 0x0100
    DB 0x83, 0xE8, 0x68 ;$1 SUB AX, 0x0068
    JMP label1
    ADD [BX+SI], AL
label1:
    MOV WORD [0x2122], 0xC8A7
    MOV DX, AX
    MOV WORD [0x2124], 0xDD23
    ADD AH, 0x03
    MOV WORD [0x2126], 0xFAF3
    MOV AL, 0xA3
    MOV WORD [0x2128], 0x0A28
    MOV BX, 0x0111
    MOV WORD [0x212A], 0xE55F
    PUSH SS
    POP DS
    MOV [BX], AX
    MOV WORD [BX+0x02], 0x1004
    PUSH CS
    POP DS
    MOV WORD [0x215C], 0xC8A7
    MOV AX, 0xCCCC
    MOV WORD [0x215E], 0xDD23
    NOP
    MOV WORD [0x2160], 0xFAF3
    PUSH DS
    MOV WORD [0x2162], 0x0A28
    POP ES
    MOV WORD [0x2164], 0xE55F
    MOV DI, DX
    SUB DI, 0x0100
    INT 0x86
    MOV AX, DX
    MOV BYTE [0xDFE9], 0xB8
    MOV [0xDFEA], AX
    MOV WORD [0x2156], 0xC8A7
    ADD WORD [0xDFEA], WORD 0x0068
    MOV WORD [0x2158], 0xDD23
    MOV WORD [0xDFEC], 0xE0FF
    MOV WORD [0x215A], 0xFAF3
    MOV SI, AX
    MOV WORD [0x215C], 0x0A28
    MOV AX, 0xCCCC
    MOV WORD [0x215E], 0xE55F
    NOP
    ADD DI, 0x0200
    INT 0x86
    MOV DX, 0xA5A5
    MOV AX, SI
    PUSH AX
    MOV BYTE [0xDFE9], 0xB8
    MOV [0xDFEA], AX
    ADD WORD [0xDFEA], WORD 0x0068
    MOV WORD [0xDFEC], 0xE0FF
    ADD SI, 0x0184
    MOV AX, 0xA5A5
    INT 0x87
    PUSH SS
    POP ES
    XOR DI, DI
    MOV CX, 0x0008
    MOVSW
    MOVSW
    MOVSW
    POP AX
    MOV BYTE [0xDFE9], 0xB8
    MOV [0xDFEA], AX
    ADD WORD [0xDFEA], WORD 0x0068
    MOV WORD [0xDFEC], 0xE0FF
    REP MOVSW
    MOV CL, 0x09
    PUSH SS
    POP DS
    LES DI, [BX]
    PUSH CS
    POP DS
    MOVSW
    DEC DI
    XOR SI, SI
    MOV DX, AX
    XCHG BX, DX
    MOV AX, 0x0500
    ADD AX, [BX+word 0x0070]
    ADD WORD [BX+word 0x0070], 0x0500
    XCHG DX, BX
    PUSH SS
    POP DS
    PUSH CS
    POP SS
    MOV SP, [BX]
    ADD SP, AX
    MOV AX, 0x2E00
    MOV DX, 0x2900
    CALL FAR [BX+SI]
    REP MOVSW
    SUB [BX], AX
    MOV DI, [BX]
    SUB SP, DX
    MOV CX, 0x0009
    DEC BP
    JZ 516
    MOVSW
    DEC DI
    XOR SI, SI
    CALL FAR [BX+SI]
    CALL FAR [BX+SI]
    CALL FAR [BX+SI]
    PUSH SS
    POP ES
    XOR DI, DI
    MOV AX, 0xA5A5
    INT 0x87
    PUSH SS
    POP DS
    MOV [BX], AX
    MOV WORD [BX+0x02], 0x1004
    PUSH CS
    POP DS
    CALL FAR [BX+SI]
    REP MOVSW
    SUB [BX], AX
    MOV DI, [BX]
    CALL FAR [BX+SI]
    CALL FAR [BX+SI]
    CALL FAR [BX+SI]
    CALL FAR [BX+SI]
    PUSH SS
    POP DS
    PUSH CS
    POP SS
    PUSH SS
    POP DS
    LES DI, [BX]
    PUSH CS
    POP DS
    MOVSW
    DEC DI
    XOR SI, SI
    PUSH SS
    POP ES
    XOR DI, DI
    MOV AX, 0xA5A5
    INT 0x87
    PUSH SS
    POP DS
    MOV [BX], AX
    MOV WORD [BX+0x02], 0x1004
    PUSH CS
    POP DS
    CALL FAR [BX+SI]
    REP MOVSW
    SUB [BX], AX
    MOV DI, [BX]
    CALL FAR [BX+SI]
    CALL FAR [BX+SI]
    CALL FAR [BX+SI]
    CALL FAR [BX+SI]
    PUSH SS
    POP DS
    PUSH CS
    POP SS
    PUSH SS
    POP DS
    LES DI, [BX]
    PUSH CS
    POP DS
    MOVSW
    DEC DI
    XOR SI, SI