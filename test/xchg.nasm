bits 16
org 0x7c00

xor ax, ax
mov bx, 0x7c00
mov cx, 0xdbdb
xchg cx, bx
xchg cx, ax
xchg cx, [data]
xchg dx, [data]
hlt

data:
dw 0x4848

times 510 - ($ - $$) db 0x00
db 0x55
db 0xaa
