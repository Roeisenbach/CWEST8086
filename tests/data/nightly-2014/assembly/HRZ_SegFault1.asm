    ADD AX, 0x0109
    JMP AX
    ADD [BP+SI+0x69], AL
    DB 0x69
    DB 0x69
    DB 0x69
    DB 0x69
    DB 0x69
    MOV [BX+SI+0x00BB], CL
    ADD [BX+SI-0x7657], DH
    DB 0xC7, 0xC7, 0x05, 0xFF ;$1 MOV DI, 0xFF05
    POP DS
    PUSH ES
    POP DS
    MOV [0x0000], AX
    MOV [0x0002], CS
    PUSH CS
    POP ES
    PUSH CS
    POP SS
    MOV SP, DI
    MOV SI, DI
    INC DI
    MOV AX, 0xAB50
    SUB SP, 0x0600
    JMP SI
    MOV BX, 0x0000
    MOV AL, 0xA9
    MOV DI, AX
    MOV WORD [DI], 0x1FFF
    PUSH ES
    POP DS
    MOV [0x0000], AX
    MOV [0x0002], CS
    PUSH CS
    POP ES
    STOSW
    PUSH AX
    STOSW
    PUSH AX
    STOSW
    PUSH AX
    PUSH CS
    POP SS
    MOV SP, DI
    MOV SI, DI
    INC DI
    MOV AX, 0xAB50
    INT3
    CALL FAR [BX]
    INT3
    CALL FAR [BX]
    INT3
    CALL FAR [BX]
    INT3
    CALL FAR [BX]
    INT3
    CALL FAR [BX]
    INT3
    CALL FAR [BX]
    CALL FAR [BX]
    INT3
    CALL FAR [BX]
    INT3
    CALL FAR [BX]
    INT3
    CALL FAR [BX]
    INT3
    CALL FAR [BX]
    INT3
    CALL FAR [BX]
    SUB SP, 0x0600
    JMP SI
    MOV BX, 0x0000
    MOV AL, 0xA9
    MOV DI, AX
    MOV WORD [DI], 0x1FFF
    PUSH ES
    POP DS
    MOV [0x0000], AX
    MOV [0x0002], CS
    PUSH CS
    POP ES
    PUSH CS
    POP SS
    STOSW
    STOSW
    CALL FAR [BX]
    INT3
    CALL FAR [BX]
    INT3
    CALL FAR [BX]
    INT3
    CALL FAR [BX]
    INT3
    CALL FAR [BX]
    INT3
    CALL FAR [BX]
    MOV SP, DI
    MOV SI, DI
    INC DI
    MOV AX, 0xAB50
    SUB SP, 0x0600
    JMP SI
    MOV BX, 0x0000
    MOV AL, 0xA9
    MOV DI, AX
    MOV WORD [DI], 0x1FFF
    PUSH ES
    POP DS
    MOV [0x0000], AX
    MOV [0x0002], CS
    PUSH CS
    POP ES
    PUSH CS
    POP SS
    MOV SP, DI
    MOV SI, DI
    INC DI
    MOV AX, 0xAB50
    SUB SP, 0x0600
    JMP SI
    MOV BX, 0x0000
    MOV AL, 0xA9
    MOV DI, AX
    MOV WORD [DI], 0x1FFF
    PUSH ES
    POP DS
    MOV [0x0000], AX
    MOV [0x0002], CS
    PUSH CS
    POP ES
    PUSH CS
    POP SS
    MOV SP, DI
    MOV SI, DI
    INC DI
    MOV AX, 0xAB50
    SUB SP, 0x0600
    JMP SI
    MOV BX, 0x0000
    MOV AL, 0xA9
    MOV DI, AX
    MOV WORD [DI], 0x1FFF
    PUSH ES
    POP DS
    MOV [0x0000], AX
    MOV [0x0002], CS
    PUSH CS
    POP ES
    PUSH CS
    POP SS
    MOV SP, DI
    MOV SI, DI
    INC DI
    MOV AX, 0xAB50
    SUB SP, 0x0600
    JMP SI
    MOV BX, 0x0000
    MOV AL, 0xA9
    MOV DI, AX
    MOV WORD [DI], 0x1FFF
    PUSH ES
    POP DS
    MOV [0x0000], AX
    MOV [0x0002], CS
    PUSH CS
    POP ES
    PUSH CS
    POP SS
    MOV SP, DI
    MOV SI, DI
    INC DI
    MOV AX, 0xAB50
    SUB SP, 0x0600
    JMP SI
    MOV BX, 0x0000
    MOV AL, 0xA9
    MOV DI, AX
    MOV WORD [DI], 0x1FFF
    PUSH ES
    POP DS
    MOV [0x0000], AX
    MOV [0x0002], CS
    PUSH CS
    POP ES
    PUSH CS
    POP SS
    MOV SP, DI
    MOV SI, DI
    INC DI
    STOSW
    PUSH AX
    STOSW
    PUSH AX
    STOSW
    PUSH AX
    MOV AX, 0xAB50
    SUB SP, 0x0600
    JMP SI
    STOSW
    STOSW
    STOSW
    STOSW
    STOSW
    STOSW
    STOSW
    MOV BX, 0x0000
    MOV AL, 0xA9
    MOV DI, AX
    CALL FAR [BX]
    INT3
    CALL FAR [BX]
    INT3
    CALL FAR [BX]
    MOV WORD [DI], 0x1FFF
    PUSH ES
    POP DS
    MOV [0x0000], AX
    MOV [0x0002], CS
    PUSH CS
    POP ES
    STOSW
    STOSW
    CALL FAR [BX]
    INT3
    CALL FAR [BX]
    INT3
    CALL FAR [BX]
    INT3
    CALL FAR [BX]
    INT3
    CALL FAR [BX]
    INT3
    CALL FAR [BX]
    PUSH CS
    STOSW
    STOSW
    MOV BX, 0x0000
    MOV AL, 0xA9
    MOV DI, AX
    MOV WORD [DI], 0x1FFF
    PUSH ES
    POP DS
    MOV [0x0000], AX
    MOV [0x0002], CS
    PUSH CS
    POP ES
    PUSH CS
    POP SS
    MOV SP, DI
    MOV SI, DI
    INC DI
    MOV AX, 0xAB50
    SUB SP, 0x0600
    JMP SI
    POP SS
    MOV SP, DI
    MOV SI, DI
    INC DI
    MOV AX, 0xAB50
    SUB SP, 0x0600
    JMP SI
    INT3
    DB 0x69
    DB 0x69
    DB 0x69
    DB 0x69
    STOSW
    STOSW
    STOSW
    MOV [BX+SI+0x6969], CL
    DB 0x69
    DB 0x00, 0x42  ; ADD [BP+SI+byte 0x00], AL