    MOV DI, AX
    PUSH ES
    MOV BX, 0x07C7
    LEA CX, [DI+word 0x0020]
    LES AX, [DI+word 0x001C]
    MOV DX, 0x0200
    INT 0x87
    POP ES
    XCHG DI, AX
    DB 0x83, 0xC0, 0x3F ;$1 ADD AX, 0x003F
    XOR DI, DI
    JMP label1
    LEA SP, [DI+0x1000]
    PUSH CS
    POP DS
    MOV AX, 0x0704
    MOV WORD [0x1FFB], 0xCC29
    MOV WORD [0x1FFD], 0x1FFF
    MOV DS, DX
    MOV SP, 0x9ABF
    MOV WORD [BX], 0x1FFB
    MOV [BX+0x02], CS
    CALL FAR [BX]
label1:
    JMP label4
    POP AX
    XOR SI, SI
    MOV DX, AX
    PUSH CS
    PUSH ES
    POP DS
    LODSW
    SUB AX, 0x5FE3
    PUSH AX
    PUSH CS
    POP DS
    MOV SI, DX
    ADD SI, 0x0135
    MOV DI, 0x00A0
    MOV CX, 0x0019
    PUSH SS
    POP ES
    REP MOVSW
    PUSH SS
    PUSH DS
    POP ES
    MOV AX, DX
    MOV CX, 0x0024
    MOV SI, AX
    ADD SI, 0x003F
    XOR DX, DX
    ADD AX, 0x0166
    PUSH AX
    PUSH AX
    MOV DI, 0x0004
    MOV BX, 0xB60F
    DIV DI
    SHL DL, 1
    ADD BH, DL
    MOV AX, 0xE33F
    MOV SI, 0x2503
    POP DS
    POP AX
    POP SS
    MOV DX, 0x0013
    MOV DI, AX
    MOV SI, 0x00A0
    MOVSW
    JMP AX
    MOVSW
    MOVSW
    MOVSW
    MOV CX, 0x0015
    REP MOVSW
    MOV SP, AX
    MOV BP, AX
label2:
    ADD AX, 0x9600
    CMP AX, 0x0A70
    JB label2
    SUB BP, 0x003C
    MOV SI, 0x00A0
    MOV [BP+0x00], SI
    MOV [BP+SI], SI
label3:
    SUB SP, DX
    PUSH DX
    SUB SP, DX
    PUSH DX
    MOV DI, [BP+SI]
    CMP DI, [BP+0x00]
    JZ label3
    MOV DI, AX
    MOVSW
    JMP AX
label4:
    MOV SI, AX
    ADD SI, 0x009D
    MOV CX, 0x0009
    REP MOVSW
    PUSH ES
    POP DS
    PUSH AX
    MOV AL, 0xA3
    ADD AX, 0x0600
    STOSW
    JMP label5
    REP MOVSW
    ADD SP, [BX+SI]
    ADD [BX], AX
    LES DI, [BX]
    MOV CL, 0x07
    MOVSW
    XOR SI, SI
    DEC DI
    CALL FAR [BX+SI]
    CALL FAR [BX+SI]
label5:
    POP AX
    XOR SI, SI
    MOV DX, AX
    PUSH CS
    PUSH ES
    POP DS
    LODSW
    SUB AX, 0x5FE3
    PUSH AX
    PUSH CS
    POP DS
    MOV SI, DX
    ADD SI, 0x0135
    MOV DI, 0x00A0
    MOV CX, 0x0019
    PUSH SS
    POP ES
    REP MOVSW
    PUSH SS
    PUSH DS
    POP ES
    MOV AX, DX
    MOV CX, 0x0024
    MOV SI, AX
    ADD SI, 0x003F
    XOR DX, DX
    ADD AX, 0x0166
    PUSH AX
    PUSH AX
    MOV DI, 0x0004
    MOV BX, 0xB60F
    DIV DI
    SHL DL, 1
    ADD BH, DL
    MOV AX, 0xE33F
    PUSH AX
    PUSH BX
    STD
    XOR DI, DI
    MOV AX, 0x1FFF
    MOV DX, 0xCCCC
    MOV BX, 0xCCCC
    MOV CX, BX
    INT 0x87
    CLD
    POP BX
    POP AX
    MOV SI, 0x2503
    MOV CX, 0x004A
    LOOP $
    POP [0xE247]
    MUL WORD [BX]
    ADD SI, AX
    XOR SI, 0xFAEB
    MOV WORD [SI+0x49], 0x26FF
    POP [0xE247]
    MOV [0xE247], CX
    POP DS
    POP AX
    POP SS
    MOV DX, 0x0013
    MOV DI, AX
    MOV SI, 0x00A0
    MOVSW
    JMP AX
    MOVSW
    MOVSW
    MOVSW
    MOV CX, 0x0015
    REP MOVSW
    MOV SP, AX
    MOV BP, AX
label6:
    ADD AX, 0x9600
    CMP AX, 0x0A70
    JB label6
    SUB BP, 0x003C
    MOV SI, 0x00A0
    MOV [BP+0x00], SI
    MOV [BP+SI], SI
label7:
    SUB SP, DX
    PUSH DX
    SUB SP, DX
    PUSH DX
    MOV DI, [BP+SI]
    CMP DI, [BP+0x00]
    JZ label7
    MOV DI, AX
    MOVSW
    JMP AX
    CALL prcd1
prcd1:
    XOR DI, DI
    MOV AX, 0x1FFF
    MOV DX, 0xCCCC
    MOV BX, 0xCCCC
    MOV CX, BX
    INT 0x87
    POP AX
    SUB AX, 0x009A