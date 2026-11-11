    JMP label1
    PUSH ES
    PUSH DS
    PUSH ES
    POP DS
    DEC AX
    MOV [0x00C8], AX
    INC AX
    MOV BX, [0x00CC]
    POP DS
    PUSH AX
    MOV DI, AX
    MOV [DI+0x0110], BX
    MOV BX, AX
    MOV [BX+0x010D], AX
    PUSH DS
    POP ES
    PUSH AX
    MOV CX, 0x00B0
    LOOP $
    MOV CX, 0x0004
    MOV SI, 0xBC0F
    MOV DX, [SI]
    SUB DX, 0xBEC3
    MOV AX, 0xE33F
    MUL DX
    XOR AX, 0xFAEB
    MOV DI, AX
    ADD DI, 0x0047
    POP DX
    PUSH DX
    ADD DX, 0x0053
    SUB DX, DI
    MOV [DI+0x01], DX
    MOV BYTE [DI], 0xE9
    SUB SI, 0x0200
    POP DX
    JMP label3
    MOV BX, 0x1234
    MOV DX, 0x6789
    ADD DI, 0x0125
    MOV CX, DI
    SUB CX, BX
    CMP CX, 0x0100
    MOV CX, DI
    SUB CX, DX
    CMP CX, 0x0100
    MOV WORD [DI], 0xCCCC
    JMP label2
    POP AX
    MOV BX, AX
    MOV WORD [BX], 0x1FFF
    DEC BX
    MOV BYTE [BX], 0xAB
    POP DS
    MOV [0x00F0], BX
    MOV [0x00FA], BX
    MOV [0x00FC], CS
    PUSH CS
    POP SS
    PUSH CS
    POP ES
    MOV DI, AX
    SUB DI, 0x04F1
    MOV SP, DI
    INT 0x86
    MOV DL, 0xAB
    MOV DH, 0x4F
    MOV AL, 0x8B
    MOV AH, 0x27
    MOV BX, 0xAAAA
    INT 0x87
    ADD DI, 0x07F1
    INT 0x86
    MOV AX, 0xCCCC
    MOV BX, 0x00FA
    CALL FAR [BX]
label1:
    PUSH ES
    PUSH DS
    PUSH ES
    POP DS
    DEC AX
    MOV [0x00C8], AX
    INC AX
    MOV BX, [0x00CC]
    POP DS
    PUSH AX
    MOV DI, AX
    MOV [DI+0x0110], BX
    MOV BX, AX
    MOV [BX+0x010D], AX
    PUSH DS
    POP ES
    PUSH AX
    MOV CX, 0x00B0
    LOOP $
    MOV CX, 0x0004
    MOV SI, 0xBC0F
loop1:
    MOV DX, [SI]
    SUB DX, 0xBEC3
    MOV AX, 0xE33F
    MUL DX
    XOR AX, 0xFAEB
    MOV DI, AX
    ADD DI, 0x0047
    POP DX
    PUSH DX
    ADD DX, 0x00B6
    ADD DX, 0x0053
    SUB DX, DI
    MOV [DI+0x01], DX
    MOV BYTE [DI], 0xE9
    SUB SI, 0x0200
    LOOP loop1
    POP DX
    JMP label3
    MOV BX, 0x1234
    MOV DX, 0x6789
label2:
    ADD DI, 0x0125
    MOV CX, DI
    SUB CX, BX
    CMP CX, 0x0100
    JB label2
    MOV CX, DI
    SUB CX, DX
    CMP CX, 0x0100
    JB label2
    MOV WORD [DI], 0xCCCC
    JMP label2
label3:
    POP AX
    MOV BX, AX
    MOV WORD [BX], 0x1FFF
    DEC BX
    MOV BYTE [BX], 0xAB
    POP DS
    MOV [0x00F0], BX
    MOV [0x00FA], BX
    MOV [0x00FC], CS
    PUSH CS
    POP SS
    PUSH CS
    POP ES
    MOV DI, AX
    SUB DI, 0x04F1
    MOV SP, DI
    INT 0x86
    MOV DL, 0xAB
    MOV DH, 0x4F
    MOV AL, 0x8B
    MOV AH, 0x27
    MOV BX, 0xAAAA
    INT 0x87
    ADD DI, 0x07F1
    INT 0x86
    MOV AX, 0xCCCC
    MOV BX, 0x00FA
    CALL FAR [BX]
    PUSH ES
    PUSH DS
    PUSH ES
    POP DS
    DEC AX
    MOV [0x00C8], AX
    INC AX
    MOV BX, [0x00CC]
    POP DS
    PUSH AX
    MOV DI, AX
    MOV [DI+0x0110], BX
    MOV BX, AX
    MOV [BX+0x010D], AX
    PUSH DS
    POP ES
    PUSH AX
    MOV CX, 0x00B0
    LOOP $
    MOV CX, 0x0004
    MOV SI, 0xBC0F
    MOV DX, [SI]
    SUB DX, 0xBEC3
    MOV AX, 0xE33F
    MUL DX
    XOR AX, 0xFAEB
    MOV DI, AX
    ADD DI, 0x0047
    POP DX
    PUSH DX
    ADD DX, 0x0053
    SUB DX, DI
    MOV [DI+0x01], DX
    MOV BYTE [DI], 0xE9
    SUB SI, 0x0200
    POP DX
    JMP label3
    MOV BX, 0x1234
    MOV DX, 0x6789
    ADD DI, 0x0125
    MOV CX, DI
    SUB CX, BX
    CMP CX, 0x0100
    MOV CX, DI
    SUB CX, DX
    CMP CX, 0x0100
    MOV WORD [DI], 0xCCCC
    JMP label2
    POP AX
    MOV BX, AX
    MOV WORD [BX], 0x1FFF
    DEC BX
    MOV BYTE [BX], 0xAB
    POP DS
    MOV [0x00F0], BX
    MOV [0x00FA], BX
    MOV [0x00FC], CS
    PUSH CS
    POP SS
    PUSH CS
    POP ES
    DB 0x89  ; MOV [BX+SI], AX