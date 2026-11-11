    JMP label1
    MOV CX, DS
    PUSH ES
    POP DS
    LODSW
    MOV DI, AX
    ADD DI, 0x4000
    MOV AX, 0xCCCF
    PUSH CX
    POP ES
    STOSW
    ADD DI, 0x000B
    JMP label2
label1:
    MOV CX, DS
    PUSH ES
    POP DS
    LODSW
    MOV DI, AX
    ADD DI, 0x4000
    MOV AX, 0xCCCF
    PUSH CX
    POP ES
label2:
    STOSW
    ADD DI, 0x000B
    JMP label2
    MOV CX, DS
    PUSH ES
    POP DS
    LODSW
    MOV DI, AX
    ADD DI, 0x4000
    MOV AX, 0xCCCF
    PUSH CX
    POP ES
    STOSW
    ADD DI, 0x000B
    JMP label2
    MOV CX, DS
    PUSH ES
    POP DS
    LODSW
    MOV DI, AX
    ADD DI, 0x4000
    MOV AX, 0xCCCF
    PUSH CX
    POP ES
    STOSW
    ADD DI, 0x000B
    JMP label2