    JMP label1
    PUSH AX
    PUSH ES
    PUSH DS
    POP ES
    INT3
    INT3
    INT3
    INT3
    ADD [BX+SI], AL
    ADD [BX+SI], AL
label1:
    PUSH AX
    PUSH ES
    PUSH DS
    POP ES
    ADD AX, 0x0026
    MOV [0x0000], AX
    MOV AX, 0x83AB
    MOV DX, 0x04C7
    MOV BX, 0x26FF
    INT 0x87
    JMP label2
    PUSH DS
    POP ES
    MOV DI, 0xC781
    MOV AX, [0x0000]
    ADD AX, 0x010A
    STOSW
    MOV SI, 0xC20F
    MOV CX, 0x0018
    LOOP $
    MOV CX, 0x0004
loop1:
    LODSW
    SUB AX, 0x12C3
    MOV BX, 0x1B3F
    MUL BX
    SUB AX, 0xFAEB
    DB 0x83, 0xC0, 0x4C ;$1 ADD AX, 0x004C
    MOV DI, AX
    MOV AX, 0x26FF
    STOSW
    ADD SI, 0x01FE
    LOOP loop1
    MOV SI, 0xB60F
    MOV CX, 0x004F
    LOOP $
    MOV CX, 0x0004
    LODSW
    SUB AX, 0xBEC3
    MOV BX, 0xE33F
    MUL BX
    XOR AX, 0xFAEB
    DB 0x83, 0xC0, 0x4C ;$1 ADD AX, 0x004C
    MOV DI, AX
    MOV AX, 0x26FF
    STOSW
    MOV SI, 0xB80F
    LODSW
    SUB AX, 0xBEC3
    MOV BX, 0xE33F
    MUL BX
    XOR AX, 0xFAEB
    DB 0x83, 0xC0, 0x4C ;$1 ADD AX, 0x004C
    MOV DI, AX
    MOV AX, 0x26FF
    STOSW
    XOR DI, DI
    MOV SI, 0xBA0F
    LODSW
    SUB AX, 0xBEC3
    MOV BX, 0xE33F
    MUL BX
    XOR AX, 0xFAEB
    DB 0x83, 0xC0, 0x4C ;$1 ADD AX, 0x004C
    MOV DI, AX
    MOV AX, 0x26FF
    STOSW
    MOV SI, 0xBC0F
    LODSW
    SUB AX, 0xBEC3
    MOV BX, 0xE33F
    MUL BX
    XOR AX, 0xFAEB
    DB 0x83, 0xC0, 0x4C ;$1 ADD AX, 0x004C
    MOV DI, AX
    MOV AX, 0x26FF
    STOSW
    JMP label4
label2:
    POP ES
    POP AX
    XOR BX, BX
    XOR DI, DI
    XOR DX, DX
    MOV AX, AX
    STOSW
    MOV SI, AX
    ADD SI, 0x0108
    MOV CX, 0x000E
    REP MOVSW
    PUSH DS
    PUSH ES
    POP DS
    POP ES
    MOV DI, AX
    ADD DI, 0x0B50
    MOV BX, 0x0020
    MOV [BX], AX
    MOV WORD [BX+0x02], 0x10A5
    PUSH CS
    POP SS
    MOV SP, AX
    ADD SP, 0x0A52
    MOV SI, 0x0004
    MOV CX, 0x000C
    REP MOVSW
    MOV AX, 0xF596
    MOV DX, 0x10A5
    MOV ES, DX
    MOV CS, DX
    REP MOVSW
label3:
    ADD WORD [BX], 0x1A00
    ADD SP, 0x1E00
    CMP [BX], AX
    JA label3
    MOV DI, [BX]
    MOVSW
    DEC DI
    MOV SI, 0x0002
    MOV CX, 0x000C
    CALL FAR [BX]
    CALL FAR [BX]
label4:
    MOV DI, 0xC781
    MOV AX, 0xCCCC
    MOV CX, 0x000F
loop2:
    STOSW
    LOOP loop2
    MOV AX, [0x0000]
    SUB AX, 0x05EF
    MOV [0x0000], AX
    MOV DI, AX
    MOV BX, AX
    MOV AX, 0x00AB
    STOSB
    MOV AX, 0xC781
    STOSW
    MOV AX, 0x0102
    STOSW
    MOV AX, 0xE3FF
    STOSW
    MOV AX, 0xCCCC
    ADD DI, 0x0102
    JMP BX
    PUSH AX
    PUSH ES
    PUSH DS
    POP ES
    INT3
    INT3
    INT3
    INT3
    ADD [BX+SI], AL
    ADD [BX+SI], AL