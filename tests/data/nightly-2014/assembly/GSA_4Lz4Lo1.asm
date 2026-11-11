    JMP label1
    NOP
    XLATB
    XCHG AH, AL
    XLATB
    XLATB
    XCHG AH, AL
    XLATB
label1:
    XCHG BX, [0xE129]
    DB 0x8B, 0xF0 ;$1 MOV SI, AX
    DB 0x8B, 0xC3 ;$1 MOV AX, BX
    DIV WORD [BX+0x29]
    ADD DX, 0x0FF6
    MOV CL, 0x0D
    DB 0x8B, 0xEA ;$1 MOV BP, DX
    RCR BP, CL
    ADD BP, 0x0211
    LES AX, [BX+0x2B]
    MOV [BP+0x02], DX
    MOV DX, 0xD7E0
    LEA DI, [SI-0x0100]
    INT 0x86
    ADD DI, 0x0200
    ADD BX, 0x002F
    INT 0x86
    LEA DI, [BX+0x00D3]
    MOV [DI+0x14], DI
    MOV [DI+0x1B], DI
    DB 0x33, 0xFF ;$1 XOR DI, DI
    PUSH SS
    ADD SI, 0x00DE
    POP ES
    REP MOVSW
    LEA AX, [SI+0x5108]
    MOV AL, 0xA2
    MOV [SI-0x55], BL
    XCHG AX, [BP+0x00]
    MOV [SI-0x5B], BH
    PUSH BP
    MOV CL, 0x04
    LEA BX, [SI]
    DB 0x33, 0xF6 ;$1 XOR SI, SI
    MOV [0x8701], BP
    DB 0x8B, 0xEF ;$1 MOV BP, DI
    MOV [0x8801], BX
label2:
    MOV [0x4501], AL
    MOV [0x4701], AL
    MOV [0x4401], AL
    MOV [0x4101], AL
    MOV [0x4201], AL
    MOV [0x4301], AL
    XCHG SP, [DI-0x7F1A]
    XCHG DX, [DI-0x7D1A]
    XCHG SI, [DI-0x7B1A]
    XCHG AX, [DI-0x791A]
loop1:
    NOP
    XLATB
    XCHG AH, AL
    XLATB
    DB 0x32, 0xE0 ;$1 XOR AH, AL
    XCHG DI, AX
    MOV WORD [DI+0x6E], 0xFFCC
    MOV WORD [DI+0x6C], 0xCCB9
    ADD BYTE [BX-0x66], 0x02
    LOOP loop1
    MOV BYTE [BX-0x66], 0x90
    INC BP
    MOV CL, 0x04
    DB 0x8B, 0xFD ;$1 MOV DI, BP
    MOV SP, 0x07FE
    JP label2
    POP BX
    PUSH SS
    MOV CL, 0x08
    POP DS
    PUSH CS
    DB 0x33, 0xF6 ;$1 XOR SI, SI
    POP SS
    MOV AX, 0xCCCC
    PUSH CS
    MOV DX, 0x29CC
    POP ES
    INT 0x87
    LES DI, [BX+SI]
    DB 0x8B, 0xC3 ;$1 MOV AX, BX
    DEC DI
    LEA SP, [BX+DI]
    MOV BP, 0x0002
    MOV DX, 0x5200
    MOVSW
    MOVSW
    DB 0x2B, 0xFD ;$1 SUB DI, BP
    CALL FAR [BX]
    DB 0x66
    CALL FAR [BX]
    DB 0x68
    MOVSB
    MOVSW
    REP MOVSW
    MOV CL, 0x08
    ADD DI, [SI]
    LEA SP, [BX+DI]
    DB 0x33, 0xF6 ;$1 XOR SI, SI
    ADD [BX+SI], DX
    MOVSW
    MOVSW
loop2:
    DB 0x2B, 0xFD ;$1 SUB DI, BP
    CALL FAR [BX]
    JMP 0x882E:0x0051
    CMPSB
    SUB AX, [DI]
    MOV [0x938D], AX
    MOV BP, 0x351B
    MOV AX, 0x3096
    PUSH DS
loop3:
    DEC SI
    DB 0x60
label3:
    DB 0xC6, 0xE8, 0x65 ;$6 MOV AL, 0x65
    DEC BX
    IN AX, DX
    RET
    ESC 0xDD
    DB 0xF3, 0x55
    JNP label3+1
    ESC 0xD8
    JLE label5+2
    CLI
    AAM 0x72
    POP SP
    SAR DI, 1
    POP CX
    JA label8+1
    INC DI
    LOOPZ loop2+1
    INC DX
    DB 0x6C
    RETF 0xB4E4
    CALL 0xB19F:0x123C
    POP SS
    CMP [BX], SP
    OR [BX+0x0C81], BP
    AND AL, [SI+0x6AAA]
    INC SP
    LOOP loop3
    INC CX
    DB 0x6F
    DB 0xC9
    OUT 0xF9, AX
    XLATB
    JNO label9+2
    SAR AH, CL
    POP DX
    JZ label6
    OR CH, [SI+0x0F82]
    AND [BX-0x4857], AX
    CWD
    AAS
    ADC [SI+0x14B2], BX
    CMP DL, [BX+SI+0x18BE]
    SS:
    MOV BX, 0x3395
    SBB AX, 0x2D03
label4:
    MOV SP, [DI+0x0628]
label5:
    MOV AL, [0xDE8E]
    LOCK
    PUSH SI
    JS label4
label6:
    ESC 0xDB
    JGE label11
    DEC BP
    DB 0x63
    DB 0xC5, 0xEB
    DB 0x66
    DEC AX
    OUT DX, AL
    DB 0xC0
label7:
    SUB BYTE [BP+0x2608], 0xAB
    TEST [BP+DI], SP
    OR AX, 0x3D13
    DB 0x9B, 0xB5  ; WAIT
    CMP [0x9EB0], DL
    INTO
label8:
    LOOPNZ loop4
    DB 0x68
    IN AX, 0xCB
    DB 0x6D
    INC BX
    POP BP
    JNB label5+2
    STI
    JBE label12
    DB 0xFE, 0xD0
    JP label12
    DB 0xF2, 0xDC
    PUSH CX
    JG label7
    IMUL CX
label9:
    DB 0xC7, 0x61, 0x4F, 0xC2, 0xEC ;$4 MOV WORD [BX+DI+0x4F], 0xECC2
    DEC DX
    DB 0x64
    XOR AL, 0x1A
    MOV SP, 0x1F92
    XOR [BX-0x5847], DX
    MOV [BX], BP
    ADD [SI+0x04A2], CX
    DB 0x2A, 0xEA ;$1 SUB CH, DL
    LES SP, [BP+SI+0x4C]
    DB 0xC1
    OUT DX, AX
    DEC CX
    DB 0x67
label10:
    JNS 537
    DB 0xF1
label11:
    ESC 0xDF
    PUSH DX
    JL label9
    HLT
    MOVSB
    MOV CH, [SI]
    ADD CL, [BX+0x07A1]
    SUB [BX], SI
loop4:
    SBB [BX+0x1C91], DI
    XOR DL, [SI+0x10BA]
    DS:
    CBW
    MOV DH, 0x3B
    ADC AX, 0x9DB3
    SUB WORD [DI+0x250B], 0xFFA8
    XCHG AH, [BX+SI]
    PUSH CS
    POP SI
    JO label10+1
    CLC
    JNZ 585
label12:
    STD
    ROR BP, CL
    JCXZ 568
    DB 0x6B
    OUT 0xC8, AL
    DB 0x6E
    INC AX
    XLATB
    XCHG AH, AL
    XLATB
    XLATB
    XCHG AH, AL
    XLATB