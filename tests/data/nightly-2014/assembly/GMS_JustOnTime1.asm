    JMP label1
    INT3
    INT3
    INT3
    PUSH CS
    POP ES
    MOV CX, 0xB111
    PUSH CS
    POP SS
    MOV BX, AX
    MOV SP, AX
    SUB SP, 0x0400
    DB 0x83, 0xC0, 0x04 ;$1 ADD AX, 0x0004
    MOV [BX], AX
    MOV [BX+0x02], CS
    MOV DI, AX
    ADD DI, 0x0400
    NOP
    CALL FAR [BX]
    NOP
    INT3
    INT3
    INT3
label1:
    PUSH CS
    POP ES
    MOV CX, 0xB111
    PUSH CS
    POP SS
    MOV BX, AX
    MOV SP, AX
    SUB SP, 0x0452
    ADD AX, 0x0052
    MOV [BX], AX
    MOV [BX+0x02], CS
    MOV DI, AX
    ADD DI, 0x00B3
    INT 0x86
    ADD DI, 0x0400
    INT 0x86
    ADD DI, 0x0400
    CALL FAR [BX]
    INT3
    INT3
    INT3
    PUSH CS
    POP ES
    MOV CX, 0xB111
    PUSH CS
    POP SS
    MOV BX, AX
    MOV SP, AX
    SUB SP, 0x0400
    DB 0x83, 0xC0, 0x04 ;$1 ADD AX, 0x0004
    MOV [BX], AX
    MOV [BX+0x02], CS
    MOV DI, AX
    ADD DI, 0x0400
    INT 0x86
    ADD DI, 0x0400
    INT 0x86
    ADD DI, 0x0400
    NOP
    CALL FAR [BX]
    NOP
    INT3
    INT3
    INT3
    PUSH CS
    POP ES
    MOV CX, 0xB111
    PUSH CS
    POP SS
    MOV BX, AX
    MOV SP, AX
    SUB SP, 0x0400
    DB 0x83, 0xC0, 0x04 ;$1 ADD AX, 0x0004
    MOV [BX], AX
    MOV [BX+0x02], CS
    MOV DI, AX
    ADD DI, 0x0400
    INT 0x86
    ADD DI, 0x0400
    INT 0x86
    ADD DI, 0x0400
    NOP
    CALL FAR [BX]