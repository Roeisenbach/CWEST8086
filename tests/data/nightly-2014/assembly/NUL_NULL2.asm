    JMP label1
    MOV DI, AX
    MOV CL, 0x02
loop1:
    PUSH CS
    POP ES
    ADD DI, 0xFC00
    INT 0x86
    LOOP loop1
    PUSH CS
    POP SS
    PUSH DI
    POP SP
    MOV BL, 0x0A
loop2:
    PUSH CX
    SUB SP, BX
    LOOP loop2
    MOV DI, AX
    MOV CL, 0x02
loop3:
    PUSH CS
    POP ES
    ADD DI, 0xFC00
    INT 0x86
    LOOP loop3
    PUSH CS
    POP SS
    PUSH DI
    POP SP
    MOV BL, 0x0A
loop4:
    PUSH CX
    SUB SP, BX
    LOOP loop4
label1:
    MOV DI, AX
    MOV CL, 0x02
loop5:
    PUSH CS
    POP ES
    ADD DI, 0xFC00
    INT 0x86
    LOOP loop5
    PUSH CS
    POP SS
    PUSH DI
    POP SP
    MOV BL, 0x14
    MOV WORD [0x0000], 0x26FF
    MOV WORD [0x0002], 0x0004
    MOV [0x0004], AX
    ADD WORD [0x0004], WORD 0x006C
    PUSH BX
    MOV BX, AX
    ADD BX, WORD 0x006D
    MOV [BX], SP
    POP BX
loop6:
    PUSH CX
    SUB SP, BX
    LOOP loop6
    MOV SP, 0x1234
    ADD SP, 0x000A
    MOV BX, 0x0014
    JMP loop6
    MOV DI, AX
    MOV CL, 0x02
loop7:
    PUSH CS
    POP ES
    ADD DI, 0xFC00
    INT 0x86
    LOOP loop7
    PUSH CS
    POP SS
    PUSH DI
    POP SP
    MOV BL, 0x0A
loop8:
    PUSH CX
    SUB SP, BX
    LOOP loop8
    INT3
    INT3
    INT3
    INT3
    INT3
    INT3
    INT3
    INT3
    INT3
    INT3
    INT3
    INT3
    INT3
    INT3
    INT3
    INT3
    INT3
    INT3
    INT3
    INT3
    INT3
    INT3
    INT3
    INT3
    INT3
    INT3
    INT3
    INT3
    INT3
    INT3
    INT3
    INT3
    INT3
    INT3
    INT3
    INT3
    INT3
    INT3
    INT3
    INT3
    INT3
    INT3
    INT3
    INT3
    INT3
    INT3
    INT3
    INT3
    INT3
    INT3
    INT3
    INT3
    INT3
    INT3
    INT3
    INT3
    INT3
    INT3
    INT3
    INT3
    INT3
    INT3
    INT3
    INT3
    INT3
    INT3
    INT3
    INT3
    INT3
    INT3
    INT3
    INT3
    INT3
    INT3
    INT3
    INT3
    INT3
    INT3
    INT3
    INT3
    INT3
    INT3
    INT3
    INT3
    INT3
    INT3
    INT3
    INT3
    INT3
    INT3
    INT3
    INT3
    INT3
    INT3
    INT3
    INT3
    INT3
    INT3
    INT3
    INT3
    INT3
    INT3
    INT3
    INT3
    INT3
    INT3
    INT3
    INT3
    INT3
    INT3
    INT3
    INT3
    INT3
    INT3
    INT3
    INT3
    INT3
    INT3
    INT3
    INT3
    INT3
    INT3
    INT3
    INT3
    INT3
    INT3
    INT3
    INT3
    INT3
    INT3
    INT3
    INT3
    INT3
    INT3
    INT3
    INT3
    INT3
    INT3
    INT3
    INT3
    INT3
    INT3
    INT3
    INT3
    INT3
    INT3
    INT3
    INT3
    INT3
    INT3
    INT3
    INT3
    INT3
    INT3
    INT3
    INT3
    INT3
    INT3
    INT3
    INT3
    INT3
    INT3
    INT3
    INT3
    INT3
    INT3
    INT3
    INT3
    INT3
    INT3
    INT3
    INT3
    INT3
    INT3
    INT3
    INT3
    INT3
    INT3
    INT3
    INT3
    INT3
    INT3
    INT3
    INT3
    INT3
    INT3
    INT3
    INT3
    INT3
    INT3
    INT3
    INT3
    INT3
    INT3
    INT3
    INT3
    INT3
    INT3
    INT3
    INT3
    INT3
    INT3
    INT3
    INT3
    INT3
    INT3
    INT3
    INT3
    INT3
    INT3
    INT3
    INT3
    INT3
    INT3
    INT3
    INT3
    INT3
    INT3
    INT3
    INT3
    INT3
    INT3
    INT3
    INT3
    INT3
    INT3
    INT3
    INT3
    INT3
    INT3
    INT3
    INT3
    INT3
    INT3
    INT3
    INT3
    INT3
    INT3
    INT3
    INT3
    INT3
    INT3
    INT3
    INT3
    INT3
    INT3
    INT3
    INT3
    INT3
    INT3
    INT3
    INT3
    INT3
    INT3
    INT3
    INT3
    INT3
    INT3
    INT3
    INT3
    INT3
    INT3
    INT3
    INT3
    INT3
    INT3
    INT3
    INT3
    INT3
    INT3
    INT3
    INT3
    INT3
    INT3
    INT3
    INT3
    INT3
    INT3
    INT3
    INT3
    INT3
    INT3
    INT3
    INT3
    INT3
    INT3
    INT3
    INT3
    INT3
    INT3
    INT3
    INT3
    INT3
    INT3
    INT3
    INT3
    INT3
    INT3
    INT3
    INT3
    INT3
    INT3
    INT3
    INT3
    INT3
    INT3
    INT3
    INT3
    INT3
    INT3
    INT3
    INT3
    INT3
    INT3
    INT3
    INT3
    INT3
    INT3
    INT3
    INT3
    INT3
    INT3
    INT3
    INT3
    INT3
    INT3
    INT3
    INT3
    INT3
    INT3
    INT3
    INT3
    INT3
    INT3
    INT3
    INT3
    INT3
    INT3
    INT3
    INT3
    INT3
    INT3
    INT3
    INT3
    INT3
    INT3
    INT3
    INT3
    INT3
    INT3
    INT3
    INT3
    INT3
    INT3
    INT3
    INT3
    INT3
    INT3
    INT3
    INT3
    INT3
    INT3
    INT3
    INT3
    INT3
    INT3
    INT3
    INT3