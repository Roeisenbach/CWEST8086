    NOP
    JMP label1
    MOV DX, 0xEE81
    MOV CX, 0x27FF
    MOV [BX], AX
    ADD WORD [BX], 0x00F7
    MOV [BX+0x05], AX
    MOV AX, 0x5ECC
    INT 0x87
    INT3
    INT 0x86
    INT 0x86
    POP ES
    POP AX
    CLD
    PUSH ES
    MOV WORD [0x2FA1], 0xCCCC
    MOV AL, 0xA2
    NOP
    NOP
    NOP
    ADD AH, 0x01
    POP DS
    PUSH CS
    POP ES
    PUSH CS
    POP SS
    MOV BX, 0x0300
    MOV [BX], AX
    MOV WORD [BX+0x02], 0x1000
    XOR SI, SI
    MOV DI, AX
    MOV SP, DI
    ADD SP, 0x0300
    MOV CX, 0x0007
    MOV AX, 0x1FFF
    STOSW
    DEC DI
    MOV DX, 0x3100
    CMC
    NOP
    NOP
    NOP
    MOV BP, 0x3400
    JMP WORD [BX]
    MOV BX, [BX]
    PUSH AX
    PUSH CS
    POP ES
    STD
    MOV DX, 0xC681
    MOV DI, AX
    MOV CX, 0x27FF
    MOV AX, 0x5ECC
    INT 0x87
    POP AX
    CLD
    INT 0x86
    INT 0x86
    PUSH CS
    POP SS
    MOV SI, 0x082A
    MOV AL, 0xBE
    MOV SP, AX
    MOV BX, 0xD4FF
    NOP
    MOV DI, 0x5552
    MOV DX, 0x5753
    MOV BP, 0xF401
    PUSH BX
    PUSH DI
    PUSH DX
    CALL SP
label1:
    PUSH AX
    PUSH ES
    PUSH CS
    POP ES
    MOV BX, 0xCCCC
    MOV DI, AX
    STD
    MOV DX, 0xEE81
    MOV CX, 0x27FF
    MOV [BX], AX
    ADD WORD [BX], 0x00F7
    JMP label2
    NOP
    NOP
    NOP
    NOP
    NOP
label2:
    MOV [BX+0x05], AX
    MOV AX, 0x5ECC
    INT 0x87
    INT 0x86
    INT 0x86
    POP ES
    POP AX
    CLD
    PUSH ES
    MOV WORD [0x2FA1], 0xCCCC
    MOV AL, 0xA2
    ADD AH, 0x05
    POP DS
    PUSH CS
    POP ES
    PUSH CS
    POP SS
    MOV BX, 0x0300
    MOV [BX], AX
    JMP label3
    INT3
    NOP
    NOP
    NOP
label3:
    MOV WORD [BX+0x02], 0x1000
    XOR SI, SI
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
    MOV BX, [BX]
    PUSH AX
    PUSH CS
    POP ES
    STD
    MOV DX, 0xC681
    MOV DI, AX
    NOP
    MOV CX, 0x27FF
    MOV AX, 0x5ECC
    INT 0x87
    POP AX
    CLD
    INT 0x86
    INT 0x86
    PUSH CS
    POP SS
    MOV SI, 0x082A
    MOV AL, 0xBE
    MOV SP, AX
    MOV BX, 0xD4FF
    MOV DI, 0x5552
    MOV DX, 0x5753
    MOV BP, 0xF401
    PUSH BX
    PUSH DI
    PUSH DX
    PUSH BP
    CALL SP
    NOP
    NOP
    MOV DX, 0xEE81
    MOV CX, 0x27FF
    MOV [BX], AX
    ADD WORD [BX], 0x00F7
    MOV [BX+0x05], AX
    MOV AX, 0x5ECC
    INT 0x87
    INT3
    INT 0x86
    INT 0x86
    POP ES
    POP AX
    CLD
    PUSH ES
    MOV WORD [0x2FA1], 0xCCCC
    MOV AL, 0xA2
    ADD AH, 0x01
    POP DS
    PUSH CS
    NOP
    POP ES
    PUSH CS
    POP SS
    MOV BX, 0x0300
    MOV [BX], AX
    MOV WORD [BX+0x02], 0x1000
    XOR SI, SI
    MOV DI, AX
    MOV SP, DI
    ADD SP, 0x0300
    MOV CX, 0x0007
    MOV AX, 0x1FFF
    STOSW
    DEC DI
    MOV DX, 0x3100
    INT3
    NOP
    NOP
    MOV BP, 0x3400
    JMP WORD [BX]
    NOP
    MOV BX, [BX]
    PUSH AX
    PUSH CS
    POP ES
    STD
    MOV DX, 0xC681
    MOV DI, AX
    MOV CX, 0x27FF
    MOV AX, 0x5ECC
    INT 0x87
    POP AX
    CLD
    INT 0x86
    INT 0x86
    PUSH CS
    POP SS
    NOP
    MOV SI, 0x082A
    MOV AL, 0xBE
    MOV SP, AX
    MOV BX, 0xD4FF
    MOV DI, 0x5552
    NOP
    NOP
    MOV DX, 0x5753
    MOV BP, 0xF401
    PUSH BX
    PUSH DI
    PUSH DX
    PUSH BP
    CALL SP