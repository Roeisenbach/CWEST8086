    JMP label1
    MOV CX, 0x000E
    LOOP $
    PUSH ES
    POP DS
    NOP
    MOV SI, 0x0012
    LODSW
    ADD AX, 0x8800
    XOR SI, SI
    XOR DI, DI
    PUSH CS
    POP ES
    MOV BX, 0x0058
    MOV [BX+0x02], CS
    MOV [BX], AX
    MOV WORD [BX+0x10], 0x1E00
    MOV CX, 0x0064
    LOOP $
    PUSH AX
    XOR DI, DI
    MOV AX, 0x1FFF
    MOV DX, 0xCCCC
    MOV BX, 0xCCCC
    MOV CX, BX
    INT 0x87
    POP AX
    INC AX
    MOV DI, AX
    MOV CX, 0x0007
    DEC AX
    MOV DX, CS
    PUSH DS
    MOV DS, DX
    MOV DX, 0x0008
    MOV BX, AX
    MOV WORD [BX], 0x18FF
    MOV BX, 0x0058
    POP DS
    PUSH CS
    POP SS
    ADD AX, 0x0500
    MOV SP, AX
    MOV AX, 0x1900
    CALL FAR [BX+SI]
    INT3
    INT3
    INT3
    JMP label1
    MOV CX, 0x000E
    LOOP $
    PUSH ES
    POP DS
    NOP
    MOV SI, 0x0012
    LODSW
    ADD AX, 0x8800
    XOR SI, SI
    XOR DI, DI
    PUSH CS
    POP ES
    MOV BX, 0x0058
    MOV [BX+0x02], CS
    MOV [BX], AX
    MOV WORD [BX+0x10], 0x1E00
    MOV CX, 0x0064
    LOOP $
    PUSH AX
    XOR DI, DI
    MOV AX, 0x1FFF
    MOV DX, 0xCCCC
    MOV BX, 0xCCCC
    MOV CX, BX
    INT 0x87
    POP AX
    INC AX
    MOV DI, AX
    MOV CX, 0x0007
    DEC AX
    MOV DX, CS
    PUSH DS
    MOV DS, DX
    MOV DX, 0x0008
    MOV BX, AX
    MOV WORD [BX], 0x18FF
    MOV BX, 0x0058
    POP DS
    PUSH CS
    POP SS
    ADD AX, 0x0500
    MOV SP, AX
    MOV AX, 0x1900
    CALL FAR [BX+SI]
    INT3
    INT3
    INT3
    BRK
label1:
    MOV CX, 0x000E
    LOOP $
    PUSH ES
    POP DS
    NOP
    MOV SI, 0x0012
    LODSW
    ADD AX, 0x8800
    XOR SI, SI
    XOR DI, DI
    PUSH CS
    POP ES
    MOV BX, 0x0058
    MOV [BX+0x02], CS
    MOV [BX], AX
    MOV WORD [BX+0x10], 0x1E00
    MOV CX, 0x0064
    LOOP $
    PUSH AX
    XOR DI, DI
    MOV AX, 0x1FFF
    MOV DX, 0xCCCC
    MOV BX, 0xCCCC
    MOV CX, BX
    INT 0x87
    POP AX
    INC AX
    MOV DI, AX
    MOV CX, 0x0007
    DEC AX
    MOV DX, CS
    PUSH DS
    MOV DS, DX
    MOV DX, 0x0008
    MOV BX, AX
    MOV WORD [BX], 0x18FF
    MOV BX, 0x0058
    POP DS
    PUSH CS
    POP SS
    ADD AX, 0x0500
    MOV SP, AX
    MOV AX, 0x1900
    CALL FAR [BX+SI]
    INT3
    INT3
    INT3
    JMP label1
    MOV CX, 0x000E
    LOOP $
    PUSH ES
    POP DS
    NOP
    MOV SI, 0x0012
    LODSW
    ADD AX, 0x8800
    XOR SI, SI
    XOR DI, DI
    PUSH CS
    POP ES
    MOV BX, 0x0058
    MOV [BX+0x02], CS
    MOV [BX], AX
    MOV WORD [BX+0x10], 0x1E00
    MOV CX, 0x0064
    LOOP $
    PUSH AX
    XOR DI, DI
    MOV AX, 0x1FFF
    MOV DX, 0xCCCC
    MOV BX, 0xCCCC
    MOV CX, BX
    INT 0x87
    POP AX
    INC AX
    MOV DI, AX
    MOV CX, 0x0007
    DEC AX
    MOV DX, CS
    PUSH DS
    MOV DS, DX
    MOV DX, 0x0008
    MOV BX, AX
    MOV WORD [BX], 0x18FF
    MOV BX, 0x0058
    POP DS
    PUSH CS
    POP SS
    ADD AX, 0x0500
    MOV SP, AX
    MOV AX, 0x1900
    CALL FAR [BX+SI]
    INT3
    INT3
    INT3
    JMP label1
    MOV CX, 0x000E
    LOOP $
    PUSH ES
    POP DS
    NOP
    MOV SI, 0x0012
    LODSW
    ADD AX, 0x8800
    XOR SI, SI
    XOR DI, DI
    PUSH CS
    POP ES
    MOV BX, 0x0058
    MOV [BX+0x02], CS
    MOV [BX], AX
    MOV WORD [BX+0x10], 0x1E00
    MOV CX, 0x0064
    LOOP $
    PUSH AX
    XOR DI, DI
    MOV AX, 0x1FFF
    MOV DX, 0xCCCC
    MOV BX, 0xCCCC
    MOV CX, BX
    INT 0x87
    POP AX
    INC AX
    MOV DI, AX
    MOV CX, 0x0007
    DEC AX
    MOV DX, CS
    PUSH DS
    MOV DS, DX
    MOV DX, 0x0008
    MOV BX, AX
    MOV WORD [BX], 0x18FF
    MOV BX, 0x0058
    POP DS
    PUSH CS
    POP SS
    ADD AX, 0x0500
    MOV SP, AX
    MOV AX, 0x1900
    CALL FAR [BX+SI]