    STOSW
    SUB DI, 0x0002
    MOV BX, AX
    STOSW
    PUSH ES
    POP DS
    ADD SI, 0x0002
    LODSW
    PUSH AX
    MOV DI, BX
    CMP BX, AX
    JG label1
    MOV DX, 0x8000
    PUSH DX
    STD
    BRK
    JMP label2
    JMP end
label1:
    MOV DX, 0x0000
    PUSH DX
    ADD DI, 0x005E
label2:
    SUB BX, AX
    TEST BX, 0x8000
    JZ label3
    NEG BX
label3:
    MOV AX, 0xFFFF
    SUB AX, BX
    MOV BX, 0x0002
    DIV BX
    MOV CX, AX
    SUB CX, 0x01F4
    MOV AX, 0xCCCC
    PUSH CS
    POP ES
    REP STOSW
    POP DX
    CMP DX, 0x0000
    JNZ label4
    ADD DI, 0x03E8
    JMP label5
label4:
    SUB DI, 0x03E8
label5:
    POP BX
    ADD BX, 0x001E
    JMP BX
    STOSW
end: