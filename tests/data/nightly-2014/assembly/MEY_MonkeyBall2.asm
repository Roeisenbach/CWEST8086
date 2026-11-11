    JMP label4
    INT3
    PUSH ES
    PUSH CS
    POP ES
    INT3
    PUSH AX
    INT 0x86
    INT 0x86
    MOV AX, 0xCCCC
    MOV DX, 0xCCCC
    MOV CX, 0xCCCC
    MOV BX, 0xCCCC
    INT 0x87
    POP AX
    POP ES
    PUSH CS
    POP SS
    MOV SI, AX
    ADD SI, 0x0159
    MOV CX, 0x000E
    REP MOVSW
    MOV CX, 0x0040
    INT3
    INT3
    MOV BP, 0x1000
    MOV DX, AX
    MOV DL, 0xA3
    MOV BX, 0x0350
    MOV AX, 0x0700
    PUSH ES
    POP DS
    MOV WORD [BX+0x02], 0x0FFB
    JMP label1
    REP MOVSW
    PUSH DS
    POP ES
    SUB DX, BP
label1:
    MOV [BX], DX
    LES DI, [BX]
    DB 0x69
    MOV SI, 0x0018
    MOVSW
    DEC DI
    ADD DX, AX
    MOV SP, DX
    XOR SI, SI
    MOV CX, 0x0010
    CALL FAR [BX]
    INT3
    PUSH ES
    PUSH CS
    POP ES
    PUSH AX
    INT 0x86
    INT 0x86
    MOV AX, 0xFAB2
    INT3
    MOV DX, 0xA5F3
    MOV CX, 0xCCCC
    MOV BX, 0xCCCC
    INT 0x87
    POP AX
    POP ES
    INT3
    INT3
    INT3
    PUSH CS
    POP SS
    MOV SI, AX
    ADD SI, 0x0159
    MOV CX, 0x000E
    REP MOVSW
    MOV CX, 0x0040
    MOV BP, 0x1000
    MOV DX, AX
    MOV DL, 0xA3
    MOV BX, 0x0350
    MOV AX, 0x0700
    PUSH ES
    POP DS
    INT3
    MOV WORD [BX+0x02], 0x0FFB
    JMP label2
    REP MOVSW
    PUSH DS
    POP ES
    SUB DX, BP
label2:
    MOV [BX], DX
    LES DI, [BX]
    MOV SI, 0x0018
    MOVSW
    DEC DI
    ADD DX, AX
    MOV SP, DX
    XOR SI, SI
    MOV CX, 0x0010
    CALL FAR [BX]
    PUSH ES
    PUSH CS
    POP ES
    PUSH AX
    INT3
    INT 0x86
    INT 0x86
    MOV AX, 0xFAB2
    MOV DX, 0xA5F3
    MOV CX, 0xCCCC
    MOV BX, 0xCCCC
    INT 0x87
    POP AX
    POP ES
    PUSH CS
    POP SS
    MOV SI, AX
    ADD SI, 0x0159
    MOV CX, 0x000E
    REP MOVSW
    MOV CX, 0x0040
    MOV BP, 0x1000
    MOV DX, AX
    MOV DL, 0xA3
    MOV BX, 0x0350
    MOV AX, 0x0700
    INT3
    DB 0x69
    PUSH ES
    POP DS
    MOV WORD [BX+0x02], 0x0FFB
    JMP label3
    REP MOVSW
    PUSH DS
    POP ES
    SUB DX, BP
label3:
    MOV [BX], DX
    LES DI, [BX]
    MOV SI, 0x0018
    MOVSW
    DEC DI
    DB 0x69
    DB 0x69
    ADD DX, AX
    MOV SP, DX
    XOR SI, SI
    MOV CX, 0x0010
    CALL FAR [BX]
label4:
    PUSH ES
    PUSH CS
    POP ES
    PUSH AX
    INT 0x86
    INT 0x86
    MOV AX, 0xC781
    MOV DX, 0x01FD
    MOV CX, 0xCCCC
    MOV BX, 0xCCCC
    INT 0x87
    POP AX
    POP ES
    PUSH CS
    POP SS
    MOV SI, AX
    ADD SI, 0x0159
    MOV CX, 0x000E
    REP MOVSW
    MOV CX, 0x0040
    MOV BP, 0x1000
    MOV DX, AX
    MOV DL, 0xA3
    MOV BX, 0x0350
    MOV AX, 0x0700
    PUSH ES
    POP DS
    MOV WORD [BX+0x02], 0x0FFB
    JMP label5
    REP MOVSW
    PUSH DS
    POP ES
    SUB DX, BP
label5:
    MOV [BX], DX
    LES DI, [BX]
    MOV SI, 0x0018
    MOVSW
    DEC DI
    ADD DX, AX
    MOV SP, DX
    XOR SI, SI
    MOV CX, 0x0010
    CALL FAR [BX]
    PUSH ES
    PUSH CS
    POP ES
    DB 0x69
    PUSH AX
    INT 0x86
    INT 0x86
    MOV AX, 0x86CD
    MOV DX, 0x815F
    MOV CX, 0xCCCC
    DB 0x69
    DB 0x69
    MOV BX, 0xCCCC
    INT 0x87
    POP AX
    POP ES
    PUSH CS
    POP SS
    MOV SI, AX
    ADD SI, 0x0159
    MOV CX, 0x000E
    REP MOVSW
    MOV CX, 0x0040
    MOV BP, 0x1000
    MOV DX, AX
    MOV DL, 0xA3
    MOV BX, 0x0350
    MOV AX, 0x0700
    PUSH ES
    POP DS
    MOV WORD [BX+0x02], 0x0FFB
    JMP label6+1
    REP MOVSW
    DB 0x69
    PUSH DS
    POP ES
label6:
    SUB DX, BP
    MOV [BX], DX
    LES DI, [BX]
    MOV SI, 0x0018
    MOVSW
    DEC DI
    ADD DX, AX
    MOV SP, DX
    XOR SI, SI
    MOV CX, 0x0010
    CALL FAR [BX]
    PUSH ES
    PUSH CS
    POP ES
    PUSH AX
    INT 0x86
    INT 0x86
    MOV AX, 0xFAB2
    MOV DX, 0xA5F3
    MOV CX, 0xCCCC
    DB 0x69
    MOV BX, 0xCCCC
    INT 0x87
    POP AX
    POP ES
    PUSH CS
    POP SS
    MOV SI, AX
    ADD SI, 0x0159
    MOV CX, 0x000E
    REP MOVSW
    MOV CX, 0x0040
    MOV BP, 0x1000
    MOV DX, AX
    MOV DL, 0xA3
    DB 0xBB  ; MOV BX, 0x0000