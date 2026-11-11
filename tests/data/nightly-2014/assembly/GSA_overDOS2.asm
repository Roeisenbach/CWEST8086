    JMP label3
    POP AX
    XOR SI, SI
    MOV DX, AX
    PUSH CS
    PUSH ES
    POP DS
    LODSW
    SUB AX, 0xBEEF
    PUSH AX
    PUSH CS
    POP DS
    MOV SI, DX
    ADD SI, 0x014D
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
    ADD AX, 0x017E
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
label1:
    ADD AX, 0x9600
    CMP AX, 0x0A70
    JB label1
    SUB BP, 0x003C
label2:
    MOV SI, 0x00A0
    MOV [BP+0x00], SI
    MOV DI, [BP+SI]
    CMP DI, [BP+0x00]
    JZ label2
    MOV DI, AX
    MOVSW
    JMP AX
label3:
    MOV SI, AX
    ADD SI, 0x0094
    MOV CX, 0x0009
    REP MOVSW
    PUSH ES
    POP DS
    PUSH AX
    MOV AL, 0xA3
    ADD AX, 0x0600
    STOSW
    JMP label4
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
label4:
    PUSH CS
    POP DS
    MOV CX, 0x0005
    MOV AX, 0x00A5
loop1:
    MOV WORD [BX], 0x18FF
    ADD BX, 0x0010
    LOOP loop1
    ADD BX, 0xE000
    MOV CX, 0x0005
loop2:
    MOV WORD [BX], 0x18FF
    ADD BX, 0x0010
    LOOP loop2
    POP AX
    XOR SI, SI
    MOV DX, AX
    PUSH CS
    PUSH ES
    POP DS
    LODSW
    SUB AX, 0xBEEF
    PUSH AX
    PUSH CS
    POP DS
    MOV SI, DX
    ADD SI, 0x014D
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
    ADD AX, 0x017E
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
    MOV CX, 0x0026
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
label5:
    ADD AX, 0x9600
    CMP AX, 0x0A70
    JB label5
    SUB BP, 0x003C
    MOV SI, 0x00A0
    MOV [BP+0x00], SI
    MOV [BP+SI], SI
label6:
    SUB SP, DX
    PUSH DX
    SUB SP, DX
    PUSH DX
    MOV DI, [BP+SI]
    CMP DI, [BP+0x00]
    JZ label6
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
    PUSH SS
    POP ES
    MOV SI, AX
    ADD SI, 0x00DD
    MOV DI, 0x00ED
    MOV CX, 0x001E
    REP MOVSW
    PUSH DS
    PUSH ES
    MOV SI, DI
    POP DS
    POP ES
    PUSH CS
    POP SS
    MOV WORD [SI], 0x00ED
    MOV WORD [SI+0x02], 0x1000
    MOV DX, DS
    SUB AX, 0x6500
    MOV SP, AX
    MOV DI, AX
    MOV SI, 0x00ED
    MOVSW
    JMP AX
    MOVSW
    MOVSW
    MOV CX, 0x001A
    MOVSW
    REP MOVSW
    MOV CX, 0x0004
    SUB DI, 0x0080
    MOV BX, DI
    SUB AX, 0x6500
    LDS SI, [SI]
    MOV [BX], SI
    MOV [BX+SI], SI
label7:
    POP DI
    POP BP
    ADD SP, [BX]
    SHL BP, CL
    MOV WORD [BP+DI-0x02], 0x07B1
    MOV WORD [BP+DI], 0xEBA5
    MOV WORD [BP+DI+0x02], 0x00FC
    MOV BP, [BX]
    CMP [BX+SI], BP
    JZ label7
    MOV DS, DX
    MOV DI, AX
    MOVSW
    JMP AX