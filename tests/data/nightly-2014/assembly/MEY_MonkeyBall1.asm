    JMP label2
    DEC WORD [0xCC17]
    MOV SI, AX
    ADD SI, 0x0002
    ADD SI, WORD 0x007A
    MOV CX, 0x000E
    XOR DI, DI
    REP MOVSW
    MOV CX, 0x0040
    INT3
    MOV BP, 0x1000
    MOV DX, AX
    MOV DL, 0xA3
    INT3
    MOV BX, 0x0250
    DB 0xF3, 0xB8
    ADD [BX], AL
    PUSH ES
    POP DS
    MOV WORD [BX+0x02], 0x0FFB
    JMP label1+1
    INT3
    REP MOVSW
    PUSH DS
    POP ES
label1:
    SUB DX, BP
    MOV [BX], DX
    LES DI, [BX]
    MOV SI, 0x0018
    MOVSW
    CLC
    DEC DI
    INT3
    DB 0xF3, 0x01
    RET 0xD489
    XOR SI, SI
    MOV CX, 0x0010
    CALL FAR [BX]
    INT3
    MOVSW
    REP MOVSW
label2:
    PUSH CS
    POP SS
    MOV SI, AX
    ADD SI, WORD 0x007A
    MOV CX, 0x000E
    REP MOVSW
    MOV CX, 0x0040
    MOV BP, 0x1000
    MOV DX, AX
    MOV DL, 0xA3
    MOV BX, 0x0250
    MOV AX, 0x0700
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
    ADD DX, AX
    MOV SP, DX
    XOR SI, SI
    MOV CX, 0x0010
    CALL FAR [BX]
    INT3
    INT3
    PUSH CS
    POP SS
    DEC WORD [BX+DI-0x7E3A]
    DB 0xC6, 0x7A, 0x00, 0xB9 ;$7 MOV BYTE [BP+SI+byte 0x00], 0xB9
    PUSH CS
    ADD BL, DH
    MOVSW
    MOV CX, 0x0040
    HLT
    MOV BP, 0x1000
    INT3
    MOV DX, AX
    MOV DL, 0xA3
    MOV BX, 0x0250
    MOV AX, 0x0700
    CLC
    PUSH ES
    POP DS
    MOV WORD [BX+0x02], 0x0FFB
    JMP label4
    REP MOVSW
    PUSH DS
    POP ES
    REP MOVSW
label4:
    INT3
    SUB DX, BP
    HLT
    MOV [BX], DX
    LES DI, [BX]
    MOV SI, 0x0018
    MOVSW
    DEC DI
    CLC
    ADD DX, AX
    INT3
    INT3
    MOV SP, DX
    XOR SI, SI
    MOV CX, 0x0010
    INT3
    CALL FAR [BX]
    INT3
    INT3
    PUSH CS
    XOR DI, DI
    POP SS
    DEC WORD [BX+DI-0x7E3A]
    DB 0xC6, 0x7A, 0x00, 0xB9 ;$7 MOV BYTE [BP+SI+byte 0x00], 0xB9
    PUSH CS
    ADD BL, DH
    MOVSW
    MOV CX, 0x0040
    HLT
    MOV BP, 0x1000
    INT3
    MOV DX, AX
    MOV DL, 0xA3
    MOV BX, 0x0250
    MOV AX, 0x0700
    CLC
    PUSH CS
    POP SS
    INT3
    MOV SI, AX
    ADD SI, 0x0002
    ADD SI, WORD 0x007A
    MOV CX, 0x000E
    REP MOVSW
    MOV CX, 0x0040
    INT3
    MOV BP, 0x1000
    MOV DX, AX
    MOV DL, 0xA3
    INT3
    MOV BX, 0x0250
    DB 0xF3, 0x31
    DB 0xFF, 0xB8, 0x00, 0x07
    PUSH ES
    POP DS
    MOV WORD [BX+0x02], 0x0FFB
    JMP label5+1
    INT3
    REP MOVSW
    PUSH DS
    POP ES
label5:
    SUB DX, BP
    MOV [BX], DX
    LES DI, [BX]
    MOV SI, 0x0018
    MOVSW
    CLC
    PUSH ES
    XOR DI, DI
    POP DS
    MOV WORD [BX+0x02], 0x0FFB
    JMP label6
    REP MOVSW
    PUSH DS
    POP ES
    INT3
    INT3
label6:
    PUSH CS
    POP SS
    DEC WORD [BX+DI-0x7E3A]
    DB 0xC6, 0x7A, 0x00, 0xB9 ;$7 MOV BYTE [BP+SI+byte 0x00], 0xB9
    PUSH CS
    ADD BL, DH
    MOVSW
    MOV CX, 0x0040
    HLT
    MOV BP, 0x1000
    INT3
    MOV DX, AX
    MOV DL, 0xA3
    MOV BX, 0x0250
    MOV AX, 0x0700
    CLC
    PUSH ES
    POP DS
    MOV WORD [BX+0x02], 0x0FFB
    JMP label7
    REP MOVSW
    PUSH DS
    POP ES
    REP MOVSW
label7:
    INT3
    SUB DX, BP
    HLT
    MOV [BX], DX
    LES DI, [BX]
    MOV SI, 0x0018
    MOVSW
    DEC DI
    CLC
    ADD DX, AX
    INT3
    INT3
    MOV SP, DX
    XOR SI, SI
    MOV CX, 0x0010
    INT3
    CALL FAR [BX]
    REP MOVSW
    INT3
    SUB DX, BP
    HLT
    MOV [BX], DX
    LES DI, [BX]
    MOV SI, 0x0018
    MOVSW
    DEC DI
    CLC
    ADD DX, AX
    INT3
    INT3
    MOV SP, DX
    XOR SI, SI
    MOV CX, 0x0010
    INT3
    MOV BP, 0x1000
    MOV DX, AX
    MOV DL, 0xA3
    INT3
    MOV BX, 0x0250
    DB 0xF3, 0xB8
    ADD [BX], AL
    PUSH ES
    POP DS
    MOV WORD [BX+0x02], 0x0FFB
    JMP label8+1
    INT3
    REP MOVSW
    PUSH DS
    POP ES
label8:
    SUB DX, BP
    MOV [BX], DX
    LES DI, [BX]
    MOV SI, 0x0018
    MOVSW
    CLC
    DEC DI
    INT3
    DB 0xF3, 0x01
    RET 0xD489
    XOR SI, SI
    MOV CX, 0x0010
    CALL FAR [BX]
    INT3
    MOVSW
    REP MOVSW
    REP MOVSW
    INT3
    SUB DX, BP
    HLT
    DB 0x89  ; MOV [BX+SI], AX