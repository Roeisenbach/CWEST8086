    MOV CX, 0x0001
    MOV BX, AX
    ADD DI, 0x0002
    REP STOSW
    PUSH ES
    POP DS
    LODSW
    MOV DI, BX
    CMP BX, AX
    JG label1
    SUB AX, BX
    CMP AX, 0x7FFF
    JG label1
    STD
    JMP label2
label1:
    ADD DI, 0x0026
label2:
    PUSH CS
    POP ES
loop1:
    REP STOSW
    LOOP loop1