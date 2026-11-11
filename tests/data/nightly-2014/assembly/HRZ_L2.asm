    PUSH CS
    PUSH ES
    POP DS
    MOV AX, [BX]
    POP DS
    MOV [0x1243], BX
    PUSH ES
    POP DS
    JMP AX
    ADD AX, 0x0095
    MOV BP, SP
    PUSH CS
    ADD WORD [BP-0x02], 0xFFFB
    PUSH AX
    SUB WORD [BP-0x04], WORD 0xFF91
    RETF
    MOV BX, ES
    INC BX
    NOP
    NOP
    NOP
    NOP
    NOP
    MOV ES, BX
    XOR DI, DI
    REP MOVSW
    REP MOVSW
    ADD SI, WORD 0x007D
    REP MOVSW
    MOV CX, AX
    PUSH DS
    PUSH ES
    POP DS
    POP ES
    MOV BX, 0x0020
    PUSH CS
    POP DX
    MOV [BX+0x02], DX
    ADD CX, 0x0500
    MOV CL, 0xA1
    MOV [BX], CX
    NOP
    NOP
    NOP
    MOV DI, CX
    MOV AX, 0xD429
    STOSW
    MOV AX, 0x1FFF
    STOSW
    MOV AX, 0x1000
    PUSH AX
    POP SS
    MOV DX, 0x0126
    MOV SP, CX
    MOV DI, CX
    ADD DI, 0x0002
    SUB SP, 0x0808
    MOV AX, 0x0500
    MOV BP, 0xBE90
    ADD DI, 0x0001
    NOP
    XOR SI, SI
    MOV CX, 0x0009
    CALL FAR [BX]
    STOSW
    REP MOVSW
    SUB [BX], AX
    MOV DI, [BX]
    ADD DI, 0xFFB0
    MOV CL, 0x09
    MOVSW
    MOVSW
    XOR SI, SI
    DEC DI
    ADD SP, BP
    CALL FAR [BX+SI]
    SUB SP, DX
    CALL FAR [BX+SI]
    MOV CX, AX
    MOV [0x1243], BX
    NOP
    SUB CX, 0x0095
    CALL FAR [BX]
    INT3
    ADD WORD [BP-0x02], 0xFFFB
    CALL FAR [BX]
    INT3
    PUSH AX
    SUB WORD [BP-0x04], 0xFFB0
    RETF
    CALL FAR [BX]
    INC BX
    CALL FAR [BX]
    MOV BX, 0x1000
    CALL FAR [BX]
    MOV BX, 0x0030
    NOP
    ADD CX, 0x2100
    NOP
    POP DX
    MOV [BX+0x02], DX
    ADD CX, 0x0500
    CALL FAR [BX]
    MOV [BX], CX
    ADD CX, 0xFFB0
    CALL FAR [BX]
    MOV AX, 0xD429
    STOSW
    MOV AX, 0x1FFF
    STOSW
    MOV AX, 0x1000
    PUSH AX
    POP SS
    MOV DX, 0x0126
    MOV SP, CX
    MOV DI, CX
    NOP
    NOP
    NOP
    SUB SP, 0x0808
    MOV AX, 0x0500
    MOV BP, 0xBE90
    INC DI
    XOR SI, SI
    MOV CX, 0x0009
    CALL FAR [BX]
    STOSW