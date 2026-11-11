    JMP label1
    INT3
    PUSH CS
    POP SS
    PUSH CS
    POP ES
    INT3
    INT3
    INT3
    PUSH CS
    POP SS
    PUSH CS
    POP ES
    INT3
    INT3
    MOV CX, 0x5153
    MOV BX, 0xD4FF
    DB 0x83, 0xC0, 0x0F ;$1 ADD AX, 0x000F
    MOV SP, AX
    PUSH BX
    PUSH CX
    CALL SP
    PUSH CS
    POP SS
    PUSH CS
    POP ES
    MOV CX, 0x5153
    MOV BX, 0xD4FF
    DB 0x83, 0xC0, 0x0F ;$1 ADD AX, 0x000F
    MOV SP, AX
    PUSH BX
    PUSH CX
    CALL SP
    PUSH CS
    POP SS
    PUSH CS
    POP ES
    INT3
    MOV CX, 0x5153
    MOV BX, 0xD4FF
    DB 0x83, 0xC0, 0x0F ;$1 ADD AX, 0x000F
    MOV SP, AX
    INT3
    PUSH CX
    CALL SP
    MOV CX, 0x5153
    MOV BX, 0xD4FF
    DB 0x83, 0xC0, 0x0F ;$1 ADD AX, 0x000F
    MOV SP, AX
    PUSH BX
    PUSH CX
    CALL SP
    PUSH CS
    POP SS
    PUSH CS
    POP ES
    MOV CX, 0x5153
    MOV BX, 0xD4FF
    DB 0x83, 0xC0, 0x0F ;$1 ADD AX, 0x000F
    MOV SP, AX
    PUSH BX
    PUSH CX
    CALL SP
    PUSH CS
    POP SS
    PUSH CS
    POP ES
    INT3
    MOV CX, 0x5153
    MOV BX, 0xD4FF
    DB 0x83, 0xC0, 0x0F ;$1 ADD AX, 0x000F
    MOV SP, AX
    INT3
    PUSH CX
    CALL SP
    INT3
    PUSH CS
    POP SS
    PUSH CS
    POP ES
    INT3
    INT3
    MOV CX, 0x5153
    MOV BX, 0xD4FF
    DB 0x83, 0xC0, 0x0F ;$1 ADD AX, 0x000F
    MOV SP, AX
    PUSH BX
    PUSH CX
    CALL SP
    PUSH CS
    POP SS
    PUSH CS
    POP ES
    MOV CX, 0x5153
    MOV BX, 0xD4FF
    DB 0x83, 0xC0, 0x0F ;$1 ADD AX, 0x000F
    MOV SP, AX
    PUSH BX
    PUSH CX
    CALL SP
    PUSH CS
    POP SS
    PUSH CS
    POP ES
    INT3
    MOV CX, 0x5153
    MOV BX, 0xD4FF
    DB 0x83, 0xC0, 0x0F ;$1 ADD AX, 0x000F
    MOV SP, AX
    INT3
    PUSH CX
    CALL SP
    INT3
    PUSH CS
    POP SS
    PUSH CS
    POP ES
    INT3
    INT3
    MOV CX, 0x5153
    MOV BX, 0xD4FF
    DB 0x83, 0xC0, 0x0F ;$1 ADD AX, 0x000F
    MOV SP, AX
    PUSH BX
    PUSH CX
    CALL SP
    PUSH CS
    POP SS
    PUSH CS
    POP ES
    MOV CX, 0x5153
    MOV BX, 0xD4FF
    DB 0x83, 0xC0, 0x0F ;$1 ADD AX, 0x000F
    MOV SP, AX
    PUSH BX
    PUSH CX
    CALL SP
    PUSH CS
    POP SS
    PUSH CS
    POP ES
    INT3
    MOV CX, 0x5153
    MOV BX, 0xD4FF
    DB 0x83, 0xC0, 0x0F ;$1 ADD AX, 0x000F
    MOV SP, AX
    INT3
    PUSH CX
    CALL SP
label1:
    PUSH CS
    POP SS
    PUSH CS
    POP ES
    MOV CX, 0x5153
    MOV BX, 0xD4FF
    DB 0x83, 0xC0, 0x0F ;$1 ADD AX, 0x000F
    MOV SP, AX
    PUSH BX
    PUSH CX
    CALL SP
    PUSH CS
    INT3
    POP SS
    PUSH CS
    POP ES
    MOV CX, 0x5153
    MOV BX, 0xD4FF
    DB 0x83, 0xC0, 0x0F ;$1 ADD AX, 0x000F
    MOV SP, AX
    PUSH BX
    PUSH CX
    CALL SP
    INT3
    PUSH CS
    POP SS
    PUSH CS
    POP ES
    MOV CX, 0x5153
    MOV BX, 0xD4FF
    DB 0x83, 0xC0, 0x0F ;$1 ADD AX, 0x000F
    MOV SP, AX
    PUSH BX
    PUSH CX
    CALL SP
    INT3
    PUSH CS
    POP SS
    PUSH CS
    POP ES
    INT3
    PUSH CS
    POP SS
    PUSH CS
    POP ES
    INT3
    INT3
    MOV CX, 0x5153
    MOV BX, 0xD4FF
    DB 0x83, 0xC0, 0x0F ;$1 ADD AX, 0x000F
    MOV SP, AX
    PUSH BX
    PUSH CX
    CALL SP
    PUSH CS
    POP SS
    PUSH CS
    POP ES
    MOV CX, 0x5153
    MOV BX, 0xD4FF
    DB 0x83, 0xC0, 0x0F ;$1 ADD AX, 0x000F
    MOV SP, AX
    PUSH BX
    PUSH CX
    CALL SP
    PUSH CS
    POP SS
    PUSH CS
    POP ES
    INT3
    MOV CX, 0x5153
    MOV BX, 0xD4FF
    DB 0x83, 0xC0, 0x0F ;$1 ADD AX, 0x000F
    MOV SP, AX
    INT3
    PUSH CX
    CALL SP
    INT3
    PUSH CS
    POP SS
    PUSH CS
    POP ES
    INT3
    INT3
    MOV CX, 0x5153
    MOV BX, 0xD4FF
    DB 0x83, 0xC0, 0x0F ;$1 ADD AX, 0x000F
    MOV SP, AX
    PUSH BX
    PUSH CX
    CALL SP
    PUSH CS
    POP SS
    PUSH CS
    POP ES
    MOV CX, 0x5153
    MOV BX, 0xD4FF
    DB 0x83, 0xC0, 0x0F ;$1 ADD AX, 0x000F
    MOV SP, AX
    PUSH BX
    PUSH CX
    CALL SP
    PUSH CS
    POP SS
    PUSH CS
    POP ES
    INT3
    MOV CX, 0x5153
    MOV BX, 0xD4FF
    DB 0x83, 0xC0, 0x0F ;$1 ADD AX, 0x000F
    MOV SP, AX
    INT3
    PUSH CX
    CALL SP
    INT3
    PUSH CS
    POP SS
    PUSH CS
    POP ES
    INT3
    INT3
    MOV CX, 0x5153
    MOV BX, 0xD4FF
    DB 0x83, 0xC0, 0x0F ;$1 ADD AX, 0x000F
    MOV SP, AX
    PUSH BX
    PUSH CX
    CALL SP
    PUSH CS
    POP SS
    PUSH CS
    POP ES
    MOV CX, 0x5153
    MOV BX, 0xD4FF
    DB 0x83, 0xC0, 0x0F ;$1 ADD AX, 0x000F
    MOV SP, AX
    PUSH BX
    PUSH CX
    CALL SP
    PUSH CS
    POP SS
    PUSH CS
    POP ES
    INT3
    MOV CX, 0x5153
    MOV BX, 0xD4FF
    DB 0x83, 0xC0, 0x0F ;$1 ADD AX, 0x000F
    MOV SP, AX
    INT3
    PUSH CX
    CALL SP
    INT3