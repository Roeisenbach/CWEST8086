    JMP label1
    INT3
    INT3
    PUSH AX
    PUSH SI
    PUSH DI
    MOV CX, 0x0046
    LOOP $
    XCHG CX, [0xC20F]
    XCHG BX, [0xC80F]
    XCHG BP, [0xC40F]
    MOV CX, 0x0059
    LOOP $
    XCHG CX, [0xB60F]
    XCHG DI, [0xB80F]
    XCHG SI, [0xBA0F]
    XCHG DX, [0xBC0F]
    POP DI
    POP SI
    POP AX
    MOV DX, 0xCCCC
    MOV SI, AX
    ADD SI, WORD 0x005F
    MOV CX, 0x0010
    PUSH SS
    POP ES
    REP MOVSW
    NOP
    MOV DI, AX
    SUB DI, 0x1400
    MOV WORD [DI], 0xC783
    MOV WORD [DI+0x02], 0xF305
    MOV BYTE [DI+0x04], 0xA5
    PUSH DS
    PUSH SS
    POP DS
    POP SS
    PUSH CS
    POP ES
    MOV SP, AX
    MOV CX, 0x0010
    XOR SI, SI
    JMP DI
    XOR SI, SI
    SUB DI, 0x02B0
    MOV CX, 0x012E
loop1:
    PUSH DX
    SUB SP, 0x0009
    LOOP loop1
    MOV SP, DI
    MOV CX, 0x0010
    SUB DI, 0x1400
    REP MOVSW
    SUB DI, 0x0020
    JMP DI
    INT3
    INT3
    PUSH AX
    PUSH SI
    PUSH DI
    MOV CX, 0x0049
    LOOP $
    XCHG CX, [0xC20F]
    XCHG BX, [0xC80F]
    XCHG BP, [0xC40F]
    MOV CX, 0x005C
    LOOP $
    XCHG CX, [0xB60F]
    XCHG DI, [0xB80F]
    XCHG SI, [0xBA0F]
    XCHG DX, [0xBC0F]
    POP DI
    POP SI
    POP AX
    MOV DX, 0xCCCC
    MOV SI, AX
    ADD SI, WORD 0x005F
    MOV CX, 0x0010
    PUSH SS
    POP ES
    REP MOVSW
    NOP
    MOV DI, AX
    SUB DI, 0x1400
    MOV WORD [DI], 0xC783
    MOV WORD [DI+0x02], 0xF305
    MOV BYTE [DI+0x04], 0xA5
    PUSH DS
    PUSH SS
    POP DS
    POP SS
    PUSH CS
    POP ES
    MOV SP, AX
    MOV CX, 0x0010
    XOR SI, SI
    JMP DI
    XOR SI, SI
    SUB DI, 0x02B0
    MOV CX, 0x012E
loop2:
    PUSH DX
    SUB SP, 0x0009
    LOOP loop2
    MOV SP, DI
    MOV CX, 0x0010
    SUB DI, 0x1400
    REP MOVSW
    SUB DI, 0x0020
    JMP DI
    INT3
    INT3
label1:
    PUSH AX
    PUSH SI
    PUSH DI
    MOV CX, 0x0049
    LOOP $
    XCHG CX, [0xC20F]
    XCHG BX, [0xC80F]
    XCHG BP, [0xC40F]
    MOV CX, 0x005C
    LOOP $
    XCHG CX, [0xB60F]
    XCHG DI, [0xB80F]
    XCHG SI, [0xBA0F]
    XCHG DX, [0xBC0F]
    POP DI
    POP SI
    POP AX
    MOV DX, 0xCCCC
    MOV SI, AX
    ADD SI, WORD 0x0064
    MOV CX, 0x0010
    PUSH SS
    POP ES
    REP MOVSW
    NOP
    MOV DI, AX
    SUB DI, 0x1400
    MOV WORD [DI], 0xC783
    MOV WORD [DI+0x02], 0xF305
    MOV BYTE [DI+0x04], 0xA5
    PUSH DS
    PUSH SS
    POP DS
    POP SS
    PUSH CS
    POP ES
    MOV SP, AX
    MOV CX, 0x0010
    XOR SI, SI
    JMP DI
    XOR SI, SI
    SUB DI, 0x02B0
    MOV CX, 0x012E
loop3:
    PUSH DX
    SUB SP, 0x0009
    LOOP loop3
    MOV SP, DI
    MOV CX, 0x0010
    SUB DI, 0x1400
    REP MOVSW
    SUB DI, 0x0020
    JMP DI
    INT3
    INT3
    PUSH AX
    PUSH SI
    PUSH DI
    MOV CX, 0x0049
    LOOP $
    XCHG CX, [0xC20F]
    XCHG BX, [0xC80F]
    XCHG BP, [0xC40F]
    MOV CX, 0x005C
    LOOP $
    XCHG CX, [0xB60F]
    XCHG DI, [0xB80F]
    XCHG SI, [0xBA0F]
    XCHG DX, [0xBC0F]
    POP DI
    POP SI
    POP AX
    MOV DX, 0xCCCC
    MOV SI, AX
    ADD SI, WORD 0x005F
    MOV CX, 0x0010
    PUSH SS
    POP ES
    REP MOVSW
    NOP
    MOV DI, AX
    SUB DI, 0x1400
    MOV WORD [DI], 0xC783
    MOV WORD [DI+0x02], 0xF305
    MOV BYTE [DI+0x04], 0xA5
    PUSH DS
    PUSH SS
    POP DS
    POP SS
    PUSH CS
    POP ES
    MOV SP, AX
    MOV CX, 0x0010
    XOR SI, SI
    JMP DI
    XOR SI, SI
    SUB DI, 0x02B0
    MOV CX, 0x012E
loop4:
    PUSH DX
    SUB SP, 0x0009
    LOOP loop4
    MOV SP, DI
    MOV CX, 0x0010
    SUB DI, 0x1400
    REP MOVSW
    DB 0x83, 0xEF  ; SUB DI, 0x0000