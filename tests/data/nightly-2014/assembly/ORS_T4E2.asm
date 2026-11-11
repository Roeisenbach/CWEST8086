    JMP label1
    PUSH DS
    POP ES
    MOV DI, AX
    ADD DI, 0x1000
    MOV CX, 0x003C
    PUSH AX
    MOV AX, 0xCCCC
    REP STOSW
    POP AX
    JMP label2
    MOV BX, AX
    ADD BX, 0x00FB
    MOV CX, 0x00FB
    SUB CX, 0x00B7
    SHR CX, 1
    ADD CX, 0x0001
    SUB BX, 0x0002
    PUSH WORD [BX]
    LOOP loop1
    MOV BP, SP
    NOP
    MOV SI, AX
    ADD SI, 0x00FB
    ADD SI, 0x1002
    MOV CX, 0x0100
    JMP label4
    MOV WORD [SI], 0xCDCD
    ADD SI, 0x0025
    JMP label3
    ADD AX, 0x2805
    JMP label5
    MOV BX, AX
    MOV DI, AX
    MOV CX, 0x0800
    SUB CX, BP
    SHR CX, 1
    JMP loop3
    POP AX
    STOSW
    SUB DI, 0x00FB
    ADD DI, 0x00B7
    MOV SP, BP
    MOV BX, DI
    MOV AX, DI
    INC DI
    JMP DI
    INT3
    INT3
label1:
    ADD AX, 0x00FB
    STOSW
    SUB AX, 0x00FB
    PUSH DS
    POP ES
    MOV DI, AX
    ADD DI, 0x1000
    MOV CX, 0x003C
    PUSH AX
    MOV AX, 0xCCCC
    REP STOSW
    POP AX
    MOV WORD [0xC80F], 0x3897
    MOV WORD [0xC80F], 0x3897
    BRK
label2:
    MOV BX, AX
    ADD BX, 0x00FB
    MOV CX, 0x00FB
    SUB CX, 0x00B7
    SHR CX, 1
    ADD CX, 0x0001
loop1:
    SUB BX, 0x0002
    PUSH WORD [BX]
    LOOP loop1
    MOV BP, SP
    NOP
    MOV SI, AX
    ADD SI, 0x00FB
    ADD SI, 0x1002
    MOV CX, 0x0100
label3:
    LOOP loop2
    JMP label4
loop2:
    MOV WORD [SI], 0xCDCD
    ADD SI, 0x0025
    JMP label3
label4:
    ADD AX, 0x2805
    BRK
label5:
    MOV BX, AX
    MOV DI, AX
    MOV CX, 0x0800
    SUB CX, BP
    SHR CX, 1
    BRK
loop3:
    POP AX
    STOSW
    LOOP loop3
    SUB DI, 0x00FB
    ADD DI, 0x00B7
    MOV SP, BP
    MOV BX, DI
    MOV AX, DI
    INC DI
    JMP DI
    INT3
    INT3
    PUSH DS
    POP ES
    MOV DI, AX
    ADD DI, 0x1000
    MOV CX, 0x003C
    PUSH AX
    MOV AX, 0xCCCC
    REP STOSW
    POP AX
    JMP label2
    MOV BX, AX
    ADD BX, 0x00FB
    MOV CX, 0x00FB
    SUB CX, 0x00B7
    SHR CX, 1
    ADD CX, 0x0001
    SUB BX, 0x0002
    PUSH WORD [BX]
    LOOP loop1
    MOV BP, SP
    NOP
    MOV SI, AX
    ADD SI, 0x00FB
    ADD SI, 0x1002
    MOV CX, 0x0100
    LOOP loop2
    JMP label4
    MOV WORD [SI], 0xCDCD
    ADD SI, 0x0025
    JMP label3
    ADD AX, 0x2805
    JMP label5
    MOV BX, AX
    MOV DI, AX
    MOV CX, 0x0800
    SUB CX, BP
    SHR CX, 1
    JMP loop3
    POP AX
    STOSW
    LOOP loop3
    SUB DI, 0x00FB
    ADD DI, 0x00B7
    MOV SP, BP
    MOV BX, DI
    MOV AX, DI
    INC DI
    JMP DI
    INT3
    INT3