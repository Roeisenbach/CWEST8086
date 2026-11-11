    MOV BX, [0xE129]
    MOV CX, [0xE129]
    MOV WORD [BX+0x29], 0x0000
    XCHG BX, CX
    MOV WORD [BX+0x29], 0x0000
    MOV WORD [BX+0x00B5], 0xCCCC
    XCHG BX, CX
    MOV WORD [BX+0x00B5], 0xCCCC
    JMP label1
    XCHG BX, CX
    MOV WORD [BX+0x29], 0x0000
    MOV WORD [BX+0x00B5], 0xCCCC
    XCHG BX, CX
    MOV WORD [BX+0x00B5], 0xCCCC
    PUSH AX
    MOV DX, AX
    MOV AX, 0xE33F
    MOV CX, 0x00AB
    LOOP $
    MOV CX, DX
    MUL WORD [0xBC0F]
    ADD AX, 0x2503
    XOR AX, 0xFAEB
    XCHG CX, AX
    MOV BX, AX
    ADD AX, 0x0158
    LEA BX, [BX+0x015B]
    MOV [BX], AX
    MOV AX, BX
    MOV BX, CX
    ADD BX, 0x004E
label1:
    MOV CX, 0x001E
    LOOP $
    PUSH AX
    PUSH DS
    PUSH ES
    POP DS
    MOV DL, [0x0052]
    MOV CL, 0x04
    SHL DX, CL
    ADD DL, 0x9C
    POP DS
    MOV BX, AX
    MOV DH, 0x02
    MOV [BX+0x0199], DX
    MOV BX, AX
    MOV DH, AH
    ADD DH, 0x41
    MOV DL, 0x1C
    MOV [BX+0x0175], DX
    MOV DX, AX
    MOV AX, 0xE33F
    MOV CX, 0x00A5
    LOOP $
    MOV CX, DX
    MUL WORD [0xBC0F]
    ADD AX, 0x2503
    XOR AX, 0xFAEB
    XCHG CX, AX
    MOV BX, AX
    ADD AX, 0x0158
    LEA BX, [BX+0x015B]
    MOV [BX], AX
    MOV AX, BX
    MOV BX, CX
    ADD BX, 0x004E
    MOV [BX], AX
    MOV WORD [BX-0x02], 0x26FF
    POP AX
    MOV CL, 0x08
    ADD AX, 0x0104
    MOV SI, SS
    MOV BX, SS
    AND BX, 0x0010
    LEA SI, [BX+SI+0x04]
    XCHG SI, AX
    MOV ES, AX
    MOV DI, 0x0066
    MOV CL, 0x2A
    MOV CH, 0x00
    REP MOVSW
    MOV DS, AX
    LEA DX, [SI+0x3EA8]
    MOV DL, 0x0D
    ADD DX, BP
    MOV DL, 0xFE
    MOV BX, DX
    ADD BX, 0x001F
    PUSH CS
    POP ES
    PUSH CS
    POP SS
    MOV SI, 0x0084
    MOV [0x00BA], AX
    MOV AX, 0xA5A5
    LEA DI, [BX-0x01]
    LEA SP, [BX+0x68]
    MOV BP, DI
    MOV CL, 0x16
    MOVSW
    JMP BP
    REP MOVSW
    MOV CX, 0x0404
    LEA SP, [BX+0x64]
    MOV BX, DI
label2:
    POP DI
    POP BP
    SHL BP, CL
    MOV [BP+DI-0x02], AX
    SUB SP, 0x0003
    DEC CH
    JNZ label2
    MOV DI, BX
    REP MOVSW
    MOV CL, 0x14
    REP MOVSW
    ADD DI, 0x003B
    MOVSW
    SUB DI, 0x0068
    MOV BX, DI
    MOVSW
    MOV CX, [SI+0x04]
    LDS SI, [SI]
    ADD DH, 0x40
    NOP
label3:
    POP DI
    POP BP
    SHL BP, CL
    MOV [BP+DI-0x02], AX
    ADD SP, [BX]
    MOV DI, [BX]
    CMP [BX+SI], DI
    JZ label3
    MOV DS, CX
    MOV DI, DX
    MOV CX, 0x000D
    MOVSW
    JMP DX
    ADD DX, [BX+0x03]
    PUSH DI
    DB 0x66
    ADD [BX+SI], AL
    ADC [0xB807], DL
    DB 0xFF, 0xFF
    MOV SI, AX
    ADD SI, 0x0037
    XOR DI, DI
    MOV CX, 0x001F
    REP MOVSW
    PUSH SS
    POP DS
    XOR SI, SI
    PUSH CS
    POP ES
    PUSH CS
    POP SS
    MOV DX, 0xA5F3
    MOV AX, 0xFFFF
    DB 0x83, 0xE8, 0x1E ;$1 SUB AX, 0x001E
    DB 0x83, 0xE8, 0x3E ;$1 SUB AX, 0x003E
    MOV SP, AX
    MOV DI, SP
    MOV AX, 0x218D
    MOV CX, 0x001E
    LOOP $
    MOV CX, 0x001F
    PUSH DX
    JMP SP
    LEA BP, [DI+0x0087]
    AND DI, 0xFF00
    ADD DI, 0x1111
    MOV BX, [BP+0x00]
    ADD SP, 0x4002
    MOV CX, 0x0300
    CMP WORD [SI], 0x5337
    JLE end
loop1:
    INC WORD [SI]
    XOR AX, 0xA37C
    STOSW
    ADD DI, 0x000E
    CMP [BP+0x00], BX
    JNZ label4
    LOOP loop1
    MOV [BP+0x00], CX
    MOV CX, 0x0032
    LOOP $
label4:
    XOR SI, SI
    MOV CX, 0x001F
    MOV DI, SP
    PUSH DX
    JMP SP
end: