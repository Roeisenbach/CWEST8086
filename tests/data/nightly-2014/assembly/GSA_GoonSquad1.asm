    JMP label1
    PUSH AX
    PUSH ES
    PUSH CS
    POP ES
    MOV DX, [0x1243]
    MOV CX, 0x10E1
    DIV CX
    MOV DI, AX
    MOV AX, 0x26FF
    MOV DX, 0x9090
    MOV [0x9090], SI
    INT 0x86
    POP DS
    PUSH CS
    POP ES
    PUSH CS
    POP SS
    MOV AL, 0xA2
    XOR SI, SI
    NOP
    ADD AH, 0x05
    MOV BX, 0x0300
    MOV [BX], AX
    MOV WORD [BX+0x02], 0x1000
    MOV DI, AX
    MOV SP, DI
    ADD SP, 0x0300
    MOV CX, 0x0007
    MOV AX, 0x1FFF
    STOSW
    DEC DI
    MOV DX, 0x3100
    MOV BP, 0x3400
    JMP WORD [BX]
    PUSH DS
    NOP
    POP ES
    MOV DI, AX
    MOV AX, 0xA4A4
    STOSW
    ADD DI, 0x000B
    JMP label2
label1:
    MOV SI, AX
    ADD SI, 0x00AC
    PUSH AX
    PUSH ES
    PUSH CS
    POP ES
    MOV DX, [0x1243]
    MOV CX, 0x10E1
    DIV CX
    MOV DI, AX
    MOV AX, 0x26FF
    MOV DX, 0x9090
    MOV [0x9090], SI
    INT 0x86
    POP DS
    PUSH CS
    POP ES
    PUSH CS
    POP SS
    MOV AL, 0xA2
    ADD AH, 0x05
    MOV BX, 0x0300
    MOV [BX], AX
    MOV WORD [BX+0x02], 0x1000
    XOR SI, SI
    MOV DI, AX
    MOV SP, DI
    ADD SP, 0x0300
    NOP
    NOP
    NOP
    MOV CX, 0x0007
    MOV AX, 0x1FFF
    STOSW
    DEC DI
    MOV DX, 0x3100
    MOV BP, 0x3400
    JMP WORD [BX]
    PUSH DS
    POP ES
    MOV DI, AX
    MOV AX, 0xA4A4
label2:
    STOSW
    ADD DI, 0x000B
    JMP label2