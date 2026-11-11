    JMP label1
    MOV BX, AX
    ADD BX, 0x6212
    MOV BP, 0x0002
    MOV SI, 0x49F6
    MOV DX, BX
    ADD DX, 0x0044
    SUB SP, 0x0018
    MOV CX, 0x0006
    POP DI
    INC DI
    MOV AX, [BX-0x61A9]
    CMP [BX], AX
    JZ label4
    MOV [BX], AX
    ADD DI, BP
    ADD BX, BP
    CMP BX, DX
    JNZ label3
    ADD BX, 0x6165
    ADD DX, 0x61A9
    PUSH DI
    CMP DI, 0x0052
    JGE label5
    ADD SP, 0x0002
    LOOP loop1
    SUB SP, 0x000C
    SUB BX, SI
    SUB DX, SI
    JMP label2
    SUB DX, 0x61ED
    JMP DX
label1:
    MOV BX, 0xC20F
    MOV WORD [BX], 0x1234
    MOV BX, 0xB80F
    MOV WORD [BX], 0x1234
    MOV BX, AX
    ADD BX, 0x6212
    MOV BP, 0x0002
    MOV SI, 0x49F6
    MOV DX, BX
    ADD DX, 0x0044
    SUB SP, 0x0018
label2:
    MOV CX, 0x0006
loop1:
    POP DI
    INC DI
label3:
    MOV AX, [BX-0x61A9]
    CMP [BX], AX
    JZ label4
    MOV [BX], AX
    ADD DI, BP
label4:
    ADD BX, BP
    CMP BX, DX
    JNZ label3
    ADD BX, 0x6165
    ADD DX, 0x61A9
    PUSH DI
    CMP DI, 0x0052
    JGE label5
    ADD SP, 0x0002
    LOOP loop1
    SUB SP, 0x000C
    SUB BX, SI
    SUB DX, SI
    JMP label2
label5:
    SUB DX, 0x61ED
    JMP DX