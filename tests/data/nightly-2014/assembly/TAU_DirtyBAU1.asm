    JMP label1
    MOV AX, 0x01F9
    MOV BX, 0x01F9
    MOV CL, 0x0C
    SHR AX, CL
    DB 0x83, 0xE0, 0x0C ;$1 AND AX, 0x000C
    MOV DX, AX
    MOV AX, BX
    ADD DX, 0x0FF8
    MOV CH, BH
    ADD CH, 0x50
    MOV CL, 0xA2
    MOV [BX+0x0168], DX
    MOV [BX+0x016B], CX
    PUSH DS
    PUSH SS
    POP DS
    MOV BX, 0x0200
    MOV [BX], CX
    MOV [BX+0x02], DX
    MOV DI, 0x0050
    MOV AX, CX
    STOSW
    MOV AX, DX
    STOSW
    PUSH SI
    PUSH CX
    PUSH DS
    PUSH DS
    PUSH CS
    POP DS
    POP ES
    LEA SI, [BP+0x0152]
    XOR DI, DI
    MOV CX, 0x0009
    REP MOVSW
    POP DS
    POP CX
    POP SI
    POP AX
    POP CX
    PUSH AX
    PUSH BX
    PUSH DX
    PUSH DS
    MOV BX, CS
    MOV DS, BX
    MOV DX, CX
    MOV AX, 0xE33F
    MOV CX, 0x007D
    LOOP $
    MOV CX, DX
    MUL WORD [0xB60F]
    ADD AX, 0x2503
    XOR AX, 0xFAEB
    XCHG CX, AX
    MOV BX, AX
    ADD AX, 0x0164
    LEA BX, [BX+0x0165]
    MOV [BX], AX
    MOV AX, BX
    MOV BX, CX
    ADD BX, 0x004E
    MOV [BX], AX
    MOV WORD [BX-0x02], 0x26FF
    POP DS
    POP DX
    POP BX
    PUSH DX
    POP ES
    POP SS
    MOV DX, 0x7300
    MOV AX, 0x1FFF
    LES DI, [BX]
    STOSW
    DEC DI
    LEA SP, [BX+DI-0x01]
    MOV CX, 0x0007
    CALL FAR [BX]
    MOVSW
    REP MOVSW
    MOV CL, 0x07
    ADD [BX], DX
    LES DI, [BX]
    LEA SP, [BX+DI]
    XOR SI, SI
    STOSW
    DEC DI
    CALL FAR [BX]
    INT3
label1:
    MOV BP, AX
    PUSH AX
    MOV BX, AX
    MOV CL, 0x0C
    SHR AX, CL
    DB 0x83, 0xE0, 0x0C ;$1 AND AX, 0x000C
    MOV DX, AX
    MOV AX, BX
    ADD DX, 0x0FF8
    MOV CH, BH
    ADD CH, 0x50
    MOV CL, 0xA2
    MOV [BX+0x0168], DX
    MOV [BX+0x016B], CX
    PUSH DS
    PUSH SS
    POP DS
    MOV BX, 0x0200
    MOV [BX], CX
    MOV [BX+0x02], DX
    MOV DI, 0x0050
    MOV AX, CX
    STOSW
    MOV AX, DX
    STOSW
    PUSH SI
    PUSH CX
    PUSH DS
    PUSH DS
    PUSH CS
    POP DS
    POP ES
    LEA SI, [BP+0x0152]
    XOR DI, DI
    MOV CX, 0x0009
    REP MOVSW
    POP DS
    POP CX
    POP SI
    POP AX
    POP CX
    PUSH AX
    PUSH BX
    PUSH DX
    PUSH DS
    MOV BX, CS
    MOV DS, BX
    MOV DX, CX
    MOV AX, 0xE33F
    MOV CX, 0x007D
    LOOP $
    MOV CX, DX
    MUL WORD [0xB60F]
    ADD AX, 0x2503
    XOR AX, 0xFAEB
    XCHG CX, AX
    MOV BX, AX
    ADD AX, 0x0164
    LEA BX, [BX+0x0165]
    MOV [BX], AX
    MOV AX, BX
    MOV BX, CX
    ADD BX, 0x004E
    MOV [BX], AX
    MOV WORD [BX-0x02], 0x26FF
    POP DS
    POP DX
    POP BX
    PUSH DX
    POP ES
    POP SS
    MOV DX, 0x7300
    MOV AX, 0x1FFF
    LES DI, [BX]
    STOSW
    DEC DI
    LEA SP, [BX+DI-0x01]
    MOV CX, 0x0007
    CALL FAR [BX]
    MOVSW
    REP MOVSW
    MOV CL, 0x07
    ADD [BX], DX
    LES DI, [BX]
    LEA SP, [BX+DI]
    XOR SI, SI
    STOSW
    DEC DI
    CALL FAR [BX]
    INT3
    MOV AX, 0xFFFF
    MOV DX, 0xFFFF
    MOV CX, 0xFFFF
    ADD CX, 0x3000
    PUSH DS
    PUSH SS
    POP DS
    MOV BX, 0x01E4
    MOV [BX], CX
    MOV [BX+0x02], DX
    MOV WORD [0x0000], 0xF3A5
    MOV WORD [0x0002], 0xB1A5
    MOV WORD [0x0004], 0x010B
    MOV WORD [0x0006], 0xC417
    MOV WORD [0x0008], 0x8D3F
    MOV WORD [0x000A], 0xFF21
    MOV WORD [0x000C], 0x8104
    MOV WORD [0x000E], 0x303C
    MOV WORD [0x0010], 0x7405
    MOV WORD [0x0012], 0x3106
    MOV WORD [0x0014], 0xABF6
    MOV WORD [0x0016], 0xFF4F
    MOV WORD [0x0018], 0xCC1F
    PUSH DX
    POP ES
    POP SS
    MOV DX, 0x7300
    MOV AX, 0x1FFF
    LES DI, [BX]
    STOSW
    DEC DI
    LEA SP, [BX+DI-0x01]
    MOV CX, 0x000B
    CALL FAR [BX]
    MOVSW
    REP MOVSW
    MOV CL, 0x0B
    ADD [BX], DX
    LES DI, [BX]
    LEA SP, [BX+DI]
    INC WORD [SI]
    CMP WORD [SI], 0x0520
    JZ label2
    XOR SI, SI
    STOSW
    DEC DI
    CALL FAR [BX]
label2:
    INT3