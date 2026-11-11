    DB 0x83, 0xC0, 0x0D ;$1 ADD AX, 0x000D
    MOV BX, 0x0050
    MOV [BX+0x02], CS
    MOV [BX], AX
    PUSH CS
    POP SS
    ADD SP, 0xFFCE
    CALL FAR [BX+SI]
    PUSH ES
    POP DS
    LODSW
    DB 0x83, 0xC0, 0x02 ;$1 ADD AX, 0x0002
    JMP AX
    PUSH ES
    POP DS
    LODSW
    DB 0x83, 0xC0, 0x02 ;$1 ADD AX, 0x0002
    JMP AX