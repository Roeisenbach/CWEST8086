    STOSW
    PUSH AX
    PUSH AX
    ADD AX, 0x005E
    MOV [0xC781], AX
    MOV BP, SS
    ADD BP, 0x007F
    PUSH CS
    POP SS
    PUSH CS
    POP ES
    MOV CL, 0x4E
    MOV DI, 0x1082
    MOV AX, 0xCCCC
loop1:
    STOSW
    ADD DI, 0x0004
    LOOP loop1
    MOV SI, 0xB60F
    LODSW
    SUB AX, 0xBEC3
    MOV BX, 0xE33F
    MUL BX
    XOR AX, 0xFAEB
    DB 0x83, 0xC0, 0x0F ;$1 ADD AX, 0x000F
    MOV DI, AX
    MOV AX, 0x26FF
    STOSW
    MOV DI, 0xB60F
    STOSW
    MOV DI, 0xB80F
    STOSW
    MOV SP, 0x000E
    MOV SS, BP
    POP AX
    PUSH DS
    POP SS
    MOV BX, AX
    MOV SP, AX
    SUB SP, 0x1024
    ADD AX, 0x005C
    ADD BX, WORD 0x005E
    MOV [BX], AX
    MOV [BX+0x02], CS
    CALL FAR [BX]
    INT 0x86
    JMP $