    MOV DX, DS
    MOV BX, ES
    MOV DS, BX
    MOV BX, AX
    LODSW
    MOV DS, DX
    MOV DX, AX
    MOV AX, BX
    MOV BX, DX
    MOV WORD [BX+0x21], 0x1E88
    MOV WORD [BX+0x65], 0x1E88
    MOV CX, 0x0014
    MOV DX, SS
    MOV ES, DX
    MOV DI, SP
    SUB DI, 0x0800
    MOV SI, AX
    ADD SI, 0x002F
    JMP label1
    REP MOVSW
    SUB BP, 0x0300
    MOV SP, BP
    ADD SP, 0x1E88
    MOV BYTE [BP+0x04], 0x6E
    MOV WORD [BP+0x02], 0x1FFF
    MOV WORD [BP+0x00], 0xD429
    MOV CX, 0x0014
    MOV SI, AX
    MOV DI, BP
    INC DI
    MOV [BX], BP
    CALL FAR [BX]
label1:
    REP MOVSW
    MOV SI, SP
    MOV DX, SS
    MOV DS, DX
    MOV DX, CS
    MOV ES, DX
    MOV SS, DX
    MOV AX, BX
    SUB AX, 0xC000
    MOV AL, 0xA1
    MOV BX, SI
    SUB BX, 0x0004
    MOV [BX+0x02], CS
    MOV [BX], AX
    SUB SI, 0x0800
    MOV CX, 0x0014
    MOV SP, AX
    ADD SP, 0x1E88
    MOV BP, AX
    MOV BYTE [BP+0x04], 0x6E
    MOV WORD [BP+0x02], 0x1FFF
    MOV WORD [BP+0x00], 0xD429
    MOV DX, AX
    MOV AX, SI
    MOV DI, SP
    INT 0x86
    INT 0x86
    MOV DI, DX
    MOV DX, 0x0078
    INC DI
    CALL FAR [BX]
    OR AL, [0x0435]