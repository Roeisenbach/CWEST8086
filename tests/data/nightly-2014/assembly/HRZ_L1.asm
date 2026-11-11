    PUSH AX
    ADD AX, 0x009F
    STOSW
    POP AX
    MOV BP, SP
    PUSH CS
    ADD WORD [BP-0x02], 0xFFFB
    PUSH AX
    SUB WORD [BP-0x04], WORD 0xFF9C
    RETF
    MOV BX, ES
    INC BX
    MOV ES, BX
    XOR DI, DI
    MOV CL, 0x0C
    MOV SI, AX
    ADD SI, WORD 0x0075
    REP MOVSW
    NOP
    NOP
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
    ADD CX, 0xFFB0
    MOV DI, CX
    MOV AX, 0xD429
    STOSW
    MOV AX, 0x1FFF
    STOSW
    MOV AX, 0x1000
    NOP
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
    XOR SI, SI
    MOV CX, 0x0009
    CALL FAR [BX]
    STOSW
    NRG
    NOP
    NOP
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
    NOP
    NOP
    NOP
    NOP
    INT3
    CALL FAR [BX]
    CALL FAR [BX]
    CALL FAR [BX]
    INT3
    NOP
    NOP
    NOP
    NOP
    NOP
    NOP
    MOV CX, AX
    SUB CX, 0x009F
    MOV BP, SP
    PUSH CS
    ADD WORD [BP-0x02], 0xFFFB
    DB 0x83, 0xC0, 0x16 ;$1 ADD AX, 0x0016
    PUSH AX
    SUB WORD [BP-0x04], 0xFFB0
    RETF
    MOV BX, DS
    INC BX
    MOV DS, BX
    MOV BX, 0x1000
    MOV ES, BX
    MOV BX, 0x0030
    NOP
    ADD CX, 0x2100
    PUSH CS
    POP DX
    MOV [BX+0x02], DX
    ADD CX, 0x0500
    MOV CL, 0xA1
    MOV [BX], CX
    ADD CX, 0xFFB0
    MOV DI, CX
    MOV AX, 0xD429
    STOSW
    MOV AX, 0x1FFF
    STOSW
    MOV AX, 0x1000
    PUSH AX
    POP SS
    MOV DX, 0x0126
    NOP
    MOV SP, CX
    MOV DI, CX
    ADD DI, 0x0002
    SUB SP, 0x0808
    MOV AX, 0x0500
    MOV BP, 0xBE90
    INC DI
    XOR SI, SI
    MOV CX, 0x0009
    CALL FAR [BX]
    STOSW