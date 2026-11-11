    JMP label1
    PUSH AX
    PUSH ES
    PUSH CS
    POP ES
    MOV DI, AX
    PUSH DI
    STD
    MOV AX, 0xEC29
    MOV DX, 0x1FFF
    MOV BX, 0xCCCC
    MOV CX, BX
    INT 0x87
    SUB DI, 0x0004
    MOV AX, 0xA5A5
    MOV DX, 0xA5A5
    INT 0x86
    POP DI
    ADD DI, 0x00A8
    CLD
    INT 0x86
    POP ES
    POP AX
    XOR SI, SI
    MOV BP, CS
    PUSH ES
    POP DS
    MOV BX, AX
    SUB BX, 0x0020
    ADD BX, 0x0059
    MOV [SI], BX
    MOV WORD [SI+0x02], 0x1002
    CALL FAR [SI]
    MOVSW
    REP MOVSW
    SUB [BX], DX
    MOV DI, [BX]
    MOVSW
    MOVSW
    XOR SI, SI
    MOV SP, DI
    DEC DI
    ADD SP, BP
    MOV CL, 0x08
    CALL FAR [BX]
    SUB SP, AX
    CALL FAR [BX]
    MOV SI, AX
    ADD SI, 0x0041
    MOV DS, BP
    PUSH SS
    POP ES
    XOR DI, DI
    MOV CX, 0x000D
    REP MOVSW
    PUSH SS
    PUSH CS
    POP ES
    POP DS
    MOV SS, BP
    XOR SI, SI
    MOV CL, 0x08
    SUB AX, 0x0200
    MOV AL, 0xC1
    MOV SP, AX
    ADD SP, 0x0007
    MOV BP, AX
    SUB BP, 0x0700
    MOV WORD [BP+0x00], 0xC429
    MOV WORD [BP+0x02], 0x1FFF
    SUB BP, 0x0020
    MOV BX, 0x005A
    MOV [BX+0x02], CS
    MOV [BX], BP
    MOV AX, 0x0007
    MOV DI, BP
    ADD DI, 0x0003
    MOV DX, 0x0700
    MOV BP, 0x070D
    CALL FAR [BX]
label1:
    PUSH AX
    PUSH ES
    PUSH CS
    POP ES
    MOV DI, AX
    PUSH DI
    STD
    MOV AX, 0xEC29
    MOV DX, 0x1FFF
    MOV BX, 0xCCCC
    MOV CX, BX
    INT 0x87
    SUB DI, 0x0004
    MOV AX, 0xA5A5
    MOV DX, 0xA5A5
    INT 0x86
    POP DI
    ADD DI, 0x01FD
    CLD
    INT 0x86
    POP ES
    POP AX
    XOR SI, SI
    MOV BP, CS
    PUSH ES
    POP DS
    MOV BX, AX
    SUB BX, 0x0020
    ADD BX, 0x0105
    MOV [SI], BX
    MOV WORD [SI+0x02], 0x1002
    CALL FAR [SI]
    MOVSW
    REP MOVSW
    SUB [BX], DX
    MOV DI, [BX]
    MOVSW
    MOVSW
    XOR SI, SI
    MOV SP, DI
    DEC DI
    ADD SP, BP
    MOV CL, 0x08
    CALL FAR [BX]
    SUB SP, AX
    CALL FAR [BX]
    MOV SI, AX
    ADD SI, 0x00ED
    MOV DS, BP
    PUSH SS
    POP ES
    XOR DI, DI
    MOV CX, 0x000D
    REP MOVSW
    PUSH SS
    PUSH CS
    POP ES
    POP DS
    MOV SS, BP
    XOR SI, SI
    MOV CL, 0x08
    SUB AX, 0x0200
    MOV AL, 0xC1
    MOV SP, AX
    ADD SP, 0x0007
    MOV BP, AX
    SUB BP, 0x0700
    MOV WORD [BP+0x00], 0xC429
    MOV WORD [BP+0x02], 0x1FFF
    SUB BP, 0x0020
    MOV BX, 0x005A
    MOV [BX+0x02], CS
    MOV [BX], BP
    MOV AX, 0x0007
    MOV DI, BP
    ADD DI, 0x0003
    MOV DX, 0x0700
    MOV BP, 0x070D
    CALL FAR [BX]
    PUSH AX
    PUSH ES
    PUSH CS
    POP ES
    MOV DI, AX
    PUSH DI
    STD
    MOV AX, 0xEC29
    MOV DX, 0x1FFF
    MOV BX, 0xCCCC
    MOV CX, BX
    INT 0x87
    SUB DI, 0x0004
    MOV AX, 0xA5A5
    MOV DX, 0xA5A5
    INT 0x86
    POP DI
    ADD DI, 0x00A8
    CLD
    INT 0x86
    POP ES
    POP AX
    XOR SI, SI
    MOV BP, CS
    PUSH ES
    POP DS
    MOV BX, AX
    SUB BX, 0x0020
    ADD BX, 0x0059
    MOV [SI], BX
    MOV WORD [SI+0x02], 0x1002
    CALL FAR [SI]
    MOVSW
    REP MOVSW
    SUB [BX], DX
    MOV DI, [BX]
    MOVSW
    MOVSW
    XOR SI, SI
    MOV SP, DI
    DEC DI
    ADD SP, BP
    MOV CL, 0x08
    CALL FAR [BX]
    SUB SP, AX
    CALL FAR [BX]
    MOV SI, AX
    ADD SI, 0x0041
    MOV DS, BP
    PUSH SS
    POP ES
    XOR DI, DI
    MOV CX, 0x000D
    REP MOVSW
    PUSH SS
    PUSH CS
    POP ES
    POP DS
    MOV SS, BP
    XOR SI, SI
    MOV CL, 0x08
    SUB AX, 0x0200
    MOV AL, 0xC1
    MOV SP, AX
    ADD SP, 0x0007
    MOV BP, AX
    SUB BP, 0x0700
    MOV WORD [BP+0x00], 0xC429
    MOV WORD [BP+0x02], 0x1FFF
    SUB BP, 0x0020
    MOV BX, 0x005A
    MOV [BX+0x02], CS
    MOV [BX], BP
    MOV AX, 0x0007
    MOV DI, BP
    ADD DI, 0x0003
    MOV DX, 0x0700
    MOV BP, 0x070D
    CALL FAR [BX]