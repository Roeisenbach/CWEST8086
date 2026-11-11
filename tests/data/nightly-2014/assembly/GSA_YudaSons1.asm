    JMP label2+1
    PUSH CS
    POP DS
    MOV DI, 0x0022
    MOV SI, AX
    ADD SI, 0x0119
    MOV CX, 0x0039
label1:
    REP MOVSW
    PUSH ES
    POP AX
    PUSH CS
    POP ES
    JMP label5
    MOV DI, BP
    SUB DI, 0x0100
    INT 0x86
    MOV CX, 0x2064
    PUSH CS
    POP DS
    ADD BX, 0x0117
    MOV WORD [BX+0x50], 0x87CD
    PUSH CS
    POP SS
    POP DI
    POP BP
    ADD SP, 0x00ED
    SHL BP, CL
    MOV [BP+DI], BX
    MOV [BP+DI+0x02], CS
    MOV WORD [BP+DI-0x02], 0xEA90
    CMP WORD [BX+0x50], 0x87CD
    DB 0x0F
    TEST AH, AH
    ADD [BX+DI-0x7E23], CL
    RET
    POP DS
    JMP label1+1
    AND AL, [BX+SI]
    MOV DI, BX
    ADD DI, 0x0002
    MOV WORD [BX], 0xA5F3
    MOV DS, AX
    MOV CX, 0x0039
    JMP BX
    MOV CX, 0x2064
    MOV DX, 0x1000
    MOV SS, DX
    MOV CS, DX
    MOV SP, AX
    MOV DX, 0x0700
    DEC DX
    POP DI
    POP BP
    SUB SP, 0x00ED
    SHL BP, CL
    MOV [BP+DI], BX
    MOV [BP+DI+0x02], CS
    MOV WORD [BP+DI-0x02], 0xEA90
    CMP DX, 0x0000
    DB 0x0F
    TEST BX, SP
label2:
    ADD [0x891F], AL
    LDS AX, [BX+DI+0x19C5]
    ADD [BP+DI+word 0x0000], SP
    MOV [0x0002], CS
    CMP WORD [0x0008], 0x0000
    JZ label3
label3:
    CMP WORD [0x000A], 0x0000
    JNZ label4
    PUSH CS
    POP DS
    MOV BP, AX
    XOR AX, 0xABCD
    MOV DH, 0x10
    MOV DL, AL
    MOV AX, BP
    MOV CX, 0x000F
    MOV SI, AX
    ADD SI, 0x00E7
    MOV DI, 0x0022
    REP MOVSW
    PUSH DS
    PUSH ES
    POP DS
    POP ES
    MOV BX, 0x000E
    MOV AL, 0xA3
    MOV [BX], AX
    MOV [BX+0x02], DX
    MOV CX, 0x000F
    MOV SI, 0x0002
    PUSH CS
    POP SS
    MOV SP, AX
    ADD SP, 0x0A00
    MOV BP, DX
    MOV ES, BP
    REP MOVSW
    SUB WORD [BX], 0x1000
    MOV DI, [BX]
    SUB SP, 0x0600
    MOV SI, 0x003C
    MOVSW
    DEC DI
    MOV SI, 0x0022
    MOV CX, 0x0019
    CALL FAR [BX]
    INT3
    CALL FAR [BX]
label4:
    PUSH CS
    POP DS
    MOV DI, 0x0022
    MOV SI, AX
    ADD SI, 0x0119
    MOV CX, 0x0039
    REP MOVSW
    PUSH ES
    POP AX
    PUSH CS
    POP ES
    JMP label5
    MOV DI, BP
    SUB DI, 0x0100
    INT 0x86
label5:
    MOV CX, 0x2064
    PUSH CS
    POP DS
    ADD BX, 0x0117
    MOV WORD [BX+0x50], 0x87CD
    PUSH CS
    POP SS
label6:
    POP DI
    POP BP
    ADD SP, 0x00ED
    SHL BP, CL
    MOV [BP+DI], BX
    MOV [BP+DI+0x02], CS
    MOV WORD [BP+DI-0x02], 0xEA90
    CMP WORD [BX+0x50], 0x87CD
    JZ label6
    MOV BP, BX
    ADD BX, 0xEB1F
    MOV SI, 0x0022
    MOV DI, BX
    ADD DI, 0x0002
    MOV WORD [BX], 0xA5F3
    MOV DS, AX
    MOV CX, 0x0039
    JMP BX
    MOV CX, 0x2064
    MOV DX, 0x1000
    MOV SS, DX
    MOV CS, DX
    MOV SP, AX
    MOV DX, 0x0700
label7:
    DEC DX
    POP DI
    POP BP
    SUB SP, 0x00ED
    SHL BP, CL
    MOV [BP+DI], BX
    MOV [BP+DI+0x02], CS
    MOV WORD [BP+DI-0x02], 0xEA90
    CMP DX, 0x0000
    JNZ label7
    INT3
    POP DI
    POP BP
    ADD SP, 0x00ED
    SHL BP, CL
    MOV [BP+DI], BX
    MOV [BP+DI+0x02], CS
    MOV WORD [BP+DI-0x02], 0xEA90
    CMP WORD [BX+0x50], 0x87CD
    JZ label6
    MOV BP, BX
    ADD BX, 0xEB1F
    MOV SI, 0x0022
    MOV DI, BX
    ADD DI, 0x0002
    MOV WORD [BX], 0xA5F3
    MOV DS, AX
    MOV CX, 0x0039
    JMP BX