    DB 0xE9, 0x01, 0x00 ;$1 JMP 4
    INT3
    NOP
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
    ADD SI, WORD 0x0075
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
    ADD SI, WORD 0x003F
loop1:
    XOR BYTE [SI], 0x6A
    INC SI
    LOOP loop1
    XOR DX, DX
    ADD AX, 0x00A6
    PUSH AX
    PUSH AX
    MOV DI, 0x0004
    SHL WORD [DI-0x24], 1
    POPF
    POPF
    MOV DX, 0x6A88
    MOV BP, 0x55D2
    MOV SP, DX
    DB 0x69
    DEC DI
    IN AX, 0x6C
    AAD 0x7B
    POPF
    DEC BP
    DB 0x6B
    LODSB
    JMP -12
    ADC WORD [BX+SI+0x2EAD], 0x9523
    DEC BP
    IN AX, 0x6C
    AAD 0x7B
    MOV [0x11BF], CX
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
    MOV SI, 0x00A0
    MOV [BP+0x00], SI
    MOV [BP+SI], SI
label2:
    SUB SP, DX
    PUSH DX
    SUB SP, DX
    PUSH DX
    MOV DI, [BP+SI]
    CMP DI, [BP+0x00]
    JZ label2
    MOV DI, AX
    MOVSW
    JMP AX
    CALL prcd1
prcd1:
    POP AX
    SUB AX, 0x00A9
    PUSH SS
    POP ES
    MOV SI, AX
    ADD SI, 0x00DD
    MOV DI, 0x00ED
    MOV CX, 0x001B
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
    MOV CX, 0x0017
    MOVSW
    REP MOVSW
    MOV CX, 0x0004
    SUB DI, 0x0080
    MOV BX, DI
    SUB AX, 0x6500
    LDS SI, [SI]
    MOV [BX], SI
    MOV [BX+SI], SI
label3:
    ADD SP, [BX]
    POP DI
    POP BP
    CMP BP, 0x10A5
    JNB label4
    SHL BP, CL
    MOV [BP+DI-0x02], SI
label4:
    MOV BP, [BX]
    CMP [BX+SI], BP
    JZ label3
    MOV DS, DX
    MOV DI, AX
    MOVSW
    JMP AX