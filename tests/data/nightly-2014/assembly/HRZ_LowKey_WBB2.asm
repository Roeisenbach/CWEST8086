    NOP
    NOP
    PUSH ES
    POP DS
    MOV BX, [DI]
    PUSH CS
    POP DS
    MOV AX, BX
    JMP BX