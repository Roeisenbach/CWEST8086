    STOSW
    JMP label1
    INT3
    MOV BP, AX
    MOV CX, 0x0036
    MOV BX, AX
    ADD BX, 0x00E4
    MOV DX, 0x001D
    ADD DX, BP
loop1:
    MOV WORD [BX], 0xCCCC
    ADD BX, 0x00FA
    LOOP loop1
    JMP DX
    PUSH BX
    MOV DX, 0x0000
    MOV SI, [0xBA0F]
    SUB SI, 0xBEC3
    MOV AX, SI
    MOV BX, 0x0010
    DIV BX
    SUB BX, DX
    MOV DI, 0x00BF
    MOV AX, BX
    MUL DI
    MOV CX, SI
    SUB CX, AX
    SUB BL, CL
    MOV CX, SI
    MOV AX, 0x0CBF
    MUL BX
    SUB CX, AX
    SUB BX, CX
    MOV CX, 0x0004
    SHL BX, CL
    SHR BX, CL
    MOV AX, 0x8CBF
    MUL BX
    SUB SI, AX
    SUB BX, SI
    XOR BX, 0xFAEB
    MOV DX, BP
    ADD DX, 0x00C2
    NOP
    MOV DI, BP
    ADD DI, 0x0002
    MOV [DI], DX
    MOV WORD [BX+0x4C], 0x26FF
    MOV [BX+0x4E], DI
    MOV [DI+0x00C7], BP
    PUSH ES
    POP DS
    MOV SI, 0x0002
    LODSW
    PUSH CS
    POP DS
    MOV [DI+0x00CA], AX
    POP BX
    SUB BX, 0x02EE
    MOV DX, 0x0000
    NOP
label1:
    MOV BP, AX
    MOV CX, 0x0036
    MOV BX, AX
    ADD BX, 0x00E4
    MOV DX, 0x001D
    ADD DX, BP
loop2:
    MOV WORD [BX], 0xCCCC
    ADD BX, 0x00FA
    LOOP loop2
    JMP DX
    PUSH BX
    MOV DX, 0x0000
    MOV SI, [0xBA0F]
    SUB SI, 0xBEC3
    MOV AX, SI
    MOV BX, 0x0010
    DIV BX
    SUB BX, DX
    MOV DI, 0x00BF
    MOV AX, BX
    MUL DI
    MOV CX, SI
    SUB CX, AX
    SUB BL, CL
    MOV CX, SI
    MOV AX, 0x0CBF
    MUL BX
    SUB CX, AX
    SUB BX, CX
    MOV CX, 0x0004
    SHL BX, CL
    SHR BX, CL
    MOV AX, 0x8CBF
    MUL BX
    SUB SI, AX
    SUB BX, SI
    XOR BX, 0xFAEB
    MOV DX, BP
    ADD DX, 0x00C2
    NOP
    MOV DI, BP
    ADD DI, 0x0002
    MOV [DI], DX
    MOV WORD [BX+0x4C], 0x26FF
    MOV [BX+0x4E], DI
    MOV [DI+0x00C7], BP
    PUSH ES
    POP DS
    MOV SI, 0x0002
    LODSW
    PUSH CS
    POP DS
    MOV [DI+0x00CA], AX
    POP BX
    SUB BX, 0x02EE
    MOV DX, 0x0000
    ADD BX, 0x02EE
    PUSH ES
    MOV AX, [DI+0x00CA]
    SUB AX, BX
    MOV CX, 0x00FA
    DIV CX
    MOV CX, AX
    MOV DX, BP
    ADD DX, 0x00A7
    JMP loop2
    MOV DX, 0x0000
    ADD BX, 0x02EE
    MOV AX, BP
    SUB AX, BX
    MOV CX, 0x00FA
    DIV CX
    MOV CX, AX
    MOV DX, BP
    ADD DX, 0x0089
    JMP loop2
    STD
    MOV CX, 0x0005
    LOOP $
    MOV DI, 0x7E1F
    MOV AX, 0x7E1D
    SUB AX, DI
    NEG AX
    SUB AX, 0x0200
    MOV CL, 0x02
    MOV DX, 0x0000
    DIV CX
    MOV CX, AX
    MOV AX, 0xCCCC
loop3:
    STOSW
    LOOP loop3
    INT3
    MOV BP, AX
    MOV CX, 0x0036
    MOV BX, AX
    ADD BX, 0x00E4
    MOV DX, 0x001D
    ADD DX, BP
loop4:
    MOV WORD [BX], 0xCCCC
    ADD BX, 0x00FA
    LOOP loop4
    JMP DX
    PUSH BX
    MOV DX, 0x0000
    MOV SI, [0xBA0F]
    SUB SI, 0xBEC3
    MOV AX, SI
    MOV BX, 0x0010
    DIV BX
    SUB BX, DX
    MOV DI, 0x00BF
    MOV AX, BX
    MUL DI
    MOV CX, SI
    SUB CX, AX
    SUB BL, CL
    MOV CX, SI
    MOV AX, 0x0CBF
    MUL BX
    SUB CX, AX
    SUB BX, CX
    MOV CX, 0x0004
    SHL BX, CL
    SHR BX, CL
    MOV AX, 0x8CBF
    MUL BX
    SUB SI, AX
    SUB BX, SI
    XOR BX, 0xFAEB
    MOV DX, BP
    ADD DX, 0x00C2
    NOP
    MOV DI, BP
    ADD DI, 0x0002
    MOV [DI], DX
    MOV WORD [BX+0x4C], 0x26FF
    MOV [BX+0x4E], DI
    MOV [DI+0x00C7], BP
    PUSH ES
    POP DS
    MOV SI, 0x0002
    LODSW
    PUSH CS
    POP DS
    MOV [DI+0x00CA], AX
    POP BX
    SUB BX, 0x02EE
    MOV DX, 0x0000