    PUSH ES
    PUSH CS
    POP ES
    PUSH AX
    MOV AX, 0x0789
    MOV DX, 0x4F8C
    INT 0x87
    POP AX
    POP ES
    STOSW
    JMP label1
    INT3
    INT3
    INT3
    INT3
    INT3
    STOSW
    NOP
    NOP
    NOP
    NOP
    PUSH CS
    POP SS
    NOP
    MOV BX, AX
    MOV SP, AX
    ADD AX, 0x004C
    ADD BX, WORD 0x004E
    MOV [BX], AX
    MOV [BX+0x02], DS
    CALL FAR [BX]
    INT3
    INT3
    INT3
    INT3
    INT3
label1:
    NOP
    NOP
    NOP
    NOP
    PUSH CS
    POP SS
    NOP
    MOV BX, AX
    MOV SP, AX
    ADD AX, 0x004C
    ADD BX, WORD 0x004E
    MOV [BX], AX
    MOV [BX+0x02], DS
    CALL FAR [BX]
    INT3
    INT3
    INT3
    INT3
    INT3
    STOSW
    NOP
    NOP
    NOP
    NOP
    PUSH CS
    POP SS
    NOP
    MOV BX, AX
    MOV SP, AX
    ADD AX, 0x004C
    ADD BX, WORD 0x004E
    MOV [BX], AX
    MOV [BX+0x02], DS
    CALL FAR [BX]
    INT3
    INT3
    INT3
    INT3
    INT3