    JMP label1
    PUSH ES
    PUSH CS
    POP ES
    MOV SI, AX
    MOV BL, [0x2612]
    MOV AX, 0x24FF
    MOV DX, 0x24FF
    ADD BX, SI
    MOV CH, [BX+word 0x0055]
    ADD CX, 0x0140
    MOV DI, CX
    INT 0x86
    POP SS
    XOR CX, CX
    XOR DI, DI
    NOP
    NOP
    NOP
    MOV SP, 0x0200
    NOP
label1:
    PUSH ES
    PUSH CS
    POP ES
    MOV SI, AX
    MOV BL, [0x2612]
    MOV AX, 0x24FF
    MOV DX, 0x24FF
    ADD BX, SI
    MOV CH, [BX+word 0x0055]
    ADD CX, 0x0140
    MOV DI, CX
    INT 0x86
    POP SS
    XOR CX, CX
    XOR DI, DI
    NOP
    MOV SP, 0x0200
    NOP
    JMP WORD [BP+0x00]
    ADD DL, [BX+SI+0x66]
    MOV AH, 0xFA
    OR [BX+0x6D], BX
    MOV BL, 0xC1
    PUSH SS
    AND AL, 0x6A
    MOV AX, 0x1DCE
    AND SI, [BX+DI-0x79]
    AAD 0x1A
    SUB [BP-0x74], BH
label2:
    SHL AL, CL
    AAA
    INC BP
    DB 0x8B, 0xD9 ;$1 MOV BX, CX
    OUT DX, AX
label3:
    CMP AL, 0x42
    NOP
    CMPSB
    HLT
label4:
    CMP CX, [BX+DI-0x61]
    LODSW
    DB 0xF3, 0x00
    PUSH SI
    DB 0x64
    STOSB
    CLC
    DB 0x0F
    POP BP
    DB 0x63
    MOV CL, 0xC7
    ADC AL, 0x5A
    DB 0x68
    MOV SI, 0x13CC
    AND [BX-0x7B], SI
    RETF
    SBB [0x827C], CH
    SHL DH, 1
label5:
    XOR AX, 0x897B
    ESC 0xDF
    NOP
    XOR AL, [BX+SI-0x6A]
    MOVSB
    JMP 0xA39D:0x4F39
    DB 0xF1
    PUSH ES
    PUSH SP
    CALL 0x530D:0xFEA8
    DB 0x61
    MOV BH, 0xC5
    OR BL, [BX+SI+0x6E]
    MOV SP, 0x11C2
    DAA
    JNZ label3+1
    DB 0xC9
    PUSH DS
    SUB AL, 0x72
    ADC DH, 0xE4
    SUB DI, [BX+DI-0x70]
    ESC 0xDD
    JCXZ loop1
    INC SI
    XCHG SP, AX
    ESC 0xDA
    CALL 19980
    XCHG BX, AX
    MOV AX, [0x04F7]
label6:
    DEC DX
    CBW
    SCASB
    CLD
    ADD DX, [BX+DI+0x67]
    MOV CH, 0xFB
    OR [BP+0x6C], BL
    MOV DL, 0xC0
    POP SS
    AND AX, 0xB96B
    IRET
    SBB AL, 0x22
    JO label2+1
    AAM 0x1B
    NOP
    JG label4+1
    SHL CX, CL
    SS:
    INC SP
    DB 0x8A, 0xD8 ;$1 MOV BL, AL
    OUT DX, AL
    CMP AX, 0x9143
loop1:
    CMPSW
    CMC
    CMP CL, [BX+SI-0x62]
    LODSB
    DB 0xF2, 0x01
    PUSH DI
    DB 0x65
    STOSW
    STC
    PUSH CS
    POP SP
    DB 0x62
    MOV AL, 0xC6
    ADC AX, 0x695B
    MOV DI, 0x12CD
    AND [BP-0x7C], DH
    NOP
    SBB [BX], BP
    JGE label5+1
    SHL DI, 1
    XOR AL, 0x7A
    MOV DH, BL
    IN AL, DX
    XOR AX, [BX+DI-0x69]
    MOVSW
    JMP label7+2
    DEC SI
    PUSHF
    MOV [0x07F0], AL
    PUSH BP
    DB 0x9B, 0xA9  ; WAIT
    DEC WORD [SI]
    PUSH DX
    DB 0x60
    MOV DH, 0xC4
    OR BX, [BX+DI+0x6F]
    MOV BP, 0x10C3
    NOP
    MOV DX, 0x1FC8
    SUB AX, 0x8173
    XLATB
    IN AX, 0x2A
    JS label6
    ESC 0xDC
    LOOP loop2+1
    INC DI
    XCHG BP, AX
    ESC 0xDB
    JMP 19850
    XCHG DX, AX
    MOV AL, [0x05F6]
    DEC BX
    CWD
    SCASW
    STD
    PUSH ES
    PUSH CS
    POP ES
    MOV SI, AX
label7:
    MOV BL, [0x2612]
    NOP
    NOP
    MOV AX, 0x24FF
    MOV DX, 0x24FF
    ADD BX, SI
    MOV CH, [BX+word 0x0055]
    ADD CX, 0x0140
    MOV DI, CX
    INT 0x86
    POP SS
    XOR CX, CX
loop2:
    XOR DI, DI
    NOP
    MOV SP, 0x0200
    JMP WORD [BP+0x00]