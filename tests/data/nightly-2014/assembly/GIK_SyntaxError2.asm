    ADD AX, 0x009B
    PUSH AX
    MOV DI, 0x0050
    MOV AX, 0x000A
    STOSW
    MOV AX, 0x1013
    STOSW
    POP AX
    PUSH AX
    SUB AH, 0x07
    STOSW
    MOV CX, 0x003B
    LOOP $
    POP BX
    PUSH BX
    PUSH BX
    MOV AX, [0xC80F]
    MOV CX, [0xC20F]
    MOV SI, [0xC40F]
    MOV [BX+0x00A5], AX
    MOV [BX+0x00A7], CX
    MOV [BX+0x00A9], SI
    MOV DI, BX
    ADD DI, 0x00A5
    PUSH DI
    MOV CX, 0x0001
loop1:
    PUSH CX
    XOR BX, BX
    MOV AX, [DI]
    SUB AX, 0x6418
    MOV BP, AX
    MOV SP, 0x000F
    MOV DI, 0x0010
    MOV SI, 0x54BF
    MOV CX, 0x0004
loop2:
    AND AX, SP
    SUB DX, AX
    AND DX, SP
    ADD BX, DX
    MOV AX, DX
    MUL SI
    SUB BP, AX
    MOV AX, SP
    MUL DI
    MOV SP, AX
    XOR DX, DX
    MOV AX, BP
    LOOP loop2
    MOV SP, 0x07F8
    POP CX
    POP DI
    POP AX
    MOV BYTE [BX+0x54], 0xB8
    MOV [BX+0x55], BX
    MOV BYTE [BX+0x57], 0xBB
    MOV [BX+0x58], AX
    MOV WORD [BX+0x5A], 0xE3FF
    MOV WORD [BX+0x4C], 0x06EB
    ADD DI, 0x0002
    PUSH AX
    PUSH DI
    LOOP loop1
    XOR SI, SI
    MOV SP, 0x07FE
    POP AX
    JMP label1
    XOR DX, DX
    POP CX
    ADD BX, 0x0014
    PUSH AX
    PUSH BX
    MOV DI, BX
    ADD DI, 0x0093
    PUSH DI
    MOV CX, 0x0002
    JMP loop1
label1:
    PUSH CS
    PUSH CS
    PUSH SS
    MOV DX, 0x1000
    ADD DL, AL
    MOV DI, DX
    MOV CL, 0x04
    SHL DX, CL
    MOV CX, DX
    XOR DX, DX
    XOR BX, BX
    MOV DH, AH
    SUB DH, 0x0A
    MOV DL, 0x2C
    MOV BH, DH
    ADD BX, 0xDBA1
    ADD BX, CX
    MOV WORD [BX], 0xEC29
    MOV WORD [BX+0x02], 0x1FFF
    SUB BX, CX
    MOV AX, BX
    POP DS
    POP ES
    MOV BX, 0x0100
    MOV [BX], AX
    MOV [BX+0x02], DI
    MOV WORD [SI], 0xA5F3
    MOV WORD [SI+0x02], 0x2F81
    MOV WORD [SI+0x04], 0x2000
    MOV WORD [SI+0x06], 0x3FC4
    MOV WORD [SI+0x08], 0xA5A5
    MOV WORD [SI+0x0A], 0xF631
    MOV WORD [SI+0x0C], 0x0AB1
    MOV WORD [SI+0x0E], 0xEC81
    MOV WORD [SI+0x10], 0x000E
    MOV WORD [SI+0x12], 0x904F
    MOV WORD [SI+0x14], 0x1FFF
    MOV WORD [SI+0x16], 0xEC29
    MOV WORD [SI+0x18], 0x1FFF
    POP SS
    MOV BP, 0x002B
    MOV SP, DX
    ADD SP, 0x0004
    ADD SP, CX
    MOV DI, AX
    ADD DI, 0x0003
    ADD DI, CX
    MOV CX, 0x000A
    CALL FAR [BX]
    INTO
    INTO
    INTO
    INTO
    INTO
    INTO
    INTO
    INTO
    INTO
    INTO
    INTO
    INTO
    INTO
    INTO
    INTO
    INTO
    INTO
    INTO
    INTO
    INTO
    INTO
    INTO
    INTO
    INTO
    INTO
    INTO
    INTO
    INTO
    INTO
    INTO
    INTO
    INTO
    INTO
    INTO
    INTO
    INTO
    INTO
    INTO
    INTO
    INTO
    INTO
    INTO
    INTO
    INTO
    INTO
    INTO
    INTO
    INTO
    INTO
    INTO
    INTO
    INTO
    INTO
    INTO
    INTO
    INTO
    INTO
    INTO
    INTO
    INTO
    INTO
    INTO
    INTO
    INTO
    INTO
    INTO
    INTO
    INTO
    INTO
    INTO
    INTO
    INTO
    INTO
    INTO
    INTO
    INTO
    INTO
    INTO
    INTO
    INTO
    INTO
    INTO
    INTO
    INTO
    INTO
    INTO
    INTO
    INTO
    INTO
    INTO
    INTO
    INTO
    INTO
    INTO
    INTO
    INTO
    INTO
    INTO
    INTO
    INTO
    INTO
    INTO
    INTO
    INTO
    INTO
    INTO
    INTO
    INTO
    INTO
    INTO
    INTO
    INTO
    INTO
    INTO
    INTO
    INTO
    INTO
    INTO
    INTO
    INTO
    INTO
    INTO
    INTO
    INTO
    INTO
    INTO
    INTO
    INTO
    INTO
    INTO
    INTO
    INTO
    INTO
    INTO
    INTO
    INTO
    INTO
    INTO
    INTO
    INTO
    INTO
    INTO
    INTO
    INTO
    INTO
    INTO
    INTO
    INTO
    INTO
    INTO
    INTO
    INTO
    INTO
    INTO
    INTO
    INTO
    INTO
    INTO
    INTO
    INTO
    INTO
    INTO
    INTO
    INTO
    INTO
    INTO
    INTO
    INTO
    INTO
    INTO
    INTO
    INTO
    INTO
    INTO
    INTO
    INTO
    INTO
    INTO
    INTO
    INTO