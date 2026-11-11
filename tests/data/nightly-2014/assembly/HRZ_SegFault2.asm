    ADD AX, 0x00D2
    JMP label1
    CALL FAR [BX]
    CALL FAR [BX]
    CALL FAR [BX]
    CALL FAR [BX]
    CALL FAR [BX]
    REP MOVSW
    SUB AX, 0x0900
    MOV [BX], AX
    MOV DI, AX
    MOV SP, DI
    INC DI
    ADD SP, 0x0002
    PUSH DX
    MOV SI, 0x0014
    ADD SP, 0x0200
    MOV CX, 0x000F
    JMP AX
    MOV SI, AX
    MOV DI, 0x0014
    MOV CX, 0x000F
    REP MOVSW
    PUSH ES
    POP DS
    MOV DX, [0x0000]
    PUSH CS
    POP DS
    MOV AX, DX
    ADD AX, 0x8100
    MOV AL, 0xA3
    MOV BX, AX
    MOV WORD [BX], 0x1FFF
    PUSH ES
    POP DS
    MOV [0x0010], AX
    MOV [0x0012], CS
    MOV DX, 0x1FFF
    PUSH CS
    PUSH CS
    POP ES
    POP SS
    MOV CX, 0x000F
    MOV SI, 0x0014
    MOV DI, AX
    INC DI
    MOV SP, AX
    MOV BX, 0x0010
    ADD SP, 0x1200
    JMP AX
    REP MOVSW
    SUB AX, 0x0900
    MOV [BX], AX
    MOV DI, AX
    MOV SP, DI
    INC DI
    ADD SP, 0x0002
    PUSH DX
    MOV SI, 0x0014
    ADD SP, 0x0200
    MOV CX, 0x000F
    JMP AX
    MOV SI, AX
    MOV DI, 0x0014
    MOV CX, 0x000F
    REP MOVSW
    PUSH ES
    POP DS
    MOV DX, [0x0000]
    PUSH CS
    POP DS
    MOV AX, DX
    ADD AX, 0x8100
    MOV AL, 0xA3
    MOVSW
    MOVSW
    MOVSW
    MOVSW
    MOV BX, AX
    MOV WORD [BX], 0x1FFF
    PUSH ES
    POP DS
    MOV [0x0010], AX
    MOV [0x0012], CS
    MOV DX, 0x1FFF
    PUSH CS
    PUSH CS
    POP ES
    POP SS
    MOV CX, 0x000F
    MOV SI, 0x0014
    MOV DI, AX
    INC DI
    MOV SP, AX
    MOV BX, 0x0010
    ADD SP, 0x1200
    JMP AX
    REP MOVSW
    SUB AX, 0x0900
    MOV [BX], AX
    MOV DI, AX
    MOV SP, DI
    INC DI
    ADD SP, 0x0002
    PUSH DX
    MOV SI, 0x0014
    ADD SP, 0x0200
    MOV CX, 0x000F
    JMP AX
label1:
    MOV SI, AX
    MOV DI, 0x0014
    MOV CX, 0x000F
    REP MOVSW
    PUSH ES
    POP DS
    MOV DX, [0x0000]
    PUSH CS
    POP DS
    MOV AX, DX
    ADD AX, 0x8100
    MOV AL, 0xA3
    MOV BX, AX
    MOV WORD [BX], 0x1FFF
    PUSH ES
    POP DS
    MOV [0x0010], AX
    MOV [0x0012], CS
    MOV DX, 0x1FFF
    PUSH CS
    PUSH CS
    POP ES
    POP SS
    MOV CX, 0x000F
    MOV SI, 0x0014
    MOV DI, AX
    INC DI
    MOV SP, AX
    MOV BX, 0x0010
    ADD SP, 0x1200
    JMP AX
    REP MOVSW
    SUB AX, 0x0900
    MOV [BX], AX
    MOV DI, AX
    MOV SP, DI
    INC DI
    ADD SP, 0x0002
    PUSH DX
    MOV SI, 0x0014
    ADD SP, 0x0200
    MOV CX, 0x000F
    JMP AX
    MOV SI, AX
    MOV DI, 0x0014
    MOV CX, 0x000F
    REP MOVSW
    PUSH ES
    POP DS
    MOV DX, [0x0000]
    PUSH CS
    POP DS
    MOV AX, DX
    ADD AX, 0x8100
    MOV AL, 0xA3
    MOV BX, AX
    MOV WORD [BX], 0x1FFF
    PUSH ES
    POP DS
    MOV [0x0010], AX
    MOV [0x0012], CS
    MOV DX, 0x1FFF
    MOVSW
    MOVSW
    MOVSW
    MOVSW
    PUSH CS
    PUSH CS
    POP ES
    POP SS
    MOV CX, 0x000F
    MOV SI, 0x0014
    MOV DI, AX
    INC DI
    MOV SP, AX
    MOV BX, 0x0010
    ADD SP, 0x1200
    JMP AX
    REP MOVSW
    SUB AX, 0x0900
    MOV [BX], AX
    MOV DI, AX
    MOV SP, DI
    INC DI
    ADD SP, 0x0002
    PUSH DX
    MOV SI, 0x0014
    ADD SP, 0x0200
    MOV CX, 0x000F
    JMP AX
    MOV SI, AX
    MOV DI, 0x0014
    MOV CX, 0x000F
    REP MOVSW
    PUSH ES
    POP DS
    MOV DX, [0x0000]
    PUSH CS
    POP DS
    MOV AX, DX
    ADD AX, 0x8100
    MOV AL, 0xA3
    MOV BX, AX
    MOV WORD [BX], 0x1FFF
    PUSH ES
    POP DS
    MOV [0x0010], AX
    MOV [0x0012], CS
    MOV DX, 0x1FFF
    PUSH CS
    PUSH CS
    POP ES
    POP SS
    MOV CX, 0x000F
    MOV SI, 0x0014
    MOV DI, AX
    INC DI
    MOV SP, AX
    MOV BX, 0x0010
    ADD SP, 0x1200
    JMP AX
    CALL FAR [BX]
    CALL FAR [BX]
    CALL FAR [BX]
    CALL FAR [BX]
    CALL FAR [BX]