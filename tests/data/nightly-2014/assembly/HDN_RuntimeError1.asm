    JMP label1
    JMP end
    JMP label1
    MOVSW
    MOV BX, 0xA5DE
    JMP label1
    MOV BX, 0xA5DE
    PUSH BX
    MOV BX, 0x89A5
    PUSH BX
    MOV BX, 0xA5A5
    PUSH BX
    MOV BX, 0xA5E7
    PUSH BX
    MOV BX, 0xFF4F
    PUSH BX
    MOV BX, 0xAAD7
    PUSH BX
    MOV BX, 0x01A5
    PUSH BX
    MOV BX, 0xA5A5
    PUSH BX
    MOV BX, 0xA5A5
    PUSH BX
    MOV BX, 0xA5A5
    PUSH BX
    PUSH AX
    PUSH SS
    PUSH CS
    POP ES
    POP DS
    MOV SI, SP
    ADD SI, 0x0002
    MOV BX, SI
    ADD BX, 0x0002
    POP DI
    ADD DI, 0x0111
    CLD
    MOV AL, 0xA3
    INC AH
    MOV DI, AX
    MOV DX, 0x00EE
    MOV AX, 0xA5A5
    STOSW
    SUB DI, 0x0002
    JMP DI
    MOVSW
    MOVSW
    JMP end
    JMP label1
    MOVSW
label1:
    MOV BX, 0xA5DE
    PUSH BX
    MOV BX, 0x89A5
    PUSH BX
    MOV BX, 0xA5A5
    PUSH BX
    MOV BX, 0xA5E7
    PUSH BX
    MOV BX, 0xFF4F
    PUSH BX
    MOV BX, 0xAAD7
    PUSH BX
    MOV BX, 0x01A5
    PUSH BX
    MOV BX, 0xA5A5
    PUSH BX
    MOV BX, 0xA5A5
    PUSH BX
    MOV BX, 0xA5A5
    PUSH BX
    PUSH AX
    PUSH SS
    PUSH CS
    POP ES
    POP DS
    MOV SI, SP
    ADD SI, 0x0002
    MOV BX, SI
    ADD BX, 0x0002
    POP DI
    ADD DI, 0x0111
    CLD
    MOV AL, 0xA3
    INC AH
    MOV DI, AX
    MOV DX, 0x00EE
    MOV AX, 0xA5A5
    STOSW
    SUB DI, 0x0002
    JMP DI
    MOVSW
    MOVSW
    JMP end
    JMP label1
    MOVSW
    MOV BX, 0xA5DE
    PUSH BX
    MOV BX, 0x89A5
    PUSH BX
    MOV BX, 0xA5A5
    PUSH BX
    MOV BX, 0xA5E7
    PUSH BX
    MOV BX, 0xFF4F
    PUSH BX
    MOV BX, 0xAAD7
    PUSH BX
    MOV BX, 0x01A5
    PUSH BX
    MOV BX, 0xA5A5
    PUSH BX
    MOV BX, 0xA5A5
    PUSH BX
    MOV BX, 0xA5A5
    PUSH BX
    PUSH AX
    PUSH SS
    PUSH CS
    POP ES
    POP DS
    MOV SI, SP
    ADD SI, 0x0002
    MOV BX, SI
    ADD BX, 0x0002
    POP DI
    ADD DI, 0x0111
    CLD
    MOV AL, 0xA3
    INC AH
    MOV DI, AX
    MOV DX, 0x00EE
    MOV AX, 0xA5A5
    STOSW
    SUB DI, 0x0002
    JMP DI
    MOVSW
    MOVSW
    JMP end
    JMP label1
    MOVSW
end: