    PUSH ES
    PUSH CS
    POP ES
    MOV SI, AX
    MOV DI, AX
    ADD DI, 0x5000
    MOV DX, 0xCCCC
    MOV AX, DX
    INT 0x86
    SUB DI, 0xA000
    INT 0x86
    ADD SI, 0x002C
    POP DS
    MOV BX, 0x0050
    MOV [BX+0x02], CS
    MOV [BX], SI
    PUSH CS
    POP SS
    MOV SP, SI
    SUB SP, 0x1024
    CALL FAR [BX]