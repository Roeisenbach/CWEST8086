    AND AX, 0x6564
    DB 0x66
    DB 0x69
    DB 0x6E
    DB 0x65
    AND [BP+DI+0x41], AL
    DEC SP
    DEC SP
    POP DI
    INC SI
    INC CX
    PUSH DX
    POP DI
    INC DX
    POP AX
    POP DI
    DEC DI
    PUSH AX
    INC BX
    DEC DI
    INC SP
    INC BP
    AND [BX+SI], DH
    JS label2
    INC SI
    INC SI
    INC SI
    OR CL, [BP+SI]
    OR AL, [BX+SI+0x73]
    JZ label5
    JB label8+1
    CMP CL, [BP+SI]
    DB 0x6D
    DB 0x6F
    JBE label2+1
    JNB label7+1
    SUB AL, 0x61
    JS label1
    DB 0x61
    DB 0x64
    DB 0x64
    AND [BP+DI+0x69], DH
    SUB AL, 0x28
    INC AX
    DB 0x63
label1:
    DB 0x6F
    JO label12+1
    SUB AX, 0x7340
    JZ label10+1
    JB label13+1
    SUB [BP+SI], CX
    OR CH, [DI+0x6F]
label2:
    JBE label3
    DB 0x63
    DB 0x6C
    SUB AL, 0x28
    INC AX
    DB 0x63
    DB 0x6F
    JO label18
    POP DI
    DB 0x65
    DB 0x6E
    DB 0x64
    AND [DI], CH
    AND [BX+SI+0x63], AL
    DB 0x6F
    JO label21+2
    SUB [BX], BP
    XOR [BX+SI+0x32], BH
    OR DH, [BP+SI+0x65]
    JO label6
    DB 0x6D
label3:
    DB 0x6F
    JBE label23
    JA label4+2
    OR DH, [BX+SI+0x75]
    JNB label22
    AND [DI+0x73], AH
label4:
    OR DH, [BX+SI+0x6F]
    JO label9
    DB 0x64
    JNB label6
    OR CH, [BX+DI+0x6E]
label5:
    DB 0x63
    AND [BX+DI+0x68], AH
    OR CH, [DI+0x6F]
label6:
    JBE label11
    DB 0x61
    DB 0x6C
    SUB AL, 0x30
    JS label19
    XOR CX, [BP+SI]
label7:
    OR CH, [DI+0x6F]
label8:
    JBE label14
    DB 0x62
    JS label16
    DB 0x64
label9:
    DB 0x69
    OR DH, [BP+DI+0x74]
    DB 0x6F
label10:
    JNB label28+1
    OR CH, [DI+0x6F]
    JBE label17
    POP BX
    DB 0x64
    DB 0x69
label11:
    POP BP
    SUB AL, 0x63
    JNB label15
    DB 0x6D
    DB 0x6F
    JBE label20
label12:
    JNB label29+1
label13:
    SUB AL, 0x5B
label14:
    DB 0x64
    DB 0x69
label15:
    POP BP
    OR CL, [BP+SI]
    DB 0x6D
    DB 0x6F
    JBE label23+1
    DB 0x61
    JS label24+1
    INC BX
    INC CX
    DEC SP
label16:
    DEC SP
label17:
    POP DI
    INC SI
    INC CX
    PUSH DX
label18:
    POP DI
    INC DX
    POP AX
    POP DI
    DEC DI
    PUSH AX
label19:
    INC BX
    DEC DI
label20:
    INC SP
    INC BP
label21:
    OR CH, [SI+0x65]
    JNB label25
    DB 0x64
label22:
    DB 0x69
    SUB AL, 0x5B
    DB 0x62
label23:
    JS label31
    OR BH, [BX+SI+0x6F]
    JB label26+1
    JNB label33+1
    SUB AL, 0x73
    DB 0x69
    OR CH, [DI+0x6F]
label24:
    JBE label27+1
    DB 0x63
    DB 0x6C
    SUB AL, 0x28
    INC AX
    DB 0x6C
    DB 0x6F
    DB 0x6F
    JO label34+1
label25:
    DB 0x65
    DB 0x6E
    DB 0x64
    SUB AX, 0x6C40
    DB 0x6F
    DB 0x6F
    JO label30
    DAS
label26:
    XOR [BX+SI+0x32], BH
    OR CH, [DI+0x6F]
    JBE label30
    JNB label39+1
label27:
    SUB AL, 0x5B
    DB 0x62
    JS label37
    OR DH, [BP+DI+0x74]
    DB 0x6F
label28:
    JNB label42+1
    OR AH, [SI+0x65]
    DB 0x63
    AND [SI+0x69], AH
    OR CL, [BP+SI]
    DB 0x63
    DB 0x61
    DB 0x6C
    DB 0x6C
label29:
    AND [BP+0x61], AH
    JB label32
    POP BX
label30:
    DB 0x62
    JS label41+2
    OR CL, [BP+SI]
    INC AX
    DB 0x63
    DB 0x6F
    JO 440
    CMP CL, [BP+SI]
    JB label44
    JO label35+1
    DB 0x6D
label31:
    DB 0x6F
    JBE 444
    JA label32+1
    INC AX
    DB 0x6C
    DB 0x6F
    DB 0x6F
    JO label40+1
    OR CH, [DI+0x6F]
label32:
    JBE label36+1
    DB 0x64
    DB 0x69
label33:
    SUB AL, 0x5B
    DB 0x62
    JS 442
    OR CH, [DI+0x6F]
label34:
    JBE label38+1
    DB 0x63
    DB 0x6C
label35:
    SUB AL, 0x28
    INC AX
    DB 0x63
    DB 0x6F
    JO 484
    POP DI
    DB 0x65
    DB 0x6E
    DB 0x64
    AND [DI], CH
    AND [BX+SI+0x63], AL
    DB 0x6F
label36:
    JO 496
    SUB [BX], BP
label37:
    XOR [BX+SI+0x32], BH
    OR BH, [BX+SI+0x6F]
    JB label43
label38:
    JNB 492
    SUB AL, 0x73
    DB 0x69
label39:
    OR DH, [BP+DI+0x74]
    DB 0x6F
label40:
    JNB 515
    OR AH, [SI+0x65]
    DB 0x63
    AND [SI+0x69], AH
label41:
    OR AH, [BP+DI+0x61]
    DB 0x6C
    DB 0x6C
label42:
    AND [BP+0x61], AH
    JB 445
    POP BX
    DB 0x62
    JS 510
label43:
    OR AL, [BX+SI+0x6C]
    DB 0x6F
    DB 0x6F
    JO 519
label44:
    DB 0x65
    DB 0x6E
    DB 0x64
    CMP CL, [BP+SI]
    INC AX
    DB 0x63
    DB 0x6F
    JO 555
    POP DI
    DB 0x65
    DB 0x6E
    DB 0x64
    DB 0x3A  ; CMP AL, [BX+SI]