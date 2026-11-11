    JMP label2+1
    POP AX
    JMP label1+1
    PUSH ES
    PUSH DS
    PUSH ES
    POP DS
    ADD AL, 0xA3
    ADD [BX+SI+0x3E90], DL
    MOV AX, [BX+SI]
    OR [BP+SI], CL
    MOV AX, [0x1600]
    MOV AX, [BX+SI]
    OR AL, 0x1E
    MOV AX, [BX+SI]
    PUSH CS
    PUSH CS
    MOV AX, [BX+SI]
    ADC [BX], AL
label1:
    XCHG CX, BP
    POP ES
    ADC AH, [BX+DI+0x1600]
    MOV AX, [BX+SI]
    ADC AL, 0x3E
    MOV AX, [BX+SI]
    PUSH SS
    PUSH ES
    PUSH CS
    POP ES
    XCHG CL, CH
    SBB [BX+DI+0x1600], AH
    MOV AX, [BX+SI]
    SBB BH, [0x008B]
    SBB AL, 0x86
    INT 0x07
    PUSH DS
    MOV AX, [BX+SI]
    PUSH DS
    DS:
    MOV AX, [BX+SI]
    AND [BX], BL
    SBB AX, 0x0689
    POP DS
    PUSH CS
    POP SS
    ES:
    MOV AX, [BX+SI]
    DB 0x22, 0xFF ;$1 AND BH, BH
    XOR [DI], BX
    CALL FAR [BX+SI-0x15]
    PUSH DS
    PUSH ES
    PUSH DS
    PUSH ES
    POP DS
    ADD AL, 0xA3
    ADD [BX+SI+0x3E90], DL
    MOV AX, [BX+SI]
    OR [BP+SI], CL
    MOV AX, [0x1600]
    MOV AX, [BX+SI]
    OR AL, 0x1E
    MOV AX, [BX+SI]
    PUSH CS
    PUSH CS
    MOV AX, [BX+SI]
    ADC [BX], AL
    XCHG CX, BP
    POP ES
    ADC AH, [BX+DI+0x1600]
    MOV AX, [BX+SI]
    ADC AL, 0x3E
    MOV AX, [BX+SI]
    PUSH SS
    PUSH ES
    PUSH CS
    POP ES
    XCHG CL, CH
    SBB [BX+DI+0x1600], AH
    MOV AX, [BX+SI]
    SBB BH, [0x008B]
    SBB AL, 0x86
    INT 0x07
    PUSH DS
    MOV AX, [BX+SI]
    PUSH DS
    DS:
    MOV AX, [BX+SI]
    AND [BX], BL
    SBB AX, 0x0689
    POP DS
    PUSH CS
    POP SS
    ES:
    MOV AX, [BX+SI]
    DB 0x22, 0xFF ;$1 AND BH, BH
    XOR [DI], BX
label2:
    CALL FAR [0x1E06]
    PUSH ES
    POP DS
    MOV [0x0004], AX
    NOP
    NOP
    MOV DI, [0x0008]
    MOV AX, [0x000A]
    MOV DX, [0x000C]
    MOV BX, [0x000E]
    MOV CX, [0x0010]
    MOV SI, [0x0006]
    CMP SI, 0x00DF
    JNZ label3
    STD
label3:
    POP ES
    INT 0x87
    POP ES
    MOV AX, [0x0012]
    MOV DX, [0x0014]
    MOV DI, [0x0016]
    PUSH ES
    PUSH CS
    POP ES
    INT 0x86
    MOV AX, [0x0018]
    MOV DX, [0x001A]
    MOV DI, [0x001C]
    INT 0x86
    POP ES
    MOV BX, [0x001E]
    MOV DI, [0x0020]
    POP DS
    MOV [DI], BX
    PUSH ES
    POP DS
    PUSH CS
    POP SS
    MOV SP, [0x0022]
    XOR DI, DI
    CALL FAR [DI]
    POP AX
    JMP label4+1
    PUSH ES
    PUSH DS
    PUSH ES
    POP DS
    ADD AL, 0xA3
    ADD [BX+SI+0x3E90], DL
    MOV AX, [BX+SI]
    OR [BP+SI], CL
    MOV AX, [0x1600]
    MOV AX, [BX+SI]
    OR AL, 0x1E
    MOV AX, [BX+SI]
    PUSH CS
    PUSH CS
    MOV AX, [BX+SI]
    ADC [BX], AL
label4:
    XCHG CX, BP
    POP ES
    ADC AH, [BX+DI+0x1600]
    MOV AX, [BX+SI]
    ADC AL, 0x3E
    MOV AX, [BX+SI]
    PUSH SS
    PUSH ES
    PUSH CS
    POP ES
    XCHG CL, CH
    SBB [BX+DI+0x1600], AH
    MOV AX, [BX+SI]
    SBB BH, [0x008B]
    SBB AL, 0x86
    INT 0x07
    PUSH DS
    MOV AX, [BX+SI]
    PUSH DS
    DS:
    MOV AX, [BX+SI]
    AND [BX], BL
    SBB AX, 0x0689
    POP DS
    PUSH CS
    POP SS
    ES:
    MOV AX, [BX+SI]
    DB 0x22, 0xFF ;$1 AND BH, BH
    XOR [DI], BX
    CALL FAR [0x1E06]
    PUSH ES
    POP DS
    ADD AL, 0xA3
    ADD [BX+SI+0x3E90], DL
    MOV AX, [BX+SI]
    OR [BP+SI], CL
    MOV AX, [0x1600]
    MOV AX, [BX+SI]
    OR AL, 0x1E
    MOV AX, [BX+SI]
    PUSH CS
    PUSH CS
    MOV AX, [BX+SI]
    ADC [BX], AL
    XCHG CX, BP
    POP ES
    ADC AH, [BX+DI+0x1600]
    MOV AX, [BX+SI]
    ADC AL, 0x3E
    MOV AX, [BX+SI]
    PUSH SS
    PUSH ES
    PUSH CS
    POP ES
    XCHG CL, CH
    SBB [BX+DI+0x1600], AH
    MOV AX, [BX+SI]
    SBB BH, [0x008B]
    SBB AL, 0x86
    INT 0x07
    PUSH DS
    MOV AX, [BX+SI]
    PUSH DS
    DS:
    MOV AX, [BX+SI]
    AND [BX], BL
    SBB AX, 0x0689
    POP DS
    PUSH CS
    POP SS
    ES:
    MOV AX, [BX+SI]
    DB 0x22, 0xFF ;$1 AND BH, BH
    XOR [DI], BX
    DB 0xFF  ; INC WORD [BX+SI]