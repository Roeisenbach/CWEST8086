    MOV [0xE129], AX
    JMP label2
    NOP
    XLATB
    XCHG AH, AL
    XLATB
    NOP
    XLATB
    XCHG AH, AL
    XLATB
    NOP
    XLATB
    XCHG AH, AL
    XLATB
    XLATB
    XCHG AH, AL
    XLATB
    XLATB
    XCHG AH, AL
    XLATB
    XLATB
    XCHG AH, AL
    XLATB
    XLATB
    XCHG AH, AL
    XLATB
    XLATB
    XCHG AH, AL
    XLATB
    DB 0x0F
    ADD BH, DL
    XCHG AL, [BX+SI]
    ADC [BP+DI-0x1DC2], CL
    CLC
    INT 0x86
    MOV DI, [0x8346]
    INT 0x86
    MOV AX, 0x86D7
    XCHG CX, SI
    STD
    MOV DX, 0xD7C4
    INT 0x87
    CLD
    LEA AX, [SI-0x2B]
    DB 0x33, 0xD2 ;$1 XOR DX, DX
    DIV WORD [SI-0x06]
    ADD DX, 0x0FF6
    ADD SI, 0x0191
label1:
    XCHG CH, [SI]
    CMP CH, 0x01
    JNZ label1
    XCHG CH, [SI]
    INC SI
    DB 0x8B, 0xEA ;$1 MOV BP, DX
    MOV CL, 0x0D
    PUSH SS
    CLC
    RCR BP, CL
    POP ES
    DB 0x33, 0xFF ;$1 XOR DI, DI
    REP MOVSW
    MOVSW
    LEA BX, [BP+0x0211]
    ADD BYTE [SI-0x0162], 0x04
    LEA AX, [SI-0x3BC0]
    PUSH SS
    MOV AL, 0xA2
    POP DS
    MOV [BX], AX
    PUSH CS
    MOV [BX+0x02], DX
    POP ES
    PUSH CS
    DB 0x8B, 0xC3 ;$1 MOV AX, BX
    POP SS
    MOV DX, 0xC600
    MOV CL, 0x09
    DB 0x33, 0xF6 ;$1 XOR SI, SI
    LES DI, [BX+SI]
    MOV BP, 0x8080
    DEC DI
    LEA SP, [BX+DI]
    MOV WORD [BX+0x1A], 0xC810
    MOVSW
    MOVSW
    MOV BYTE [BX+SI], 0x02
    SUB DI, [BX+SI]
    CALL FAR [BX]
label2:
    ADD AX, 0x018A
    MOV BX, SS
    OR BX, 0x0010
    ADD BX, 0x0014
    XCHG SI, AX
    PUSH BX
    MOV DI, 0x0084
    PUSH SI
    LEA DX, [SI-0x41C0]
    MOV ES, BX
    MOV CL, 0x1B
    MOV DL, 0xFA
    REP MOVSW
    MOV CL, 0x04
    DB 0x8B, 0xEF ;$1 MOV BP, DI
    LEA BX, [SI-0x0191]
    MOV [SI-0x00B3], BH
    MOV [SI-0x00AD], BL
    DB 0x33, 0xF6 ;$1 XOR SI, SI
    ADD DX, [0x8701]
    PUSH DX
    XCHG BX, [0x8801]
label3:
    MOV [0xC501], AL
    MOV [0xC701], AL
    MOV [0xC401], AL
    MOV [0xC101], AL
    MOV [0xC201], AL
    MOV [0xC301], AL
    XCHG SP, [DI-0x7EBA]
    XCHG DX, [DI-0x7CBA]
    XCHG SI, [DI-0x7ABA]
    XCHG AX, [DI-0x78BA]
loop1:
    NOP
    XLATB
    XCHG AH, AL
    XLATB
    DB 0x32, 0xE0 ;$1 XOR AH, AL
    XCHG DI, AX
    MOV WORD [DI+0x6E], 0xFFCC
    MOV WORD [DI+0x6C], 0xCCB9
    ADD BYTE [0xCCCC], 0x02
    LOOP loop1
    MOV BYTE [0xCCCC], 0x90
    INC BP
    MOV CL, 0x04
    DB 0x8B, 0xFD ;$1 MOV DI, BP
    MOV SP, 0x07FA
    JP label3
    POP BX
    MOV CL, 0x10
    POP SI
    MOV DI, 0x0064
    PUSH CS
    ADD SI, 0xFFE0
    REP MOVSW
    POP ES
    MOV DI, [0x0000]
    POP DS
    INT 0x86
    DB 0x33, 0xFF ;$1 XOR DI, DI
    INT 0x86
    ADD BX, 0x0025
    MOV SI, 0x0088
    MOV [SI+0x32], DS
    PUSH CS
    MOV CL, 0x14
    MOV AX, 0xCCCC
    POP SS
    MOV DX, 0x29CC
    LEA SP, [BX+0x66]
    INT 0x87
    MOV AX, 0xA593
    LEA DX, [BX-0x25]
    LEA DI, [BX-0x01]
    DB 0x8B, 0xEF ;$1 MOV BP, DI
    MOVSW
    JMP BP
    CWD
    MOVSW
    MOVSW
    MOV CX, 0x000D
    MOVSW
    REP MOVSW
    LEA SP, [BX+0x61]
    MOV CX, 0x0504
    DB 0x8B, 0xDF ;$1 MOV BX, DI
label4:
    POP DI
    POP BP
    SHL BP, CL
    MOV [BP+DI-0x02], AX
    ADD SP, 0xFFFD
    DEC CH
    JNZ label4
    MOV DI, BX
    REP MOVSW
    MOV CL, 0x12
    REP MOVSW
    ADD DI, 0x003B
    MOVSW
    SUB DI, 0x0066
    DB 0x8B, 0xDF ;$1 MOV BX, DI
    MOVSW
    MOV CX, [SI+0x04]
    LDS SI, [SI]
    ADD DH, 0xC0
label5:
    POP DI
    POP BP
    ADD SP, [BX]
    SHL BP, CL
    MOV [BP+DI-0x02], AX
    MOV BP, [BX]
    CMP [BX+SI], BP
    JZ label5
    MOV DS, CX
    DB 0x8B, 0xFA ;$1 MOV DI, DX
    MOVSW
    JMP DX
    IN AX, DX
    ADD CH, CH
    ADD [SI+byte 0x00], AH
    ADD [BX+SI], DL
    ADD [BP-0x01], SP
    POP DS
    DB 0x68
    MOVSB
    MOVSW
    REP MOVSW
    MOV CL, 0x09
    ADD DI, [SI]
    ADD SP, [BX+SI]
    DB 0x33, 0xF6 ;$1 XOR SI, SI
    ADD [BX+SI], DX
    MOVSW
    MOVSW
    SUB DI, [BX+SI]
    DEC BP
label6:
    JS label6+1
    POP DS
    CALL -9821
    XCHG AH, AL
    XLATB
    XLATB
    XCHG AH, AL
    XLATB
    XLATB
    XCHG AH, AL
    XLATB
    XLATB
    XCHG AH, AL
    XLATB
    XLATB
    XCHG AH, AL
    XLATB
    XLATB
    XCHG AH, AL
    XLATB
    XLATB
    XCHG AH, AL
    XLATB
    XLATB
    XCHG AH, AL
    XLATB