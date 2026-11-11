    PUSH CS
    POP SS
    MOV BX, AX
    MOV SP, AX
    ADD AX, 0x0012
    ADD BX, WORD 0x0014
    MOV [BX], AX
    MOV [BX+0x02], CS
    CALL FAR [BX]