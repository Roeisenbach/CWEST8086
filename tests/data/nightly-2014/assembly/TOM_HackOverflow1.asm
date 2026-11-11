    JMP label1
    TEST [SI], SI
    PUSH ES
    POP DS
    LODSW
    DB 0x83, 0xC0, 0x02 ;$1 ADD AX, 0x0002
    JMP AX
label1:
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