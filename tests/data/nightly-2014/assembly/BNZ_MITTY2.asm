    MOV SI, AX
    ADD SI, 0x0037
    PUSH AX
    MOV BX, 0xDE3F
    DB 0x9B, 0x81  ; WAIT
    DS:
    DB 0x0F
    RET 0xCCCC
    DB 0x0F
    TEST DH, DH
    INC WORD [BX+0x0F06]
    RET 0x15B1
loop1:
    SUB AX, 0x4743
    MUL BX
    LOOP loop1+2
    SUB AX, 0xFAEB
    MOV BX, AX
    POP AX
    MOV DI, BX
    ADD DI, 0x004E
    MOV CX, 0x0050
    PUSH CS
    POP ES
    REP MOVSW
    JMP label1
    MOV BL, 0xA3
    INC BH
    MOV CX, 0x0200
loop2:
    MOV WORD [BX], 0xCCCC
    DEC BH
    LOOP loop2+2
    XOR DI, DI
    XOR SI, SI
    MOV CX, 0x0704
    MOV WORD [0xFDEF], 0x1CFF
    MOV WORD [0xFDED], 0xCC29
    PUSH SS
    POP DS
    PUSH CS
    POP SS
    MOV SP, 0x1EDF
    MOV WORD [DI], 0x1DFD
    MOV WORD [DI+0x02], 0x1DFF
    CALL FAR [DI]
    PUSH CS
    POP ES
label1:
    PUSH CS
    POP SS
    MOV BX, 0xCCCC
    DB 0x83, 0xC0, 0x7D ;$1 ADD AX, 0x007D
    MOV DI, AX
    MOV SP, AX
    MOV AX, 0xAB53
    STOSW