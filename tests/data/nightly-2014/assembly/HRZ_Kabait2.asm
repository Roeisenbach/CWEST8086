    XCHG SI, [0xE129]
    XCHG DI, [0xE129]
    JMP label2
    INT3
    INT3
    INT3
    STOSW
    INT3
    INT3
    INT3
    STOSW
    PUSH DX
    MOVSW
    INT3
    INT3
    INT3
    CALL FAR [BX]
    INT3
    INT3
    INT3
    INT3
    CALL FAR [BX]
    INT3
    INT3
    CMP DI, 0xCCCC
    JZ label1
    MOV DI, SI
label1:
    PUSH CS
    POP ES
    MOV DX, 0xCCCC
    INT 0x86
    PUSH AX
    MOV CL, 0xCC
    MOV AX, 0x5153
    MOV DX, 0xD4FF
    INT 0x87
    POP AX
    XOR DI, DI
    MOV SI, 0xA5AB
    MOV DX, 0xD401
    ADD AX, 0x00E6
    XCHG SI, AX
    PUSH SS
    POP ES
    MOVSW
    MOVSW
    MOVSW
    MOVSW
    INT 0x86
    SUB DI, 0x00B8
    MOV CL, 0x47
    MOV BP, 0x6465
    XOR [BP-0x6465], BP
    XOR [BP-0x6461], BP
loop1:
    MOVSW
    MOVSW
    XOR [BP+DI-0x6469], BP
    LOOP loop1
    XCHG BP, BX
    PUSH DS
    PUSH ES
    PUSH DS
    POP ES
    POP DS
    POP SS
    MOV BP, 0xEC4E
    LEA SP, [BP-0x0094]
    MOV WORD [0x03FE], 0xAAAA
    MOV DI, BP
    MOV AX, 0xA552
    MOV SI, 0x0008
    MOV DX, 0x3C88
    PUSH DX
    MOV [BP+0x02CA], DX
    MOVSB
    JMP BP
label2:
    CMP DI, 0xCCCC
    JZ label3
    CMP DI, 0x0000
    JNZ label4
label3:
    MOV DI, SI
label4:
    PUSH ES
    PUSH CS
    POP ES
    MOV DX, 0xCCCC
    INT 0x86
    XOR DI, DI
    MOV SI, 0xA5AB
    MOV DX, 0xD401
    ADD AX, 0x0111
    XCHG SI, AX
    PUSH SS
    POP ES
    MOVSW
    MOVSW
    MOVSW
    MOVSW
    INT 0x86
    SUB DI, 0x00B8
    MOV CL, 0x47
    MOV BP, 0x6465
    XOR [BP-0x6465], BP
    XOR [BP-0x6461], BP
loop2:
    MOVSW
    MOVSW
    XOR [BP+DI-0x6469], BP
    LOOP loop2
    PUSH AX
    PUSH CX
    PUSH DX
    PUSH ES
    PUSH CS
    POP ES
    MOV CL, 0xCC
    MOV AX, 0xCCCC
    MOV DX, 0xFFCC
    INT 0x87
    POP ES
    POP DX
    POP CX
    POP AX
    POP AX
    XCHG BP, BX
    PUSH DS
    PUSH ES
    PUSH DS
    POP ES
    POP DS
    POP SS
    MOV BP, 0x564E
    LEA SP, [BP-0x0094]
    MOV WORD [0x03FE], 0xAAAA
    MOV DI, BP
    MOV AX, 0xA552
    MOV SI, 0x0008
    MOV DX, 0x3C88
    PUSH DX
    MOV [BP+0x02CA], DX
    MOVSB
    JMP BP
    DB 0xC0
    DB 0xC1
    MOVSW
    MOV [BP+SI-word 0x0040], CX
    IN AX, 0xC0
    DB 0xC1
    MOV CL, 0x06
    DB 0xC0
    DB 0xC1
    REP MOVSW
    OUT DX, AL
    CLI
    RETF 0x5C02
    CLI
    DB 0x6A
    CALL WORD [BX+SI]
    DB 0x66
    XOR SI, SI
    DB 0xC0
    DB 0xC1
    MOV CL, 0x0D
    DB 0xC0
    XCHG DI, AX
    MOVSW
    CMP [BP-0x6117], SI
    DB 0xC7, 0x67, 0x11, 0x08, 0x81 ;$4 MOV WORD [BX+0x11], 0x8108
    MOV [SI+0x0A], AH
    OR WORD [BP+0x0304], 0xA081
    DB 0x64
    ADD AX, 0x8A89
    DB 0xC1
    JMP BP
    ADD CX, AX
    MOV CL, 0x0C
    DB 0xC0
    XCHG DI, AX
    MOVSW
    MOV CX, 0x6161
loop3:
    XCHG BX, SP
    IN AL, DX
    MOV SS, [BP+0x385E]
    MOV BH, 0xE5
    MOV [BX], DI
    CWD
    SUB SP, 0x0066
    CALL 0xF297:0x75CD
    MOVSW
    MOVSW
    AAM 0x62
    MOVSW
    DB 0xF3, 0xC0
    JCXZ loop3
    AND WORD [BP+0x00BA], 0xEDB8
    POP AX
    MOV BP, DX
    DB 0xC0
    DB 0xC1
    MOV CL, 0x05
    DB 0xC0
    DB 0xC1
    REP MOVSW
    AAM 0x5E
    XCHG BX, DI
    CBW
    XCHG DI, AX
    STOSW
    MOV [BP+SI-0x5A68], DI
    MOVSW
    AAM 0x63
    MOVSW
    MOVSW
    XCHG SI, AX
    DB 0xC1
    MOV [BP-0x64F1], AX
    MOV [BP+0x66AF], AX
    XCHG DX, AX
    MOV AX, 0xC137
    XOR SI, SI
    DB 0xC0
    CMP DI, 0xCCCC
    JZ label5
    MOV DI, SI
label5:
    PUSH CS
    POP ES
    MOV DX, 0xCCCC
    INT 0x86
    PUSH AX
    MOV CL, 0xCC
    MOV AX, 0x5153
    MOV DX, 0xD4FF
    INT 0x87
    POP AX
    XOR DI, DI
    MOV SI, 0xA5AB
    MOV DX, 0xD401
    ADD AX, 0x00E6
    XCHG SI, AX
    PUSH SS
    POP ES
    MOVSW
    MOVSW
    MOVSW
    MOVSW
    INT 0x86
    SUB DI, 0x00B8
    MOV CL, 0x47
    MOV BP, 0x6465
    XOR [BP-0x6465], BP
    XOR [BP-0x6461], BP
loop4:
    MOVSW
    MOVSW
    XOR [BP+DI-0x6469], BP
    LOOP loop4
    XCHG BP, BX
    PUSH DS
    PUSH ES
    PUSH DS
    POP ES
    POP DS
    POP SS
    MOV BP, 0xEC4E
    LEA SP, [BP-0x0094]
    DB 0xC7, 0x06, 0xFE  ; MOV WORD [0x00FE], 0x0000